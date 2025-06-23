#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration constants */
#define CT_MAX_CONNECTIONS      100000
#define CT_MAX_SESSIONS         10000
#define CT_SESSION_ID_LEN       32
#define CT_BUFFER_SIZE          65536
#define CT_MAX_HEADERS          64
#define CT_MAX_PATH_LEN         4096
#define CT_HASH_TABLE_SIZE      16384
#define CT_MEM_POOL_CHUNK_SIZE  1024

/* Platform-specific definitions */
#ifdef LINUX
    #include <sys/epoll.h>
    #define CT_EVENT_MAX EPOLLIN | EPOLLOUT | EPOLLET
    typedef struct epoll_event ct_event_t;
#elif defined(DARWIN) || defined(BSD)
    #include <sys/event.h>
    #define CT_EVENT_MAX EVFILT_READ | EVFILT_WRITE
    typedef struct kevent ct_event_t;
#endif

/* Forward declarations */
typedef struct ct_connection ct_connection_t;
typedef struct ct_session ct_session_t;
typedef struct ct_server ct_server_t;
typedef struct ct_request ct_request_t;
typedef struct ct_response ct_response_t;

/* Memory pool for O(1) allocation */
typedef struct ct_mem_pool {
    void *free_list;
    void *chunks;
    size_t chunk_size;
    size_t total_chunks;
    size_t free_chunks;
} ct_mem_pool_t;

/* Lock-free ring buffer for async I/O */
typedef struct ct_ring_buffer {
    char *data;
    size_t size;
    _Atomic size_t read_pos;
    _Atomic size_t write_pos;
} ct_ring_buffer_t;

/* Red-black tree node for O(log n) operations */
typedef struct ct_rb_node {
    struct ct_rb_node *left;
    struct ct_rb_node *right;
    struct ct_rb_node *parent;
    int color;
} ct_rb_node_t;

/* Hash table for O(1) lookups */
typedef struct ct_hash_table {
    void **buckets;
    size_t size;
    size_t count;
    uint32_t (*hash_func)(const void *key, size_t len);
} ct_hash_table_t;

/* HTTP request parser state */
typedef enum {
    CT_PARSE_METHOD,
    CT_PARSE_URL,
    CT_PARSE_VERSION,
    CT_PARSE_HEADER_NAME,
    CT_PARSE_HEADER_VALUE,
    CT_PARSE_BODY,
    CT_PARSE_COMPLETE,
    CT_PARSE_ERROR
} ct_parse_state_t;

/* Connection states */
typedef enum {
    CT_CONN_IDLE,
    CT_CONN_READING,
    CT_CONN_WRITING,
    CT_CONN_PROXYING,
    CT_CONN_CLOSING
} ct_conn_state_t;

/* HTTP methods */
typedef enum {
    CT_METHOD_GET,
    CT_METHOD_POST,
    CT_METHOD_PUT,
    CT_METHOD_DELETE,
    CT_METHOD_HEAD,
    CT_METHOD_OPTIONS,
    CT_METHOD_CONNECT,
    CT_METHOD_UNKNOWN
} ct_http_method_t;

/* WebSocket opcodes */
typedef enum {
    CT_WS_CONTINUATION = 0x0,
    CT_WS_TEXT = 0x1,
    CT_WS_BINARY = 0x2,
    CT_WS_CLOSE = 0x8,
    CT_WS_PING = 0x9,
    CT_WS_PONG = 0xA
} ct_ws_opcode_t;

/* HTTP header */
typedef struct ct_header {
    const char *name;
    const char *value;
    size_t name_len;
    size_t value_len;
} ct_header_t;

/* HTTP request */
struct ct_request {
    ct_http_method_t method;
    const char *url;
    const char *version;
    ct_header_t headers[CT_MAX_HEADERS];
    size_t header_count;
    const char *body;
    size_t body_len;
    ct_parse_state_t parse_state;
    bool is_websocket;
    bool keep_alive;
};

/* HTTP response */
struct ct_response {
    int status_code;
    const char *status_text;
    ct_header_t headers[CT_MAX_HEADERS];
    size_t header_count;
    const char *body;
    size_t body_len;
    bool chunked;
};

/* Session data with O(1) hash lookup and O(log n) expiry */
struct ct_session {
    char id[CT_SESSION_ID_LEN + 1];
    time_t created;
    time_t last_access;
    bool authenticated;
    void *user_data;
    
    /* Hash table chain */
    struct ct_session *hash_next;
    
    /* Red-black tree node for expiry */
    ct_rb_node_t expiry_node;
};

/* Connection structure */
struct ct_connection {
    int fd;
    uint64_t id;
    ct_conn_state_t state;
    ct_session_t *session;
    ct_request_t request;
    ct_response_t response;
    
    /* Buffers */
    ct_ring_buffer_t read_buf;
    ct_ring_buffer_t write_buf;
    
    /* WebSocket state */
    bool is_websocket;
    bool ws_handshake_done;
    uint8_t ws_mask_key[4];
    
