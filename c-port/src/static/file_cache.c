#include "cloudterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <zlib.h>

/* File cache entry */
typedef struct ct_file_entry {
    char *path;
    char *content;
    size_t size;
    char *content_type;
    time_t mtime;
    
    /* Compressed version */
    char *gzip_content;
    size_t gzip_size;
    
    /* Memory mapping info */
    void *mmap_addr;
    size_t mmap_size;
    
    /* LRU tracking */
    struct ct_file_entry *lru_prev;
    struct ct_file_entry *lru_next;
    
    /* Reference counting */
    _Atomic int ref_count;
} ct_file_entry_t;

/* File cache structure */
typedef struct ct_file_cache {
    ct_hash_table_t *entries;
    ct_file_entry_t *lru_head;
    ct_file_entry_t *lru_tail;
    size_t max_size;
    size_t current_size;
    _Atomic size_t hits;
    _Atomic size_t misses;
} ct_file_cache_t;

/* MIME types - perfect hash would be ideal */
static struct {
    const char *ext;
    const char *type;
} mime_types[] = {
    {".html", "text/html; charset=utf-8"},
    {".js", "application/javascript"},
    {".css", "text/css"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".ttf", "font/ttf"},
    {".txt", "text/plain"},
    {".xml", "application/xml"},
    {".pdf", "application/pdf"},
    {".zip", "application/zip"},
    {NULL, NULL}
};

/* Get MIME type from file extension */
static const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    for (int i = 0; mime_types[i].ext; i++) {
        if (strcasecmp(ext, mime_types[i].ext) == 0) {
            return mime_types[i].type;
        }
    }
    
    return "application/octet-stream";
}

/* Compress content with gzip */
static int compress_content(const char *input, size_t input_len,
                           char **output, size_t *output_len) {
    /* Allocate output buffer - worst case is input_len + headers */
    size_t max_len = compressBound(input_len);
    *output = malloc(max_len);
    if (!*output) return -1;
    
    /* Compress with gzip */
    z_stream strm = {0};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    /* Use gzip encoding */
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        free(*output);
        return -1;
    }
    
    strm.avail_in = input_len;
    strm.next_in = (unsigned char *)input;
    strm.avail_out = max_len;
    strm.next_out = (unsigned char *)*output;
    
    if (deflate(&strm, Z_FINISH) != Z_STREAM_END) {
        deflateEnd(&strm);
        free(*output);
        return -1;
    }
    
    *output_len = max_len - strm.avail_out;
    deflateEnd(&strm);
    
    /* Resize to actual size */
    char *resized = realloc(*output, *output_len);
    if (resized) *output = resized;
    
    return 0;
}

/* Create file cache */
ct_file_cache_t *ct_file_cache_create(size_t max_size) {
    ct_file_cache_t *cache = calloc(1, sizeof(ct_file_cache_t));
    if (!cache) return NULL;
    
    cache->entries = ct_hash_table_create(1024, ct_hash_fnv1a);
    if (!cache->entries) {
        free(cache);
        return NULL;
    }
    
    cache->max_size = max_size;
    atomic_init(&cache->hits, 0);
    atomic_init(&cache->misses, 0);
    
    return cache;
}

/* LRU operations */
static void lru_remove(ct_file_cache_t *cache, ct_file_entry_t *entry) {
    if (entry->lru_prev) {
        entry->lru_prev->lru_next = entry->lru_next;
    } else {
        cache->lru_head = entry->lru_next;
    }
    
    if (entry->lru_next) {
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        cache->lru_tail = entry->lru_prev;
    }
    
    entry->lru_prev = entry->lru_next = NULL;
}

static void lru_add_front(ct_file_cache_t *cache, ct_file_entry_t *entry) {
    entry->lru_prev = NULL;
    entry->lru_next = cache->lru_head;
    
    if (cache->lru_head) {
        cache->lru_head->lru_prev = entry;
    }
    cache->lru_head = entry;
    
    if (!cache->lru_tail) {
        cache->lru_tail = entry;
    }
}

/* Evict LRU entries to make space */
static void evict_lru(ct_file_cache_t *cache, size_t needed) {
    while (cache->current_size + needed > cache->max_size && cache->lru_tail) {
        ct_file_entry_t *entry = cache->lru_tail;
        
        /* Skip if still referenced */
        if (atomic_load(&entry->ref_count) > 0) {
            lru_remove(cache, entry);
            lru_add_front(cache, entry);
            continue;
        }
        
        /* Remove from cache */
        ct_hash_table_delete(cache->entries, entry->path, strlen(entry->path));
        lru_remove(cache, entry);
        
        /* Free resources */
        cache->current_size -= entry->size + entry->gzip_size;
        
        if (entry->mmap_addr) {
            munmap(entry->mmap_addr, entry->mmap_size);
        }
        
        free(entry->path);
        free(entry->content);
        free(entry->gzip_content);
        free(entry);
    }
}

