#include "cloudterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* Proxy connection state */
typedef struct {
    int backend_fd;
    ct_ring_buffer_t *backend_read_buf;
    ct_ring_buffer_t *backend_write_buf;
    bool backend_connected;
    bool backend_handshake_done;
} proxy_state_t;

/* Connect to backend terminal server */
static int connect_to_backend(const char *host, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    /* Set non-blocking */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    /* Set socket options */
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    
    /* Resolve host */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        /* Try hostname resolution */
        struct hostent *he = gethostbyname(host);
        if (!he) {
            close(fd);
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);
    }
    
    /* Connect (non-blocking) */
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        if (errno != EINPROGRESS) {
            close(fd);
            return -1;
        }
    }
    
    return fd;
}

/* Initialize WebSocket proxy */
int ct_proxy_init(ct_connection_t *conn, const char *backend_host, 
                  uint16_t backend_port) {
    proxy_state_t *proxy = calloc(1, sizeof(proxy_state_t));
    if (!proxy) return -1;
    
    /* Create buffers */
    proxy->backend_read_buf = ct_ring_buffer_create(CT_BUFFER_SIZE);
    proxy->backend_write_buf = ct_ring_buffer_create(CT_BUFFER_SIZE);
    
    if (!proxy->backend_read_buf || !proxy->backend_write_buf) {
        ct_ring_buffer_destroy(proxy->backend_read_buf);
        ct_ring_buffer_destroy(proxy->backend_write_buf);
        free(proxy);
        return -1;
    }
    
    /* Connect to backend */
    proxy->backend_fd = connect_to_backend(backend_host, backend_port);
    if (proxy->backend_fd < 0) {
        ct_ring_buffer_destroy(proxy->backend_read_buf);
        ct_ring_buffer_destroy(proxy->backend_write_buf);
        free(proxy);
        return -1;
    }
    
    conn->proxy_state = proxy;
    conn->is_proxying = true;
    
    /* Add backend to event loop */
    if (event_add_backend(conn->server, proxy->backend_fd, conn) < 0) {
        close(proxy->backend_fd);
        ct_ring_buffer_destroy(proxy->backend_read_buf);
        ct_ring_buffer_destroy(proxy->backend_write_buf);
        free(proxy);
        conn->proxy_state = NULL;
        return -1;
    }
    
    return 0;
}

