#include "terminal.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <assert.h>

/* Ensure power of 2 for efficient modulo operation */
static inline size_t next_power_of_2(size_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;
    return n;
}

ct_ring_buffer_t *ct_ring_buffer_create(size_t size) {
    /* Ensure size is power of 2 for fast modulo */
    size = next_power_of_2(size);
    
    ct_ring_buffer_t *rb = calloc(1, sizeof(ct_ring_buffer_t));
    if (!rb) return NULL;
    
    rb->data = calloc(1, size);
    if (!rb->data) {
        free(rb);
        return NULL;
    }
    
    rb->size = size;
    atomic_init(&rb->read_pos, 0);
    atomic_init(&rb->write_pos, 0);
    
    return rb;
}

void ct_ring_buffer_destroy(ct_ring_buffer_t *rb) {
    if (!rb) return;
    free(rb->data);
    free(rb);
}

size_t ct_ring_buffer_available(ct_ring_buffer_t *rb) {
    if (!rb) return 0;
    
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    
    return (write_pos - read_pos) & (rb->size - 1);
}

size_t ct_ring_buffer_free_space(ct_ring_buffer_t *rb) {
    if (!rb) return 0;
    return rb->size - ct_ring_buffer_available(rb) - 1;
}

size_t ct_ring_buffer_write(ct_ring_buffer_t *rb, const char *data, size_t len) {
    if (!rb || !data || len == 0) return 0;
    
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_relaxed);
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    
    /* Calculate available space */
    size_t free_space = (read_pos - write_pos - 1) & (rb->size - 1);
    if (free_space == 0) return 0;
    
    /* Limit write to available space */
    size_t to_write = (len < free_space) ? len : free_space;
    
    /* Calculate wrap point */
    size_t write_idx = write_pos & (rb->size - 1);
    size_t first_part = rb->size - write_idx;
    
    if (to_write <= first_part) {
        /* No wrap needed - single memcpy */
        memcpy(rb->data + write_idx, data, to_write);
    } else {
        /* Handle wrap - two memcpy operations */
        memcpy(rb->data + write_idx, data, first_part);
        memcpy(rb->data, data + first_part, to_write - first_part);
    }
    
    /* Update write position atomically */
    atomic_store_explicit(&rb->write_pos, write_pos + to_write, 
                         memory_order_release);
    
    return to_write;
}

size_t ct_ring_buffer_read(ct_ring_buffer_t *rb, char *data, size_t len) {
    if (!rb || !data || len == 0) return 0;
    
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_relaxed);
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    
    /* Calculate available data */
    size_t available = (write_pos - read_pos) & (rb->size - 1);
    if (available == 0) return 0;
    
    /* Limit read to available data */
    size_t to_read = (len < available) ? len : available;
    
    /* Calculate wrap point */
    size_t read_idx = read_pos & (rb->size - 1);
    size_t first_part = rb->size - read_idx;
    
    if (to_read <= first_part) {
        /* No wrap needed - single memcpy */
        memcpy(data, rb->data + read_idx, to_read);
    } else {
        /* Handle wrap - two memcpy operations */
        memcpy(data, rb->data + read_idx, first_part);
        memcpy(data + first_part, rb->data, to_read - first_part);
    }
    
    /* Update read position atomically */
    atomic_store_explicit(&rb->read_pos, read_pos + to_read, 
                         memory_order_release);
    
    return to_read;
}

/* Peek at data without consuming it */
size_t ct_ring_buffer_peek(ct_ring_buffer_t *rb, char *data, size_t len) {
    if (!rb || !data || len == 0) return 0;
    
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    
    /* Calculate available data */
    size_t available = (write_pos - read_pos) & (rb->size - 1);
    if (available == 0) return 0;
    
    /* Limit peek to available data */
    size_t to_peek = (len < available) ? len : available;
    
    /* Calculate wrap point */
    size_t read_idx = read_pos & (rb->size - 1);
    size_t first_part = rb->size - read_idx;
    
    if (to_peek <= first_part) {
        /* No wrap needed - single memcpy */
        memcpy(data, rb->data + read_idx, to_peek);
    } else {
        /* Handle wrap - two memcpy operations */
        memcpy(data, rb->data + read_idx, first_part);
        memcpy(data + first_part, rb->data, to_peek - first_part);
    }
    
    return to_peek;
}

/* Discard data without reading */
size_t ct_ring_buffer_skip(ct_ring_buffer_t *rb, size_t len) {
    if (!rb || len == 0) return 0;
    
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_relaxed);
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    
    /* Calculate available data */
    size_t available = (write_pos - read_pos) & (rb->size - 1);
    if (available == 0) return 0;
    
    /* Limit skip to available data */
    size_t to_skip = (len < available) ? len : available;
    
    /* Update read position atomically */
    atomic_store_explicit(&rb->read_pos, read_pos + to_skip, 
                         memory_order_release);
    
    return to_skip;
}