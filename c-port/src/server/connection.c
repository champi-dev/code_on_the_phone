#include "cloudterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/* Connection ID counter */
static _Atomic uint64_t next_conn_id = 1;

/* Create new connection */
ct_connection_t *ct_connection_create(ct_server_t *server, int fd) {
    /* Allocate from pool - O(1) */
    ct_connection_t *conn = ct_mem_pool_alloc(server->conn_pool);
    if (!conn) return NULL;
    
    /* Initialize connection */
    memset(conn, 0, sizeof(ct_connection_t));
    conn->fd = fd;
    conn->id = atomic_fetch_add(&next_conn_id, 1);
    conn->state = CT_CONN_IDLE;
    conn->created = time(NULL);
    conn->last_activity = conn->created;
    
    /* Initialize buffers */
    conn->read_buf.data = malloc(CT_BUFFER_SIZE);
    conn->read_buf.size = CT_BUFFER_SIZE;
    atomic_init(&conn->read_buf.read_pos, 0);
    atomic_init(&conn->read_buf.write_pos, 0);
    
    conn->write_buf.data = malloc(CT_BUFFER_SIZE);
    conn->write_buf.size = CT_BUFFER_SIZE;
    atomic_init(&conn->write_buf.read_pos, 0);
    atomic_init(&conn->write_buf.write_pos, 0);
    
    if (!conn->read_buf.data || !conn->write_buf.data) {
        free(conn->read_buf.data);
        free(conn->write_buf.data);
        ct_mem_pool_free(server->conn_pool, conn);
        return NULL;
    }
    
    /* Add to connection hash table - O(1) */
    ct_hash_table_set(server->connections, &conn->id, sizeof(conn->id), conn);
    
    return conn;
}

/* Destroy connection */
void ct_connection_destroy(ct_server_t *server, ct_connection_t *conn) {
    if (!conn) return;
    
    /* Remove from event loop */
    event_del_connection(server, conn);
    
    /* Clean up proxy if active */
    if (conn->is_proxying) {
        ct_proxy_cleanup(conn);
    }
    
    /* Release file cache reference */
    if (conn->file_entry) {
        ct_file_cache_release(server->file_cache, conn->file_entry);
    }
    
    /* Close socket */
    if (conn->fd >= 0) {
        close(conn->fd);
    }
    
    /* Remove from hash table - O(1) */
    ct_hash_table_delete(server->connections, &conn->id, sizeof(conn->id));
    
    /* Free buffers */
    free(conn->read_buf.data);
    free(conn->write_buf.data);
    
    /* Clear sensitive data */
    memset(conn, 0, sizeof(ct_connection_t));
    
    /* Return to pool - O(1) */
    ct_mem_pool_free(server->conn_pool, conn);
    
    atomic_fetch_sub(&server->active_connections, 1);
}

/* Read data from connection */
int ct_connection_read(ct_connection_t *conn) {
    /* Calculate available space */
    size_t free_space = ct_ring_buffer_free_space(&conn->read_buf);
    if (free_space == 0) {
        /* Buffer full */
        return 0;
    }
    
    /* Read directly into ring buffer */
    char temp[8192];
    size_t to_read = (free_space < sizeof(temp)) ? free_space : sizeof(temp);
    
    ssize_t n = read(conn->fd, temp, to_read);
    if (n > 0) {
        ct_ring_buffer_write(&conn->read_buf, temp, n);
        conn->last_activity = time(NULL);
        return n;
    } else if (n == 0) {
        /* Connection closed */
        return -1;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; /* No data available */
        }
        return -1; /* Error */
    }
}

/* Write data to connection */
int ct_connection_write(ct_connection_t *conn) {
    /* Calculate available data */
    size_t available = ct_ring_buffer_available(&conn->write_buf);
    if (available == 0) {
        /* Nothing to write */
        return 0;
    }
    
    /* Write from ring buffer */
    char temp[8192];
    size_t to_write = (available < sizeof(temp)) ? available : sizeof(temp);
    size_t actual = ct_ring_buffer_read(&conn->write_buf, temp, to_write);
    
    ssize_t n = write(conn->fd, temp, actual);
    if (n > 0) {
        conn->last_activity = time(NULL);
        
        /* If we couldn't write everything, put it back */
        if (n < actual) {
            /* This is inefficient but rare - could optimize with peek/skip */
            ct_ring_buffer_write(&conn->write_buf, temp + n, actual - n);
        }
        
        return n;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Put data back */
            ct_ring_buffer_write(&conn->write_buf, temp, actual);
            return 0;
        }
        return -1; /* Error */
    }
}