/* Send WebSocket handshake to backend */
static int send_backend_handshake(proxy_state_t *proxy, const char *path) {
    char handshake[1024];
    
    /* Generate random WebSocket key */
    char ws_key[25];
    for (int i = 0; i < 16; i++) {
        ws_key[i] = rand() & 0xFF;
    }
    
    /* Base64 encode */
    char ws_key_b64[32];
    base64_encode((unsigned char *)ws_key, 16, ws_key_b64);
    
    /* Build handshake */
    int len = snprintf(handshake, sizeof(handshake),
        "GET %s HTTP/1.1\r\n"
        "Host: terminal\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path, ws_key_b64);
    
    return ct_ring_buffer_write(proxy->backend_write_buf, handshake, len);
}

/* Parse backend handshake response */
static int parse_backend_handshake(proxy_state_t *proxy) {
    char buf[CT_BUFFER_SIZE];
    size_t len = ct_ring_buffer_peek(proxy->backend_read_buf, buf, sizeof(buf));
    
    /* Look for end of headers */
    const char *end = strstr(buf, "\r\n\r\n");
    if (!end) return -1; /* Need more data */
    
    /* Check status */
    if (strncmp(buf, "HTTP/1.1 101", 12) != 0) {
        return -2; /* Not switching protocols */
    }
    
    /* Consume headers */
    size_t header_len = (end - buf) + 4;
    ct_ring_buffer_skip(proxy->backend_read_buf, header_len);
    
    proxy->backend_handshake_done = true;
    return 0;
}

/* Forward data between client and backend using zero-copy splice */
static int forward_data_splice(int from_fd, int to_fd, size_t max_bytes) {
#ifdef __linux__
    /* Use splice for zero-copy on Linux */
    ssize_t n = splice(from_fd, NULL, to_fd, NULL, max_bytes, 
                      SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        return -1;
    }
    return n > 0 ? n : 0;
#else
    /* Fallback for non-Linux */
    char buf[8192];
    ssize_t n = read(from_fd, buf, sizeof(buf));
    if (n <= 0) return n;
    
    ssize_t written = 0;
    while (written < n) {
        ssize_t w = write(to_fd, buf + written, n - written);
        if (w < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return -1;
        }
        written += w;
    }
    return written;
#endif
}

/* Process proxy data */
int ct_proxy_process(ct_connection_t *conn) {
    proxy_state_t *proxy = conn->proxy_state;
    if (!proxy) return -1;
    
    /* Handle backend connection */
    if (!proxy->backend_connected) {
        /* Check if connected */
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(proxy->backend_fd, SOL_SOCKET, SO_ERROR, 
                      &error, &len) < 0 || error != 0) {
            return -1; /* Connection failed */
        }
        
        proxy->backend_connected = true;
        
        /* Send WebSocket handshake */
        send_backend_handshake(proxy, "/ws");
    }
    
    /* Handle backend handshake */
    if (!proxy->backend_handshake_done) {
        /* Read backend response */
        char buf[CT_BUFFER_SIZE];
        ssize_t n = read(proxy->backend_fd, buf, sizeof(buf));
        if (n > 0) {
            ct_ring_buffer_write(proxy->backend_read_buf, buf, n);
            
            /* Try to parse handshake */
            if (parse_backend_handshake(proxy) < 0) {
                return -1;
            }
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            return -1;
        }
    }
    
    if (!proxy->backend_handshake_done) {
        return 0; /* Still waiting for handshake */
    }
    
    /* Forward WebSocket frames between client and backend */
    
    /* Client -> Backend */
    while (ct_ring_buffer_available(&conn->read_buf) > 0) {
        char buf[CT_BUFFER_SIZE];
        size_t available = ct_ring_buffer_peek(&conn->read_buf, buf, sizeof(buf));
        
        /* Parse WebSocket frame */
        ct_ws_opcode_t opcode;
        const char *payload;
        size_t payload_len;
        
        int frame_size = ct_ws_parse_frame(buf, available, &opcode, 
                                          &payload, &payload_len);
        if (frame_size < 0) break; /* Need more data */
        
        /* Forward to backend (frames from client are masked) */
        ssize_t n = write(proxy->backend_fd, buf, frame_size);
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) return -1;
            break;
        }
        
        ct_ring_buffer_skip(&conn->read_buf, frame_size);
    }
    
    /* Backend -> Client */
    char buf[CT_BUFFER_SIZE];
    ssize_t n = read(proxy->backend_fd, buf, sizeof(buf));
    if (n > 0) {
        /* Backend frames are not masked, forward as-is */
        ct_ring_buffer_write(&conn->write_buf, buf, n);
    } else if (n == 0) {
        /* Backend closed */
        return -1;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        return -1;
    }
    
    return 0;
}

/* Clean up proxy resources */
void ct_proxy_cleanup(ct_connection_t *conn) {
    proxy_state_t *proxy = conn->proxy_state;
    if (!proxy) return;
    
    if (proxy->backend_fd >= 0) {
        close(proxy->backend_fd);
    }
    
    ct_ring_buffer_destroy(proxy->backend_read_buf);
    ct_ring_buffer_destroy(proxy->backend_write_buf);
    
    free(proxy);
    conn->proxy_state = NULL;
    conn->is_proxying = false;
}

/* High-performance terminal proxy with zero-copy */
int ct_proxy_terminal(ct_connection_t *conn, const char *terminal_host,
                     uint16_t terminal_port) {
    /* Perform WebSocket handshake with client first */
    if (!conn->ws_handshake_done) {
        if (ct_ws_handshake(conn) < 0) {
            return -1;
        }
        
        /* Send handshake response */
        char buf[1024];
        int len = ct_build_response(&conn->response, buf, sizeof(buf));
        ct_ring_buffer_write(&conn->write_buf, buf, len);
    }
    
    /* Initialize proxy if not done */
    if (!conn->is_proxying) {
        if (ct_proxy_init(conn, terminal_host, terminal_port) < 0) {
            /* Send error to client */
            ct_ws_send_text(conn, "{\"error\":\"Failed to connect to terminal\"}");
            return -1;
        }
    }
    
    /* Process proxy data */
    return ct_proxy_process(conn);
}