#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __linux__
#include <sys/sendfile.h>
#ifndef SPLICE_F_MOVE
#define SPLICE_F_MOVE 1
#endif
#ifndef SPLICE_F_NONBLOCK
#define SPLICE_F_NONBLOCK 2
#endif
#endif

/* Zero-copy data transfer between file descriptors */
ssize_t ct_splice(int fd_in, int fd_out, size_t len) {
#ifdef __linux__
    /* Linux splice - true zero-copy */
    return splice(fd_in, NULL, fd_out, NULL, len, 
                  SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
#else
    /* Fallback implementation */
    static __thread char buffer[65536];
    
    ssize_t n = read(fd_in, buffer, (len < sizeof(buffer)) ? len : sizeof(buffer));
    if (n <= 0) return n;
    
    ssize_t written = 0;
    while (written < n) {
        ssize_t w = write(fd_out, buffer + written, n - written);
        if (w < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return written > 0 ? written : -1;
            }
            return -1;
        }
        written += w;
    }
    
    return written;
#endif
}

/* Zero-copy file to socket transfer */
ssize_t ct_sendfile(int out_fd, int in_fd, off_t *offset, size_t count) {
#ifdef __linux__
    /* Linux sendfile */
    return sendfile(out_fd, in_fd, offset, count);
#elif defined(__APPLE__) || defined(__FreeBSD__)
    /* macOS/FreeBSD sendfile */
    off_t len = count;
    int ret = sendfile(in_fd, out_fd, offset ? *offset : 0, &len, NULL, 0);
    if (ret == 0 || (ret < 0 && errno == EAGAIN)) {
        if (offset) *offset += len;
        return len;
    }
    return -1;
#else
    /* Generic fallback */
    static __thread char buffer[65536];
    
    if (offset && lseek(in_fd, *offset, SEEK_SET) < 0) {
        return -1;
    }
    
    size_t total = 0;
    while (total < count) {
        size_t to_read = count - total;
        if (to_read > sizeof(buffer)) to_read = sizeof(buffer);
        
        ssize_t n = read(in_fd, buffer, to_read);
        if (n <= 0) break;
        
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(out_fd, buffer + written, n - written);
            if (w < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    total += written;
                    if (offset) *offset += total;
                    return total > 0 ? total : -1;
                }
                return -1;
            }
            written += w;
        }
        
        total += written;
    }
    
    if (offset) *offset += total;
    return total;
#endif
}

/* Optimized memory copy using platform-specific features */
void ct_fast_memcpy(void *dst, const void *src, size_t n) {
    /* For small copies, use regular memcpy */
    if (n < 1024) {
        memcpy(dst, src, n);
        return;
    }
    
#ifdef __x86_64__
    /* Use non-temporal stores for large copies to avoid cache pollution */
    if (n > 524288) { /* 512KB threshold */
        size_t i;
        const char *s = src;
        char *d = dst;
        
        /* Align destination to 16 bytes */
        size_t align = (16 - ((uintptr_t)d & 15)) & 15;
        if (align) {
            memcpy(d, s, align);
            d += align;
            s += align;
            n -= align;
        }
        
        /* Copy 64 bytes at a time using non-temporal stores */
        for (i = 0; i + 63 < n; i += 64) {
            __asm__ __volatile__ (
                "movdqu   (%1), %%xmm0\n"
                "movdqu 16(%1), %%xmm1\n"
                "movdqu 32(%1), %%xmm2\n"
                "movdqu 48(%1), %%xmm3\n"
                "movntdq %%xmm0,   (%0)\n"
                "movntdq %%xmm1, 16(%0)\n"
                "movntdq %%xmm2, 32(%0)\n"
                "movntdq %%xmm3, 48(%0)\n"
                : : "r" (d + i), "r" (s + i) : "memory"
            );
        }
        
        /* Fence to ensure completion */
        __asm__ __volatile__ ("sfence" : : : "memory");
        
        /* Copy remainder */
        if (i < n) {
            memcpy(d + i, s + i, n - i);
        }
    } else {
        memcpy(dst, src, n);
    }
#else
    /* Fallback to standard memcpy */
    memcpy(dst, src, n);
#endif
}

/* Pipe buffer for splice operations */
typedef struct {
    int pipe_fd[2];
    bool initialized;
} ct_pipe_buffer_t;

static __thread ct_pipe_buffer_t pipe_buffer = {.initialized = false};

/* Get thread-local pipe for splice operations */
static int *get_splice_pipe(void) {
#ifdef __linux__
    if (!pipe_buffer.initialized) {
        if (pipe2(pipe_buffer.pipe_fd, O_NONBLOCK) < 0) {
            return NULL;
        }
        
        /* Increase pipe buffer size for better performance */
        int pipe_size = 1048576; /* 1MB */
        fcntl(pipe_buffer.pipe_fd[0], F_SETPIPE_SZ, pipe_size);
        
        pipe_buffer.initialized = true;
    }
    
    return pipe_buffer.pipe_fd;
#else
    return NULL;
#endif
}

/* Bidirectional zero-copy proxy */
int ct_proxy_splice_loop(int fd1, int fd2) {
#ifdef __linux__
    int *pipe_fd = get_splice_pipe();
    if (!pipe_fd) return -1;
    
    /* Set both sockets to non-blocking */
    fcntl(fd1, F_SETFL, fcntl(fd1, F_GETFL) | O_NONBLOCK);
    fcntl(fd2, F_SETFL, fcntl(fd2, F_GETFL) | O_NONBLOCK);
    
    size_t total_transferred = 0;
    
    while (1) {
        ssize_t n;
        
        /* Transfer fd1 -> fd2 */
        n = splice(fd1, NULL, pipe_fd[1], NULL, 65536, 
                   SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
        if (n > 0) {
            ssize_t m = splice(pipe_fd[0], NULL, fd2, NULL, n,
                              SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
            if (m < 0 && errno != EAGAIN) return -1;
            if (m > 0) total_transferred += m;
        } else if (n == 0) {
            return 0; /* EOF */
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            return -1;
        }
        
        /* Transfer fd2 -> fd1 */
        n = splice(fd2, NULL, pipe_fd[1], NULL, 65536,
                   SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
        if (n > 0) {
            ssize_t m = splice(pipe_fd[0], NULL, fd1, NULL, n,
                              SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
            if (m < 0 && errno != EAGAIN) return -1;
            if (m > 0) total_transferred += m;
        } else if (n == 0) {
            return 0; /* EOF */
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            return -1;
        }
        
        /* If no data transferred in this iteration, wait for events */
        if (total_transferred == 0) {
            break;
        }
        total_transferred = 0;
    }
    
    return 1; /* Would block */
#else
    /* Fallback implementation */
    char buffer[8192];
    ssize_t n;
    
    /* Try fd1 -> fd2 */
    n = read(fd1, buffer, sizeof(buffer));
    if (n > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(fd2, buffer + written, n - written);
            if (w < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                return -1;
            }
            written += w;
        }
    } else if (n == 0) {
        return 0; /* EOF */
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        return -1;
    }
    
    /* Try fd2 -> fd1 */
    n = read(fd2, buffer, sizeof(buffer));
    if (n > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(fd1, buffer + written, n - written);
            if (w < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                return -1;
            }
            written += w;
        }
    } else if (n == 0) {
        return 0; /* EOF */
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        return -1;
    }
    
    return 1; /* Would block */
#endif
}