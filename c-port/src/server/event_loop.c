#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#ifdef LINUX
#include <sys/epoll.h>
#elif defined(DARWIN) || defined(BSD)
#include <sys/event.h>
#include <sys/time.h>
#endif

static volatile bool g_running = true;

/* Set socket to non-blocking mode */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* Set socket options for performance */
static void set_socket_options(int fd) {
    int yes = 1;
    
    /* Reuse address */
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    
    /* Disable Nagle's algorithm for low latency */
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    
    /* Set socket buffer sizes */
    int bufsize = 256 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
    
#ifdef SO_REUSEPORT
    /* Enable SO_REUSEPORT for load balancing */
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
    
#ifdef TCP_FASTOPEN
    /* Enable TCP Fast Open */
    int qlen = 5;
    setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));
#endif
}

/* Create listening socket */
static int create_listen_socket(const char *host, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    
    set_socket_options(fd);
    set_nonblocking(fd);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        close(fd);
        return -1;
    }
    
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    
    if (listen(fd, SOMAXCONN) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    
    return fd;
}

#ifdef LINUX
/* Linux epoll implementation */

static int event_init(ct_server_t *server) {
    server->event_fd = epoll_create1(EPOLL_CLOEXEC);
    if (server->event_fd < 0) {
        perror("epoll_create1");
        return -1;
    }
    
    /* Add listen socket to epoll */
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = NULL; /* NULL means listen socket */
    
    if (epoll_ctl(server->event_fd, EPOLL_CTL_ADD, server->listen_fd, &ev) < 0) {
        perror("epoll_ctl");
        close(server->event_fd);
        return -1;
    }
    
    return 0;
}

static int event_add_connection(ct_server_t *server, ct_connection_t *conn) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.ptr = conn;
    
    return epoll_ctl(server->event_fd, EPOLL_CTL_ADD, conn->fd, &ev);
}

static int event_mod_connection(ct_server_t *server, ct_connection_t *conn, 
                               uint32_t events) {
    struct epoll_event ev;
    ev.events = events | EPOLLET;
    ev.data.ptr = conn;
    
    return epoll_ctl(server->event_fd, EPOLL_CTL_MOD, conn->fd, &ev);
}

static int event_del_connection(ct_server_t *server, ct_connection_t *conn) {
    return epoll_ctl(server->event_fd, EPOLL_CTL_DEL, conn->fd, NULL);
}

static int event_wait(ct_server_t *server, ct_event_t *events, int max_events) {
    return epoll_wait(server->event_fd, events, max_events, 1000);
}

#elif defined(DARWIN) || defined(BSD)
/* macOS/BSD kqueue implementation */

static int event_init(ct_server_t *server) {
    server->event_fd = kqueue();
    if (server->event_fd < 0) {
        perror("kqueue");
        return -1;
    }
    
    /* Add listen socket to kqueue */
    struct kevent ev;
    EV_SET(&ev, server->listen_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    
    if (kevent(server->event_fd, &ev, 1, NULL, 0, NULL) < 0) {
        perror("kevent");
        close(server->event_fd);
        return -1;
    }
    
    return 0;
}

static int event_add_connection(ct_server_t *server, ct_connection_t *conn) {
    struct kevent ev[2];
    EV_SET(&ev[0], conn->fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, conn);
    EV_SET(&ev[1], conn->fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, conn);
    
    return kevent(server->event_fd, ev, 2, NULL, 0, NULL);
}

static int event_mod_connection(ct_server_t *server, ct_connection_t *conn, 
                               uint32_t events) {
    /* kqueue doesn't need explicit modification */
    return 0;
}

static int event_del_connection(ct_server_t *server, ct_connection_t *conn) {
    struct kevent ev[2];
    EV_SET(&ev[0], conn->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&ev[1], conn->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    
    return kevent(server->event_fd, ev, 2, NULL, 0, NULL);
}

static int event_wait(ct_server_t *server, ct_event_t *events, int max_events) {
    struct timespec timeout = {1, 0}; /* 1 second timeout */
    return kevent(server->event_fd, NULL, 0, events, max_events, &timeout);
}
#endif

/* Accept new connections */
static void accept_connections(ct_server_t *server) {
    while (1) {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        
        int fd = accept(server->listen_fd, (struct sockaddr *)&addr, &addrlen);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; /* No more connections */
            }
            perror("accept");
            continue;
        }
        
        /* Check connection limit */
        if (server->active_connections >= server->config.max_connections) {
            close(fd);
            continue;
        }
        
        set_nonblocking(fd);
        set_socket_options(fd);
        
        /* Create connection object */
        ct_connection_t *conn = ct_connection_create(server, fd);
        if (!conn) {
            close(fd);
            continue;
        }
        
        /* Add to event loop */
        if (event_add_connection(server, conn) < 0) {
            ct_connection_destroy(server, conn);
            continue;
        }
        
        atomic_fetch_add(&server->active_connections, 1);
    }
}

