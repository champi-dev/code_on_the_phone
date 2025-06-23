#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

/* HTTP method strings - ordered by frequency for performance */
static const char *method_strings[] = {
    "GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "CONNECT"
};

/* Common HTTP headers - perfect hash would be ideal here */
static const char *common_headers[] = {
    "Host", "Connection", "Content-Length", "Content-Type",
    "User-Agent", "Accept", "Cookie", "Upgrade", "Sec-WebSocket-Key",
    "Sec-WebSocket-Version", "Sec-WebSocket-Protocol"
};

/* Fast method parsing using prefix matching */
static ct_http_method_t parse_method(const char *data, size_t len) {
    if (len < 3) return CT_METHOD_UNKNOWN;
    
    /* Optimize for common case (GET) */
    if (len >= 3 && data[0] == 'G' && data[1] == 'E' && data[2] == 'T') {
        return CT_METHOD_GET;
    }
    
    /* Check other methods */
    for (int i = 1; i < 7; i++) {
        size_t method_len = strlen(method_strings[i]);
        if (len >= method_len && memcmp(data, method_strings[i], method_len) == 0) {
            return (ct_http_method_t)i;
        }
    }
    
    return CT_METHOD_UNKNOWN;
}

/* Find end of line (CRLF) - optimized with SIMD when available */
static const char *find_crlf(const char *data, size_t len) {
    const char *end = data + len;
    
    /* Simple implementation - can be optimized with SIMD */
    while (data < end - 1) {
        if (data[0] == '\r' && data[1] == '\n') {
            return data;
        }
        data++;
    }
    
    return NULL;
}

/* Parse URL and extract path/query - zero-copy */
int ct_parse_url(const char *url, char *path, size_t path_len, 
                 char *query, size_t query_len) {
    const char *p = url;
    const char *path_start = url;
    const char *path_end = NULL;
    const char *query_start = NULL;
    
    /* Find end of path and start of query */
    while (*p && *p != ' ' && *p != '\r' && *p != '\n') {
        if (*p == '?') {
            path_end = p;
            query_start = p + 1;
            break;
        }
        p++;
    }
    
    if (!path_end) {
        path_end = p;
    }
    
    /* Copy path */
    size_t plen = path_end - path_start;
    if (plen >= path_len) plen = path_len - 1;
    memcpy(path, path_start, plen);
    path[plen] = '\0';
    
    /* Copy query if present */
    if (query && query_len > 0) {
        if (query_start) {
            size_t qlen = p - query_start;
            if (qlen >= query_len) qlen = query_len - 1;
            memcpy(query, query_start, qlen);
            query[qlen] = '\0';
        } else {
            query[0] = '\0';
        }
    }
    
    return 0;
}

/* Zero-copy HTTP request parser with state machine */
int ct_parse_request(ct_request_t *req, const char *data, size_t len) {
    const char *p = data;
    const char *end = data + len;
    const char *line_end;
    
    /* Already completed */
    if (req->parse_state == CT_PARSE_COMPLETE) {
        return 0;
    }
    
    /* Parse request line if not done */
    if (req->parse_state == CT_PARSE_METHOD) {
        line_end = find_crlf(p, end - p);
        if (!line_end) {
            return -1; /* Need more data */
        }
        
        /* Parse method */
        const char *space = memchr(p, ' ', line_end - p);
        if (!space) {
            req->parse_state = CT_PARSE_ERROR;
            return -2;
        }
        
        req->method = parse_method(p, space - p);
        if (req->method == CT_METHOD_UNKNOWN) {
            req->parse_state = CT_PARSE_ERROR;
            return -2;
        }
        
        /* Parse URL */
        p = space + 1;
        space = memchr(p, ' ', line_end - p);
        if (!space) {
            req->parse_state = CT_PARSE_ERROR;
            return -2;
        }
        
        req->url = p;
        
        /* Parse version */
        p = space + 1;
        req->version = p;
        
        p = line_end + 2; /* Skip CRLF */
        req->parse_state = CT_PARSE_HEADER_NAME;
    }
    
    /* Parse headers */
    while (req->parse_state == CT_PARSE_HEADER_NAME && p < end) {
        line_end = find_crlf(p, end - p);
        if (!line_end) {
            return -1; /* Need more data */
        }
        
        /* Empty line = end of headers */
        if (line_end == p) {
            p += 2; /* Skip CRLF */
            req->parse_state = CT_PARSE_BODY;
            
            /* Check for WebSocket upgrade */
            for (size_t i = 0; i < req->header_count; i++) {
                if (strcasecmp(req->headers[i].name, "Upgrade") == 0 &&
                    strcasecmp(req->headers[i].value, "websocket") == 0) {
                    req->is_websocket = true;
                }
                if (strcasecmp(req->headers[i].name, "Connection") == 0) {
                    if (strstr(req->headers[i].value, "keep-alive")) {
                        req->keep_alive = true;
                    }
                }
            }
            break;
        }
        
        /* Parse header name:value */
        const char *colon = memchr(p, ':', line_end - p);
        if (!colon || req->header_count >= CT_MAX_HEADERS) {
            req->parse_state = CT_PARSE_ERROR;
            return -2;
        }
        
        /* Store header pointers (zero-copy) */
        req->headers[req->header_count].name = p;
        req->headers[req->header_count].name_len = colon - p;
        
        /* Skip colon and whitespace */
        p = colon + 1;
        while (p < line_end && (*p == ' ' || *p == '\t')) p++;
        
        req->headers[req->header_count].value = p;
        req->headers[req->header_count].value_len = line_end - p;
        
        req->header_count++;
        p = line_end + 2; /* Skip CRLF */
    }
    
    /* Parse body if needed */
    if (req->parse_state == CT_PARSE_BODY) {
        /* Find Content-Length header */
        size_t content_length = 0;
        for (size_t i = 0; i < req->header_count; i++) {
            if (strncasecmp(req->headers[i].name, "Content-Length", 
                           req->headers[i].name_len) == 0) {
                content_length = strtoul(req->headers[i].value, NULL, 10);
                break;
            }
        }
        
        if (content_length > 0) {
            size_t body_available = end - p;
            if (body_available < content_length) {
                return -1; /* Need more data */
            }
            
            req->body = p;
            req->body_len = content_length;
            p += content_length;
        }
        
        req->parse_state = CT_PARSE_COMPLETE;
    }
    
    return p - data; /* Return bytes consumed */
}