/* Process connection - main request handler */
int ct_connection_process(ct_server_t *server, ct_connection_t *conn) {
    /* Handle WebSocket proxy */
    if (conn->is_proxying) {
        return ct_proxy_process(conn);
    }
    
    /* Handle WebSocket data */
    if (conn->is_websocket && conn->ws_handshake_done) {
        return ct_connection_process_websocket(server, conn);
    }
    
    /* Parse HTTP request */
    char buf[CT_BUFFER_SIZE];
    size_t available = ct_ring_buffer_peek(&conn->read_buf, buf, sizeof(buf));
    if (available == 0) return 0;
    
    int consumed = ct_parse_request(&conn->request, buf, available);
    if (consumed < 0) {
        if (consumed == -1) {
            /* Need more data */
            return 0;
        }
        /* Parse error */
        ct_response_init(&conn->response, 400, "Bad Request");
        ct_response_html(&conn->response, 400,
                        "<html><body><h1>400 Bad Request</h1></body></html>");
        goto send_response;
    }
    
    /* Consume parsed data */
    ct_ring_buffer_skip(&conn->read_buf, consumed);
    
    /* Request complete - process it */
    if (conn->request.parse_state == CT_PARSE_COMPLETE) {
        atomic_fetch_add(&server->total_requests, 1);
        
        /* Extract session from cookie */
        const char *cookie = ct_request_get_header(&conn->request, "Cookie");
        if (cookie) {
            char *session_id = ct_session_from_cookie(cookie);
            if (session_id) {
                conn->session = ct_session_find(server, session_id);
            }
        }
        
        /* Route request */
        ct_route_request(server, conn);
        
        /* Send response */
send_response:
        {
            char resp_buf[CT_BUFFER_SIZE];
            int resp_len = ct_build_response(&conn->response, resp_buf, 
                                           sizeof(resp_buf));
            if (resp_len > 0) {
                ct_ring_buffer_write(&conn->write_buf, resp_buf, resp_len);
            }
            
            /* Reset for next request if keep-alive */
            if (conn->request.keep_alive && !conn->is_websocket) {
                memset(&conn->request, 0, sizeof(conn->request));
                memset(&conn->response, 0, sizeof(conn->response));
            } else if (!conn->is_websocket) {
                conn->state = CT_CONN_CLOSING;
            }
        }
    }
    
    return 0;
}

/* Process WebSocket connection */
int ct_connection_process_websocket(ct_server_t *server, ct_connection_t *conn) {
    char buf[CT_BUFFER_SIZE];
    
    while (1) {
        size_t available = ct_ring_buffer_peek(&conn->read_buf, buf, sizeof(buf));
        if (available == 0) break;
        
        /* Parse WebSocket frame */
        ct_ws_opcode_t opcode;
        const char *payload;
        size_t payload_len;
        
        int frame_size = ct_ws_parse_frame(buf, available, &opcode,
                                          &payload, &payload_len);
        if (frame_size < 0) {
            if (frame_size == -1) {
                /* Need more data */
                break;
            }
            /* Protocol error */
            ct_ws_send_close(conn, 1002, "Protocol error");
            conn->state = CT_CONN_CLOSING;
            return -1;
        }
        
        /* Consume frame */
        ct_ring_buffer_skip(&conn->read_buf, frame_size);
        
        /* Process frame */
        ct_ws_process_frame(conn, opcode, payload, payload_len);
        
        /* Handle application-specific messages */
        if (opcode == CT_WS_TEXT || opcode == CT_WS_BINARY) {
            /* Echo server example - replace with actual logic */
            ct_ws_send_message(conn, opcode, payload, payload_len);
        }
    }
    
    return 0;
}

/* Route HTTP request to appropriate handler */
void ct_route_request(ct_server_t *server, ct_connection_t *conn) {
    const char *path = conn->request.url;
    
    /* API endpoints */
    if (strncmp(path, "/api/", 5) == 0) {
        ct_handle_api_request(server, conn);
        return;
    }
    
    /* WebSocket upgrade */
    if (conn->request.is_websocket) {
        if (strcmp(path, "/terminal-proxy") == 0) {
            /* Terminal WebSocket proxy */
            ct_proxy_terminal(conn, server->config.terminal_host,
                            server->config.terminal_port);
        } else {
            /* Regular WebSocket */
            ct_ws_handshake(conn);
        }
        return;
    }
    
    /* Static files */
    ct_serve_static_file(conn, server->config.static_dir, path);
}

