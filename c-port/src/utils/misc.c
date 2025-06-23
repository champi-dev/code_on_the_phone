#include "cloudterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* Get current timestamp in ISO format */
void ct_get_timestamp(char *buf, size_t buf_len) {
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    
    strftime(buf, buf_len, "%Y-%m-%dT%H:%M:%SZ", tm);
}

/* URL decode */
static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

char *ct_url_decode(const char *str, size_t len) {
    char *decoded = malloc(len + 1);
    if (!decoded) return NULL;
    
    size_t i, j;
    for (i = 0, j = 0; i < len; i++, j++) {
        if (str[i] == '%' && i + 2 < len) {
            int high = hex_to_int(str[i + 1]);
            int low = hex_to_int(str[i + 2]);
            if (high >= 0 && low >= 0) {
                decoded[j] = (high << 4) | low;
                i += 2;
                continue;
            }
        } else if (str[i] == '+') {
            decoded[j] = ' ';
            continue;
        }
        decoded[j] = str[i];
    }
    
    decoded[j] = '\0';
    return decoded;
}

/* URL encode */
char *ct_url_encode(const char *str) {
    size_t len = strlen(str);
    char *encoded = malloc(len * 3 + 1);
    if (!encoded) return NULL;
    
    size_t i, j;
    for (i = 0, j = 0; i < len; i++) {
        if (isalnum(str[i]) || str[i] == '-' || str[i] == '_' || 
            str[i] == '.' || str[i] == '~') {
            encoded[j++] = str[i];
        } else {
            sprintf(encoded + j, "%%%02X", (unsigned char)str[i]);
            j += 3;
        }
    }
    
    encoded[j] = '\0';
    return encoded;
}

/* Parse query string */
ct_hash_table_t *ct_parse_query_string(const char *query) {
    if (!query || !*query) return NULL;
    
    ct_hash_table_t *params = ct_hash_table_create(16, ct_hash_fnv1a);
    if (!params) return NULL;
    
    char *query_copy = strdup(query);
    if (!query_copy) {
        ct_hash_table_destroy(params);
        return NULL;
    }
    
    char *saveptr;
    char *pair = strtok_r(query_copy, "&", &saveptr);
    
    while (pair) {
        char *eq = strchr(pair, '=');
        if (eq) {
            *eq = '\0';
            char *key = ct_url_decode(pair, strlen(pair));
            char *value = ct_url_decode(eq + 1, strlen(eq + 1));
            
            if (key && value) {
                ct_hash_table_set(params, key, strlen(key), value);
            }
            
            free(key);
        }
        
        pair = strtok_r(NULL, "&", &saveptr);
    }
    
    free(query_copy);
    return params;
}

/* Simple JSON builder */
typedef struct {
    char *buf;
    size_t size;
    size_t pos;
    int depth;
    bool in_array;
} json_builder_t;

json_builder_t *ct_json_create(size_t initial_size) {
    json_builder_t *json = calloc(1, sizeof(json_builder_t));
    if (!json) return NULL;
    
    json->buf = malloc(initial_size);
    if (!json->buf) {
        free(json);
        return NULL;
    }
    
    json->size = initial_size;
    json->buf[0] = '{';
    json->pos = 1;
    
    return json;
}

static void json_ensure_space(json_builder_t *json, size_t needed) {
    if (json->pos + needed >= json->size) {
        size_t new_size = json->size * 2;
        while (json->pos + needed >= new_size) {
            new_size *= 2;
        }
        
        char *new_buf = realloc(json->buf, new_size);
        if (new_buf) {
            json->buf = new_buf;
            json->size = new_size;
        }
    }
}

void ct_json_add_string(json_builder_t *json, const char *key, const char *value) {
    if (!json || !key || !value) return;
    
    /* Add comma if needed */
    if (json->pos > 1 && json->buf[json->pos - 1] != '{') {
        json_ensure_space(json, 1);
        json->buf[json->pos++] = ',';
    }
    
    /* Calculate needed space */
    size_t needed = strlen(key) + strlen(value) + 16;
    json_ensure_space(json, needed);
    
    /* Add key:value */
    json->pos += sprintf(json->buf + json->pos, "\"%s\":\"%s\"", key, value);
}

void ct_json_add_int(json_builder_t *json, const char *key, int value) {
    if (!json || !key) return;
    
    if (json->pos > 1 && json->buf[json->pos - 1] != '{') {
        json_ensure_space(json, 1);
        json->buf[json->pos++] = ',';
    }
    
    size_t needed = strlen(key) + 32;
    json_ensure_space(json, needed);
    
    json->pos += sprintf(json->buf + json->pos, "\"%s\":%d", key, value);
}

void ct_json_add_bool(json_builder_t *json, const char *key, bool value) {
    if (!json || !key) return;
    
    if (json->pos > 1 && json->buf[json->pos - 1] != '{') {
        json_ensure_space(json, 1);
        json->buf[json->pos++] = ',';
    }
    
    size_t needed = strlen(key) + 16;
    json_ensure_space(json, needed);
    
    json->pos += sprintf(json->buf + json->pos, "\"%s\":%s", 
                        key, value ? "true" : "false");
}

char *ct_json_finish(json_builder_t *json) {
    if (!json) return NULL;
    
    json_ensure_space(json, 2);
    json->buf[json->pos++] = '}';
    json->buf[json->pos] = '\0';
    
    char *result = json->buf;
    free(json);
    
    return result;
}

/* Error response helpers */
void ct_response_error(ct_response_t *resp, int status, const char *message) {
    json_builder_t *json = ct_json_create(256);
    ct_json_add_string(json, "error", message);
    ct_json_add_int(json, "status", status);
    
    char *body = ct_json_finish(json);
    ct_response_json(resp, status, body);
    free(body);
}

/* CORS headers */
void ct_response_add_cors_headers(ct_response_t *resp) {
    ct_response_add_header(resp, "Access-Control-Allow-Origin", "*");
    ct_response_add_header(resp, "Access-Control-Allow-Methods", 
                          "GET, POST, PUT, DELETE, OPTIONS");
    ct_response_add_header(resp, "Access-Control-Allow-Headers", 
                          "Content-Type, Authorization");
    ct_response_add_header(resp, "Access-Control-Max-Age", "86400");
}

/* Security headers */
void ct_response_add_security_headers(ct_response_t *resp) {
    ct_response_add_header(resp, "X-Content-Type-Options", "nosniff");
    ct_response_add_header(resp, "X-Frame-Options", "SAMEORIGIN");
    ct_response_add_header(resp, "X-XSS-Protection", "1; mode=block");
    ct_response_add_header(resp, "Referrer-Policy", "strict-origin-when-cross-origin");
    ct_response_add_header(resp, "Content-Security-Policy", 
                          "default-src 'self'; script-src 'self' 'unsafe-inline' 'unsafe-eval'; "
                          "style-src 'self' 'unsafe-inline'; font-src 'self' data:; "
                          "img-src 'self' data: blob:; connect-src 'self' ws: wss:");
}