/* Main server implementation */
ct_server_t *ct_server_create(const ct_config_t *config) {
    ct_server_t *server = calloc(1, sizeof(ct_server_t));
    if (!server) return NULL;
    
    /* Copy configuration */
    memcpy(&server->config, config, sizeof(ct_config_t));
    
    /* Create listen socket */
    server->listen_fd = create_listen_socket(config->host, config->port);
    if (server->listen_fd < 0) {
        free(server);
        return NULL;
    }
    
    /* Initialize event system */
    if (event_init(server) < 0) {
        close(server->listen_fd);
        free(server);
        return NULL;
    }
    
    /* Create connection hash table and pool */
    server->connections = ct_hash_table_create(CT_HASH_TABLE_SIZE, ct_hash_fnv1a);
    server->conn_pool = ct_mem_pool_create(sizeof(ct_connection_t), 1024);
    
    /* Create session hash table and pool */
    server->sessions = ct_hash_table_create(CT_HASH_TABLE_SIZE, ct_hash_fnv1a);
    server->session_pool = ct_mem_pool_create(sizeof(ct_session_t), 256);
    
    /* Create file cache */
    server->file_cache = ct_hash_table_create(1024, ct_hash_fnv1a);
    
    /* Initialize statistics */
    atomic_init(&server->total_requests, 0);
    atomic_init(&server->active_connections, 0);
    atomic_init(&server->active_sessions, 0);
    
    return server;
}

void ct_server_destroy(ct_server_t *server) {
    if (!server) return;
    
    close(server->listen_fd);
    close(server->event_fd);
    
    ct_hash_table_destroy(server->connections);
    ct_mem_pool_destroy(server->conn_pool);
    
    ct_hash_table_destroy(server->sessions);
    ct_mem_pool_destroy(server->session_pool);
    
    ct_hash_table_destroy(server->file_cache);
    
    free(server);
}

void ct_server_stop(ct_server_t *server) {
    g_running = false;
}

int ct_server_run(ct_server_t *server) {
    ct_event_t events[1024];
    
    while (g_running) {
        int nev = event_wait(server, events, 1024);
        
        if (nev < 0) {
            if (errno == EINTR) continue;
            perror("event_wait");
            return -1;
        }
        
        for (int i = 0; i < nev; i++) {
#ifdef LINUX
            if (events[i].data.ptr == NULL) {
                /* Listen socket event */
                accept_connections(server);
            } else {
                /* Connection event */
                ct_connection_t *conn = events[i].data.ptr;
                
                if (events[i].events & EPOLLIN) {
                    if (ct_connection_read(conn) < 0) {
                        ct_connection_destroy(server, conn);
                        continue;
                    }
                    
                    if (ct_connection_process(server, conn) < 0) {
                        ct_connection_destroy(server, conn);
                        continue;
                    }
                }
                
                if (events[i].events & EPOLLOUT) {
                    if (ct_connection_write(conn) < 0) {
                        ct_connection_destroy(server, conn);
                        continue;
                    }
                }
                
                if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    ct_connection_destroy(server, conn);
                }
            }
#elif defined(DARWIN) || defined(BSD)
            if (events[i].udata == NULL) {
                /* Listen socket event */
                accept_connections(server);
            } else {
                /* Connection event */
                ct_connection_t *conn = events[i].udata;
                
                if (events[i].filter == EVFILT_READ) {
                    if (ct_connection_read(conn) < 0) {
                        ct_connection_destroy(server, conn);
                        continue;
                    }
                    
                    if (ct_connection_process(server, conn) < 0) {
                        ct_connection_destroy(server, conn);
                        continue;
                    }
                }
                
                if (events[i].filter == EVFILT_WRITE) {
                    if (ct_connection_write(conn) < 0) {
                        ct_connection_destroy(server, conn);
                        continue;
                    }
                }
                
                if (events[i].flags & EV_EOF) {
                    ct_connection_destroy(server, conn);
                }
            }
#endif
        }
        
        /* Periodic cleanup */
        static time_t last_cleanup = 0;
        time_t now = time(NULL);
        if (now - last_cleanup > 60) {
            ct_session_cleanup_expired(server);
            last_cleanup = now;
        }
    }
    
    return 0;
}