/* Load file into cache */
static ct_file_entry_t *load_file(ct_file_cache_t *cache, const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        return NULL;
    }
    
    /* Check file size */
    if (st.st_size > cache->max_size / 4) {
        return NULL; /* Too large for cache */
    }
    
    ct_file_entry_t *entry = calloc(1, sizeof(ct_file_entry_t));
    if (!entry) return NULL;
    
    entry->path = strdup(path);
    entry->size = st.st_size;
    entry->mtime = st.st_mtime;
    entry->content_type = get_mime_type(path);
    atomic_init(&entry->ref_count, 1);
    
    /* Try memory mapping for large files */
    if (st.st_size > 4096) {
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            void *addr = mmap(NULL, st.st_size, PROT_READ, 
                             MAP_PRIVATE, fd, 0);
            close(fd);
            
            if (addr != MAP_FAILED) {
                entry->mmap_addr = addr;
                entry->mmap_size = st.st_size;
                entry->content = addr;
                
                /* Compress if text-based */
                if (strstr(entry->content_type, "text/") ||
                    strstr(entry->content_type, "javascript") ||
                    strstr(entry->content_type, "json")) {
                    compress_content(entry->content, entry->size,
                                   &entry->gzip_content, &entry->gzip_size);
                }
                
                return entry;
            }
        }
    }
    
    /* Fall back to regular read */
    FILE *f = fopen(path, "rb");
    if (!f) {
        free(entry->path);
        free(entry);
        return NULL;
    }
    
    entry->content = malloc(st.st_size);
    if (!entry->content || fread(entry->content, 1, st.st_size, f) != st.st_size) {
        fclose(f);
        free(entry->path);
        free(entry->content);
        free(entry);
        return NULL;
    }
    
    fclose(f);
    
    /* Compress if beneficial */
    if (st.st_size > 1024 &&
        (strstr(entry->content_type, "text/") ||
         strstr(entry->content_type, "javascript") ||
         strstr(entry->content_type, "json"))) {
        compress_content(entry->content, entry->size,
                       &entry->gzip_content, &entry->gzip_size);
    }
    
    return entry;
}

/* Get file from cache or load it */
ct_file_entry_t *ct_file_cache_get(ct_file_cache_t *cache, const char *path) {
    /* Check cache first - O(1) */
    ct_file_entry_t *entry = ct_hash_table_get(cache->entries, 
                                              path, strlen(path));
    
    if (entry) {
        /* Cache hit - move to front of LRU */
        atomic_fetch_add(&cache->hits, 1);
        atomic_fetch_add(&entry->ref_count, 1);
        
        lru_remove(cache, entry);
        lru_add_front(cache, entry);
        
        /* Check if file was modified */
        struct stat st;
        if (stat(path, &st) == 0 && st.st_mtime > entry->mtime) {
            /* File changed - reload */
            ct_hash_table_delete(cache->entries, path, strlen(path));
            lru_remove(cache, entry);
            
            if (atomic_fetch_sub(&entry->ref_count, 1) == 1) {
                /* We were the last reference */
                if (entry->mmap_addr) {
                    munmap(entry->mmap_addr, entry->mmap_size);
                }
                free(entry->path);
                free(entry->content);
                free(entry->gzip_content);
                free(entry);
            }
            
            entry = NULL;
        }
    }
    
    if (!entry) {
        /* Cache miss - load file */
        atomic_fetch_add(&cache->misses, 1);
        
        entry = load_file(cache, path);
        if (!entry) return NULL;
        
        /* Make space if needed */
        size_t needed = entry->size + entry->gzip_size;
        evict_lru(cache, needed);
        
        /* Add to cache */
        ct_hash_table_set(cache->entries, entry->path, 
                         strlen(entry->path), entry);
        lru_add_front(cache, entry);
        cache->current_size += needed;
    }
    
    return entry;
}

/* Release file reference */
void ct_file_cache_release(ct_file_cache_t *cache, ct_file_entry_t *entry) {
    if (!entry) return;
    atomic_fetch_sub(&entry->ref_count, 1);
}

/* Get cache statistics */
void ct_file_cache_stats(ct_file_cache_t *cache, size_t *hits, size_t *misses,
                        size_t *size, size_t *count) {
    *hits = atomic_load(&cache->hits);
    *misses = atomic_load(&cache->misses);
    *size = cache->current_size;
    *count = cache->entries->count;
}

/* Destroy file cache */
void ct_file_cache_destroy(ct_file_cache_t *cache) {
    if (!cache) return;
    
    /* Free all entries */
    ct_file_entry_t *entry = cache->lru_head;
    while (entry) {
        ct_file_entry_t *next = entry->lru_next;
        
        if (entry->mmap_addr) {
            munmap(entry->mmap_addr, entry->mmap_size);
        }
        
        free(entry->path);
        if (!entry->mmap_addr) {
            free(entry->content);
        }
        free(entry->gzip_content);
        free(entry);
        
        entry = next;
    }
    
    ct_hash_table_destroy(cache->entries);
    free(cache);
}