    /* Proxy state */
    int proxy_fd;
    bool is_proxying;
    
    /* Timing */
    time_t created;
    time_t last_activity;
    
    /* Hash table chain */
    struct ct_connection *hash_next;
};

/* Server configuration */
typedef struct ct_config {
    const char *host;
    uint16_t port;
    const char *static_dir;
    const char *terminal_host;
    uint16_t terminal_port;
    const char *password_hash;
    size_t max_connections;
    size_t max_sessions;
    time_t session_timeout;
    bool enable_compression;
    bool enable_ssl;
} ct_config_t;

/* Main server structure */
struct ct_server {
    int listen_fd;
    int event_fd;
    ct_config_t config;
    
    /* Connection management */
    ct_hash_table_t *connections;
    ct_mem_pool_t *conn_pool;
    
    /* Session management */
    ct_hash_table_t *sessions;
    ct_rb_node_t *session_expiry_tree;
    ct_mem_pool_t *session_pool;
    
    /* File cache */
    ct_hash_table_t *file_cache;
    
    /* Statistics */
    _Atomic uint64_t total_requests;
    _Atomic uint64_t active_connections;
    _Atomic uint64_t active_sessions;
};

/* Core server functions */
ct_server_t *ct_server_create(const ct_config_t *config);
void ct_server_destroy(ct_server_t *server);
int ct_server_run(ct_server_t *server);
void ct_server_stop(ct_server_t *server);

/* Connection management */
ct_connection_t *ct_connection_create(ct_server_t *server, int fd);
void ct_connection_destroy(ct_server_t *server, ct_connection_t *conn);
int ct_connection_read(ct_connection_t *conn);
int ct_connection_write(ct_connection_t *conn);
int ct_connection_process(ct_server_t *server, ct_connection_t *conn);

/* Session management */
ct_session_t *ct_session_create(ct_server_t *server);
ct_session_t *ct_session_find(ct_server_t *server, const char *id);
void ct_session_destroy(ct_server_t *server, ct_session_t *session);
void ct_session_cleanup_expired(ct_server_t *server);

/* HTTP parsing */
int ct_parse_request(ct_request_t *req, const char *data, size_t len);
int ct_build_response(ct_response_t *resp, char *buf, size_t buf_len);

/* WebSocket handling */
int ct_ws_handshake(ct_connection_t *conn);
int ct_ws_parse_frame(const char *data, size_t len, ct_ws_opcode_t *opcode, 
                      const char **payload, size_t *payload_len);
int ct_ws_build_frame(ct_ws_opcode_t opcode, const char *payload, 
                      size_t payload_len, char *buf, size_t buf_len);

/* Authentication */
bool ct_auth_verify_password(const char *password, const char *hash);
char *ct_auth_hash_password(const char *password);

/* Utility functions */
uint32_t ct_hash_fnv1a(const void *key, size_t len);
uint32_t ct_hash_murmur3(const void *key, size_t len);
void ct_get_timestamp(char *buf, size_t buf_len);
int ct_parse_url(const char *url, char *path, size_t path_len, 
                 char *query, size_t query_len);

/* Memory pool operations */
ct_mem_pool_t *ct_mem_pool_create(size_t chunk_size, size_t initial_chunks);
void ct_mem_pool_destroy(ct_mem_pool_t *pool);
void *ct_mem_pool_alloc(ct_mem_pool_t *pool);
void ct_mem_pool_free(ct_mem_pool_t *pool, void *ptr);

/* Ring buffer operations */
ct_ring_buffer_t *ct_ring_buffer_create(size_t size);
void ct_ring_buffer_destroy(ct_ring_buffer_t *rb);
size_t ct_ring_buffer_write(ct_ring_buffer_t *rb, const char *data, size_t len);
size_t ct_ring_buffer_read(ct_ring_buffer_t *rb, char *data, size_t len);
size_t ct_ring_buffer_available(ct_ring_buffer_t *rb);

/* Hash table operations */
ct_hash_table_t *ct_hash_table_create(size_t size, 
                                      uint32_t (*hash_func)(const void *, size_t));
void ct_hash_table_destroy(ct_hash_table_t *ht);
void *ct_hash_table_get(ct_hash_table_t *ht, const void *key, size_t key_len);
void ct_hash_table_set(ct_hash_table_t *ht, const void *key, size_t key_len, 
                       void *value);
void ct_hash_table_delete(ct_hash_table_t *ht, const void *key, size_t key_len);

/* Red-black tree operations */
void ct_rb_insert(ct_rb_node_t **root, ct_rb_node_t *node, 
                  int (*compare)(ct_rb_node_t *, ct_rb_node_t *));
void ct_rb_delete(ct_rb_node_t **root, ct_rb_node_t *node);
ct_rb_node_t *ct_rb_find_min(ct_rb_node_t *root);

#ifdef __cplusplus
}
#endif

#endif /* TERMINAL_H */