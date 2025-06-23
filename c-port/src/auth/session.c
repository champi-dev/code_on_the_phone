#include "cloudterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Session comparison for red-black tree */
static int session_expiry_compare(ct_rb_node_t *a, ct_rb_node_t *b) {
    ct_session_t *sa = (ct_session_t *)((char *)a - offsetof(ct_session_t, expiry_node));
    ct_session_t *sb = (ct_session_t *)((char *)b - offsetof(ct_session_t, expiry_node));
    
    /* Compare by last access time for expiry */
    if (sa->last_access < sb->last_access) return -1;
    if (sa->last_access > sb->last_access) return 1;
    
    /* If equal, compare by ID for stability */
    return strcmp(sa->id, sb->id);
}

/* Create new session */
ct_session_t *ct_session_create(ct_server_t *server) {
    /* Allocate from pool - O(1) */
    ct_session_t *session = ct_mem_pool_alloc(server->session_pool);
    if (!session) return NULL;
    
    /* Initialize session */
    memset(session, 0, sizeof(ct_session_t));
    ct_generate_session_id(session->id, CT_SESSION_ID_LEN);
    
    time_t now = time(NULL);
    session->created = now;
    session->last_access = now;
    session->authenticated = false;
    
    /* Add to hash table - O(1) */
    ct_hash_table_set(server->sessions, session->id, strlen(session->id), session);
    
    /* Add to expiry tree - O(log n) */
    ct_rb_insert(&server->session_expiry_tree, &session->expiry_node, 
                 session_expiry_compare);
    
    atomic_fetch_add(&server->active_sessions, 1);
    
    return session;
}

/* Find session by ID - O(1) average */
ct_session_t *ct_session_find(ct_server_t *server, const char *id) {
    if (!id || strlen(id) != CT_SESSION_ID_LEN) return NULL;
    
    ct_session_t *session = ct_hash_table_get(server->sessions, id, strlen(id));
    
    if (session) {
        /* Update last access time */
        time_t now = time(NULL);
        
        /* Remove from tree, update time, reinsert - O(log n) */
        ct_rb_delete(&server->session_expiry_tree, &session->expiry_node);
        session->last_access = now;
        ct_rb_insert(&server->session_expiry_tree, &session->expiry_node,
                    session_expiry_compare);
    }
    
    return session;
}

/* Destroy session */
void ct_session_destroy(ct_server_t *server, ct_session_t *session) {
    if (!session) return;
    
    /* Remove from hash table - O(1) */
    ct_hash_table_delete(server->sessions, session->id, strlen(session->id));
    
    /* Remove from expiry tree - O(log n) */
    ct_rb_delete(&server->session_expiry_tree, &session->expiry_node);
    
    /* Clear sensitive data */
    memset(session, 0, sizeof(ct_session_t));
    
    /* Return to pool - O(1) */
    ct_mem_pool_free(server->session_pool, session);
    
    atomic_fetch_sub(&server->active_sessions, 1);
}

/* Clean up expired sessions - O(k log n) where k is expired count */
void ct_session_cleanup_expired(ct_server_t *server) {
    time_t now = time(NULL);
    time_t expiry_time = now - server->config.session_timeout;
    
    /* Find all expired sessions from tree minimum */
    while (1) {
        ct_rb_node_t *node = ct_rb_find_min(server->session_expiry_tree);
        if (!node) break;
        
        ct_session_t *session = (ct_session_t *)((char *)node - 
                                offsetof(ct_session_t, expiry_node));
        
        /* If this session isn't expired, we're done */
        if (session->last_access > expiry_time) break;
        
        /* Destroy expired session */
        ct_session_destroy(server, session);
    }
}

/* Session cookie handling */
void ct_session_set_cookie(ct_response_t *resp, const char *session_id) {
    char cookie[256];
    
    /* Build secure cookie */
    snprintf(cookie, sizeof(cookie),
             "sessionId=%s; Path=/; HttpOnly; SameSite=Lax; Max-Age=2592000",
             session_id);
    
    ct_response_add_header(resp, "Set-Cookie", cookie);
}

/* Extract session ID from cookie header */
char *ct_session_from_cookie(const char *cookie_header) {
    static char session_id[CT_SESSION_ID_LEN + 1];
    
    if (!cookie_header) return NULL;
    
    /* Find sessionId cookie */
    const char *p = strstr(cookie_header, "sessionId=");
    if (!p) return NULL;
    
    p += 10; /* Skip "sessionId=" */
    
    /* Extract session ID */
    size_t i;
    for (i = 0; i < CT_SESSION_ID_LEN && p[i] && p[i] != ';' && p[i] != ' '; i++) {
        session_id[i] = p[i];
    }
    
    if (i != CT_SESSION_ID_LEN) return NULL;
    
    session_id[i] = '\0';
    return session_id;
}

/* Authenticate session */
bool ct_session_authenticate(ct_session_t *session, const char *password,
                            const char *password_hash) {
    if (!session || !password || !password_hash) return false;
    
    /* Verify password */
    if (!ct_auth_verify_password(password, password_hash)) {
        return false;
    }
    
    /* Mark session as authenticated */
    session->authenticated = true;
    session->last_access = time(NULL);
    
    return true;
}

/* Check if session is authenticated */
bool ct_session_is_authenticated(ct_session_t *session) {
    return session && session->authenticated;
}

/* Session statistics */
void ct_session_get_stats(ct_server_t *server, size_t *total, size_t *authenticated) {
    *total = atomic_load(&server->active_sessions);
    
    /* Count authenticated sessions - could be optimized with separate counter */
    *authenticated = 0;
    
    /* This is O(n) but typically called rarely */
    struct {
        size_t count;
    } ctx = {0};
    
    void count_authenticated(void *key, size_t key_len, void *value, void *context) {
        ct_session_t *session = (ct_session_t *)value;
        struct { size_t count; } *ctx = context;
        if (session->authenticated) {
            ctx->count++;
        }
    }
    
    ct_hash_table_foreach(server->sessions, count_authenticated, &ctx);
    *authenticated = ctx.count;
}