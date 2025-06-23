#include "terminal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct ct_mem_chunk {
    struct ct_mem_chunk *next;
} ct_mem_chunk_t;

typedef struct ct_mem_block {
    struct ct_mem_block *next;
    char data[];
} ct_mem_block_t;

ct_mem_pool_t *ct_mem_pool_create(size_t chunk_size, size_t initial_chunks) {
    assert(chunk_size >= sizeof(ct_mem_chunk_t));
    assert(initial_chunks > 0);
    
    ct_mem_pool_t *pool = calloc(1, sizeof(ct_mem_pool_t));
    if (!pool) return NULL;
    
    pool->chunk_size = chunk_size;
    pool->total_chunks = initial_chunks;
    pool->free_chunks = initial_chunks;
    
    /* Allocate initial block */
    size_t block_size = sizeof(ct_mem_block_t) + (chunk_size * initial_chunks);
    ct_mem_block_t *block = malloc(block_size);
    if (!block) {
        free(pool);
        return NULL;
    }
    
    block->next = NULL;
    pool->chunks = block;
    
    /* Initialize free list - O(n) but only done once */
    char *ptr = block->data;
    ct_mem_chunk_t *prev = NULL;
    
    for (size_t i = 0; i < initial_chunks; i++) {
        ct_mem_chunk_t *chunk = (ct_mem_chunk_t *)ptr;
        chunk->next = prev;
        prev = chunk;
        ptr += chunk_size;
    }
    
    pool->free_list = prev;
    
    return pool;
}

void ct_mem_pool_destroy(ct_mem_pool_t *pool) {
    if (!pool) return;
    
    ct_mem_block_t *block = (ct_mem_block_t *)pool->chunks;
    while (block) {
        ct_mem_block_t *next = block->next;
        free(block);
        block = next;
    }
    
    free(pool);
}

void *ct_mem_pool_alloc(ct_mem_pool_t *pool) {
    if (!pool) return NULL;
    
    /* Fast path - O(1) allocation from free list */
    if (pool->free_list) {
        ct_mem_chunk_t *chunk = (ct_mem_chunk_t *)pool->free_list;
        pool->free_list = chunk->next;
        pool->free_chunks--;
        
        /* Clear the memory for security */
        memset(chunk, 0, pool->chunk_size);
        return chunk;
    }
    
    /* Slow path - allocate new block */
    size_t new_chunks = pool->total_chunks;
    size_t block_size = sizeof(ct_mem_block_t) + (pool->chunk_size * new_chunks);
    ct_mem_block_t *block = malloc(block_size);
    if (!block) return NULL;
    
    /* Link new block */
    block->next = (ct_mem_block_t *)pool->chunks;
    pool->chunks = block;
    pool->total_chunks += new_chunks;
    pool->free_chunks += new_chunks - 1; /* -1 because we'll return one */
    
    /* Initialize new free chunks */
    char *ptr = block->data + pool->chunk_size; /* Skip first chunk */
    ct_mem_chunk_t *prev = NULL;
    
    for (size_t i = 1; i < new_chunks; i++) {
        ct_mem_chunk_t *chunk = (ct_mem_chunk_t *)ptr;
        chunk->next = prev;
        prev = chunk;
        ptr += pool->chunk_size;
    }
    
    pool->free_list = prev;
    
    /* Return first chunk from new block */
    memset(block->data, 0, pool->chunk_size);
    return block->data;
}

void ct_mem_pool_free(ct_mem_pool_t *pool, void *ptr) {
    if (!pool || !ptr) return;
    
    /* O(1) return to free list */
    ct_mem_chunk_t *chunk = (ct_mem_chunk_t *)ptr;
    chunk->next = (ct_mem_chunk_t *)pool->free_list;
    pool->free_list = chunk;
    pool->free_chunks++;
}