/* Handle API requests */
void ct_handle_api_request(ct_server_t *server, ct_connection_t *conn) {
    const char *path = conn->request.url;
    
    /* Login endpoint */
    if (strcmp(path, "/api/login") == 0 && 
        conn->request.method == CT_METHOD_POST) {
        ct_handle_login(server, conn);
        return;
    }
    
    /* All other API endpoints require authentication */
    if (!conn->session || !ct_session_is_authenticated(conn->session)) {
        ct_response_json(&conn->response, 401,
                        "{\"error\":\"Unauthorized\",\"redirect\":\"/login\"}");
        return;
    }
    
    /* Authenticated endpoints */
    if (strcmp(path, "/api/logout") == 0 && 
        conn->request.method == CT_METHOD_POST) {
        ct_handle_logout(server, conn);
    } else if (strcmp(path, "/api/terminal-config") == 0) {
        ct_handle_terminal_config(server, conn);
    } else if (strcmp(path, "/api/session-status") == 0) {
        ct_handle_session_status(server, conn);
    } else {
        ct_response_json(&conn->response, 404,
                        "{\"error\":\"Not Found\"}");
    }
}

/* Handle login request */
void ct_handle_login(ct_server_t *server, ct_connection_t *conn) {
    /* Parse JSON body - simplified, real implementation needs JSON parser */
    const char *password = NULL;
    
    if (conn->request.body && conn->request.body_len > 0) {
        /* Extract password from JSON - this is simplified */
        const char *p = strstr(conn->request.body, "\"password\":\"");
        if (p) {
            p += 12;
            const char *end = strchr(p, '"');
            if (end) {
                static char pwd_buf[256];
                size_t len = end - p;
                if (len < sizeof(pwd_buf)) {
                    memcpy(pwd_buf, p, len);
                    pwd_buf[len] = '\0';
                    password = pwd_buf;
                }
            }
        }
    }
    
    if (!password) {
        ct_response_json(&conn->response, 400,
                        "{\"success\":false,\"message\":\"Missing password\"}");
        return;
    }
    
    /* Create or get session */
    if (!conn->session) {
        conn->session = ct_session_create(server);
        if (!conn->session) {
            ct_response_json(&conn->response, 500,
                            "{\"success\":false,\"message\":\"Session error\"}");
            return;
        }
        ct_session_set_cookie(&conn->response, conn->session->id);
    }
    
    /* Authenticate */
    if (ct_session_authenticate(conn->session, password, 
                               server->config.password_hash)) {
        ct_response_json(&conn->response, 200,
                        "{\"success\":true,\"sessionInfo\":{\"expiresIn\":\"30 days\",\"persistent\":true}}");
    } else {
        ct_response_json(&conn->response, 401,
                        "{\"success\":false,\"message\":\"Invalid password\"}");
    }
}

/* Handle logout request */
void ct_handle_logout(ct_server_t *server, ct_connection_t *conn) {
    if (conn->session) {
        ct_session_destroy(server, conn->session);
        conn->session = NULL;
    }
    
    ct_response_json(&conn->response, 200, "{\"success\":true}");
    ct_response_add_header(&conn->response, "Set-Cookie",
                          "sessionId=; Path=/; HttpOnly; Max-Age=0");
}

/* Handle terminal config request */
void ct_handle_terminal_config(ct_server_t *server, ct_connection_t *conn) {
    char json[512];
    snprintf(json, sizeof(json),
            "{\"host\":\"%s\",\"port\":%d,\"url\":\"/terminal-proxy\","
            "\"checkHealth\":true,\"rebootOnLogout\":false}",
            server->config.terminal_host, server->config.terminal_port);
    
    ct_response_json(&conn->response, 200, json);
}

/* Handle session status request */
void ct_handle_session_status(ct_server_t *server, ct_connection_t *conn) {
    char json[512];
    char time_buf[64];
    
    ct_get_timestamp(time_buf, sizeof(time_buf));
    
    snprintf(json, sizeof(json),
            "{\"authenticated\":true,\"loginTime\":\"%s\","
            "\"lastActivity\":\"%s\",\"sessionExpiry\":\"%s\"}",
            time_buf, time_buf, time_buf);
    
    ct_response_json(&conn->response, 200, json);
}