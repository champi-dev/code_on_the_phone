#include "cloudterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <errno.h>

/* Directory index files */
static const char *index_files[] = {
    "index.html",
    "index.htm",
    NULL
};

/* Security check - prevent directory traversal */
static bool is_safe_path(const char *path) {
    /* Must not contain .. */
    if (strstr(path, "..")) return false;
    
    /* Must not be absolute */
    if (path[0] == '/') return false;
    
    /* Must not contain double slashes */
    if (strstr(path, "//")) return false;
    
    return true;
}

/* Normalize path - remove trailing slashes, add index if directory */
static int normalize_path(const char *base_dir, const char *url_path, 
                         char *full_path, size_t path_len) {
    /* Security check */
    if (!is_safe_path(url_path)) {
        return -1;
    }
    
    /* Skip leading slash */
    if (url_path[0] == '/') url_path++;
    
    /* Build full path */
    snprintf(full_path, path_len, "%s/%s", base_dir, url_path);
    
    /* Check if it's a directory */
    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        /* Try index files */
        for (int i = 0; index_files[i]; i++) {
            char index_path[CT_MAX_PATH_LEN];
            snprintf(index_path, sizeof(index_path), "%s/%s", 
                    full_path, index_files[i]);
            
            if (stat(index_path, &st) == 0 && S_ISREG(st.st_mode)) {
                strcpy(full_path, index_path);
                return 0;
            }
        }
        
        /* No index file found */
        return -1;
    }
    
    return 0;
}

/* Serve static file using zero-copy sendfile */
int ct_serve_static_file(ct_connection_t *conn, const char *base_dir,
                        const char *url_path) {
    char full_path[CT_MAX_PATH_LEN];
    
    /* Normalize and validate path */
    if (normalize_path(base_dir, url_path, full_path, sizeof(full_path)) < 0) {
        ct_response_init(&conn->response, 404, "Not Found");
        ct_response_html(&conn->response, 404, 
                        "<html><body><h1>404 Not Found</h1></body></html>");
        return 0;
    }
    
    /* Get file from cache */
    ct_file_cache_t *cache = conn->server->file_cache;
    ct_file_entry_t *entry = ct_file_cache_get(cache, full_path);
    
    if (!entry) {
        ct_response_init(&conn->response, 404, "Not Found");
        ct_response_html(&conn->response, 404,
                        "<html><body><h1>404 Not Found</h1></body></html>");
        return 0;
    }
    
    /* Check if client accepts gzip */
    bool accepts_gzip = false;
    const char *accept_encoding = ct_request_get_header(&conn->request, 
                                                       "Accept-Encoding");
    if (accept_encoding && strstr(accept_encoding, "gzip") && 
        entry->gzip_content) {
        accepts_gzip = true;
    }
    
    /* Build response */
    ct_response_init(&conn->response, 200, "OK");
    ct_response_add_header(&conn->response, "Content-Type", entry->content_type);
    
    /* Cache headers */
    ct_response_add_header(&conn->response, "Cache-Control", 
                          "public, max-age=3600");
    
    char etag[64];
    snprintf(etag, sizeof(etag), "\"%lx-%lx\"", 
            (long)entry->mtime, (long)entry->size);
    ct_response_add_header(&conn->response, "ETag", etag);
    
    /* Check if-none-match */
    const char *if_none_match = ct_request_get_header(&conn->request, 
                                                     "If-None-Match");
    if (if_none_match && strcmp(if_none_match, etag) == 0) {
        ct_file_cache_release(cache, entry);
        ct_response_init(&conn->response, 304, "Not Modified");
        return 0;
    }
    
    /* Use gzipped version if available and accepted */
    if (accepts_gzip && entry->gzip_content) {
        ct_response_add_header(&conn->response, "Content-Encoding", "gzip");
        conn->response.body = entry->gzip_content;
        conn->response.body_len = entry->gzip_size;
    } else {
        conn->response.body = entry->content;
        conn->response.body_len = entry->size;
    }
    
    /* Keep reference until response is sent */
    conn->file_entry = entry;
    
    return 0;
}

/* Serve directory listing (optional) */
int ct_serve_directory(ct_connection_t *conn, const char *base_dir,
                      const char *url_path) {
    /* For security, directory listing is disabled by default */
    ct_response_init(&conn->response, 403, "Forbidden");
    ct_response_html(&conn->response, 403,
                    "<html><body><h1>403 Forbidden</h1>"
                    "<p>Directory listing is not allowed.</p></body></html>");
    return 0;
}

/* Handle range requests for large files */
int ct_serve_range_request(ct_connection_t *conn, ct_file_entry_t *entry) {
    const char *range_header = ct_request_get_header(&conn->request, "Range");
    if (!range_header || strncmp(range_header, "bytes=", 6) != 0) {
        return -1; /* Not a range request */
    }
    
    /* Parse range header */
    long start = 0, end = entry->size - 1;
    sscanf(range_header + 6, "%ld-%ld", &start, &end);
    
    if (start < 0 || start >= entry->size || end >= entry->size || start > end) {
        ct_response_init(&conn->response, 416, "Range Not Satisfiable");
        return 0;
    }
    
    /* Build partial content response */
    ct_response_init(&conn->response, 206, "Partial Content");
    ct_response_add_header(&conn->response, "Content-Type", entry->content_type);
    ct_response_add_header(&conn->response, "Accept-Ranges", "bytes");
    
    char content_range[128];
    snprintf(content_range, sizeof(content_range), "bytes %ld-%ld/%zu",
            start, end, entry->size);
    ct_response_add_header(&conn->response, "Content-Range", content_range);
    
    conn->response.body = entry->content + start;
    conn->response.body_len = end - start + 1;
    
    return 0;
}

/* Fast path for small files */
int ct_serve_small_file(ct_connection_t *conn, const char *path,
                       const char *content_type, const char *content,
                       size_t size) {
    ct_response_init(&conn->response, 200, "OK");
    ct_response_add_header(&conn->response, "Content-Type", content_type);
    ct_response_add_header(&conn->response, "Cache-Control", 
                          "public, max-age=86400");
    
    conn->response.body = content;
    conn->response.body_len = size;
    
    return 0;
}

/* Embedded files for offline fallback */
static const char *offline_page = 
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "    <title>Offline</title>\n"
    "    <style>\n"
    "        body { font-family: system-ui; text-align: center; padding: 50px; }\n"
    "        h1 { color: #e74c3c; }\n"
    "    </style>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>Offline</h1>\n"
    "    <p>Unable to connect to the server.</p>\n"
    "</body>\n"
    "</html>\n";

int ct_serve_offline_page(ct_connection_t *conn) {
    return ct_serve_small_file(conn, "/offline.html", "text/html", 
                              offline_page, strlen(offline_page));
}