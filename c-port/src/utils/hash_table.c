#include "terminal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* FNV-1a hash function - fast and good distribution */
uint32_t ct_hash_fnv1a(const void *key, size_t len) {
    const uint8_t *data = (const uint8_t *)key;
    uint32_t hash = 2166136261u;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    
    return hash;
}

/* MurmurHash3 - excellent performance and distribution */
uint32_t ct_hash_murmur3(const void *key, size_t len) {
    const uint8_t *data = (const uint8_t *)key;
    const int nblocks = len / 4;
    uint32_t h1 = 0x811c9dc5;
    
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    
    /* Process 4-byte blocks */
    const uint32_t *blocks = (const uint32_t *)data;
    for (int i = 0; i < nblocks; i++) {
        uint32_t k1 = blocks[i];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        
        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }
    
    /* Process remaining bytes */
    const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);
    uint32_t k1 = 0;
    
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16; /* fall through */
        case 2: k1 ^= tail[1] << 8;  /* fall through */
        case 1: k1 ^= tail[0];
            k1 *= c1;
            k1 = (k1 << 15) | (k1 >> 17);
            k1 *= c2;
            h1 ^= k1;
    }
    
    /* Finalization */
    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    
    return h1;
}

typedef struct ct_hash_entry {
    void *key;
    size_t key_len;
    void *value;
    struct ct_hash_entry *next;
} ct_hash_entry_t;

ct_hash_table_t *ct_hash_table_create(size_t size, 
                                      uint32_t (*hash_func)(const void *, size_t)) {
    assert(size > 0 && (size & (size - 1)) == 0); /* Must be power of 2 */
    
    ct_hash_table_t *ht = calloc(1, sizeof(ct_hash_table_t));
    if (!ht) return NULL;
    
    ht->buckets = calloc(size, sizeof(void *));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }
    
    ht->size = size;
    ht->count = 0;
    ht->hash_func = hash_func ? hash_func : ct_hash_murmur3;
    
    return ht;
}

void ct_hash_table_destroy(ct_hash_table_t *ht) {
    if (!ht) return;
    
    /* Free all entries */
    for (size_t i = 0; i < ht->size; i++) {
        ct_hash_entry_t *entry = (ct_hash_entry_t *)ht->buckets[i];
        while (entry) {
            ct_hash_entry_t *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    
    free(ht->buckets);
    free(ht);
}

void *ct_hash_table_get(ct_hash_table_t *ht, const void *key, size_t key_len) {
    if (!ht || !key || key_len == 0) return NULL;
    
    uint32_t hash = ht->hash_func(key, key_len);
    size_t index = hash & (ht->size - 1);
    
    ct_hash_entry_t *entry = (ct_hash_entry_t *)ht->buckets[index];
    
    /* O(1) average case - walk chain */
    while (entry) {
        if (entry->key_len == key_len && 
            memcmp(entry->key, key, key_len) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    
    return NULL;
}

void ct_hash_table_set(ct_hash_table_t *ht, const void *key, size_t key_len, 
                       void *value) {
    if (!ht || !key || key_len == 0) return;
    
    uint32_t hash = ht->hash_func(key, key_len);
    size_t index = hash & (ht->size - 1);
    
    ct_hash_entry_t *entry = (ct_hash_entry_t *)ht->buckets[index];
    
    /* Check if key exists */
    while (entry) {
        if (entry->key_len == key_len && 
            memcmp(entry->key, key, key_len) == 0) {
            /* Update existing entry */
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    /* Create new entry */
    ct_hash_entry_t *new_entry = malloc(sizeof(ct_hash_entry_t));
    if (!new_entry) return;
    
    new_entry->key = malloc(key_len);
    if (!new_entry->key) {
        free(new_entry);
        return;
    }
    
    memcpy(new_entry->key, key, key_len);
    new_entry->key_len = key_len;
    new_entry->value = value;
    
    /* Insert at head of chain - O(1) */
    new_entry->next = (ct_hash_entry_t *)ht->buckets[index];
    ht->buckets[index] = new_entry;
    ht->count++;
}

void ct_hash_table_delete(ct_hash_table_t *ht, const void *key, size_t key_len) {
    if (!ht || !key || key_len == 0) return;
    
    uint32_t hash = ht->hash_func(key, key_len);
    size_t index = hash & (ht->size - 1);
    
    ct_hash_entry_t *entry = (ct_hash_entry_t *)ht->buckets[index];
    ct_hash_entry_t *prev = NULL;
    
    while (entry) {
        if (entry->key_len == key_len && 
            memcmp(entry->key, key, key_len) == 0) {
            /* Remove from chain */
            if (prev) {
                prev->next = entry->next;
            } else {
                ht->buckets[index] = entry->next;
            }
            
            free(entry->key);
            free(entry);
            ht->count--;
            return;
        }
        
        prev = entry;
        entry = entry->next;
    }
}

/* Iterate over all entries - useful for cleanup */
void ct_hash_table_foreach(ct_hash_table_t *ht, 
                          void (*callback)(void *key, size_t key_len, 
                                         void *value, void *ctx),
                          void *ctx) {
    if (!ht || !callback) return;
    
    for (size_t i = 0; i < ht->size; i++) {
        ct_hash_entry_t *entry = (ct_hash_entry_t *)ht->buckets[i];
        while (entry) {
            ct_hash_entry_t *next = entry->next;
            callback(entry->key, entry->key_len, entry->value, ctx);
            entry = next;
        }
    }
}