/* Build HTTP response - optimized for common cases */
int ct_build_response(ct_response_t *resp, char *buf, size_t buf_len) {
    char *p = buf;
    char *end = buf + buf_len;
    
    /* Status line */
    int n = snprintf(p, end - p, "HTTP/1.1 %d %s\r\n", 
                     resp->status_code, resp->status_text);
    if (n < 0 || n >= end - p) return -1;
    p += n;
    
    /* Headers */
    for (size_t i = 0; i < resp->header_count; i++) {
        n = snprintf(p, end - p, "%s: %s\r\n",
                     resp->headers[i].name, resp->headers[i].value);
        if (n < 0 || n >= end - p) return -1;
        p += n;
    }
    
    /* Content-Length if not chunked */
    if (!resp->chunked && resp->body_len > 0) {
        n = snprintf(p, end - p, "Content-Length: %zu\r\n", resp->body_len);
        if (n < 0 || n >= end - p) return -1;
        p += n;
    }
    
    /* End of headers */
    if (p + 2 > end) return -1;
    *p++ = '\r';
    *p++ = '\n';
    
    /* Body */
    if (resp->body && resp->body_len > 0) {
        if (p + resp->body_len > end) return -1;
        memcpy(p, resp->body, resp->body_len);
        p += resp->body_len;
    }
    
    return p - buf;
}

/* Find header value by name - O(n) but typically small n */
const char *ct_request_get_header(ct_request_t *req, const char *name) {
    size_t name_len = strlen(name);
    
    for (size_t i = 0; i < req->header_count; i++) {
        if (req->headers[i].name_len == name_len &&
            strncasecmp(req->headers[i].name, name, name_len) == 0) {
            return req->headers[i].value;
        }
    }
    
    return NULL;
}

/* Add response header */
int ct_response_add_header(ct_response_t *resp, const char *name, 
                          const char *value) {
    if (resp->header_count >= CT_MAX_HEADERS) {
        return -1;
    }
    
    resp->headers[resp->header_count].name = name;
    resp->headers[resp->header_count].value = value;
    resp->headers[resp->header_count].name_len = strlen(name);
    resp->headers[resp->header_count].value_len = strlen(value);
    resp->header_count++;
    
    return 0;
}

/* Quick response builders for common cases */
void ct_response_init(ct_response_t *resp, int status_code, 
                     const char *status_text) {
    memset(resp, 0, sizeof(ct_response_t));
    resp->status_code = status_code;
    resp->status_text = status_text;
}

void ct_response_json(ct_response_t *resp, int status_code, 
                     const char *json_body) {
    ct_response_init(resp, status_code, 
                    status_code == 200 ? "OK" : "Error");
    ct_response_add_header(resp, "Content-Type", "application/json");
    resp->body = json_body;
    resp->body_len = strlen(json_body);
}

void ct_response_html(ct_response_t *resp, int status_code, 
                     const char *html_body) {
    ct_response_init(resp, status_code,
                    status_code == 200 ? "OK" : "Error");
    ct_response_add_header(resp, "Content-Type", "text/html; charset=utf-8");
    resp->body = html_body;
    resp->body_len = strlen(html_body);
}