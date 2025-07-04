#include "quantum_terminal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>

#ifdef QT_MACOS
#include <util.h>
#else
#include <pty.h>
#endif

/* ANSI escape sequence parser states */
typedef enum {
    STATE_NORMAL,
    STATE_ESCAPE,
    STATE_CSI,
    STATE_OSC,
    STATE_DCS
} parser_state_t;

/* Create terminal */
qt_terminal_t *qt_terminal_create(int cols, int rows) {
    qt_terminal_t *term = calloc(1, sizeof(qt_terminal_t));
    if (!term) return NULL;
    
    term->cols = cols;
    term->rows = rows;
    
    /* Allocate cell buffers */
    size_t buffer_size = cols * rows * sizeof(qt_cell_t);
    term->buffer = calloc(1, buffer_size);
    term->alt_buffer = calloc(1, buffer_size);
    
    if (!term->buffer || !term->alt_buffer) {
        free(term->buffer);
        free(term->alt_buffer);
        free(term);
        return NULL;
    }
    
    /* Initialize cells with default colors and welcome message */
    qt_color_t default_fg = {0.0f, 1.0f, 0.0f, 1.0f};  // Bright green
    qt_color_t default_bg = {0.0f, 0.0f, 0.0f, 0.0f};  // Transparent
    
    for (int i = 0; i < cols * rows; i++) {
        term->buffer[i].codepoint = ' ';
        term->buffer[i].fg = default_fg;
        term->buffer[i].bg = default_bg;
        term->alt_buffer[i] = term->buffer[i];
    }
    
    /* Add welcome message */
    const char *welcome = "Welcome to Quantum Terminal!";
    const char *prompt = "$ ";
    int row = 0;
    int col = 0;
    
    /* Write welcome message */
    for (const char *p = welcome; *p && col < cols; p++, col++) {
        term->buffer[row * cols + col].codepoint = *p;
        term->buffer[row * cols + col].fg = (qt_color_t){0.0f, 1.0f, 1.0f, 1.0f}; // Cyan
    }
    
    /* Write prompt on next line */
    row = 1;
    col = 0;
    for (const char *p = prompt; *p && col < cols; p++, col++) {
        term->buffer[row * cols + col].codepoint = *p;
        term->buffer[row * cols + col].fg = (qt_color_t){1.0f, 1.0f, 0.0f, 1.0f}; // Yellow
    }
    term->cursor_x = col;
    term->cursor_y = row;
    
    /* Allocate ring buffers */
    term->read_size = 65536;
    term->write_size = 65536;
    term->read_buffer = malloc(term->read_size);
    term->write_buffer = malloc(term->write_size);
    
    if (!term->read_buffer || !term->write_buffer) {
        free(term->buffer);
        free(term->alt_buffer);
        free(term->read_buffer);
        free(term->write_buffer);
        free(term);
        return NULL;
    }
    
    term->cursor_visible = true;
    term->master_fd = -1;
    term->slave_fd = -1;
    
    return term;
}

/* Destroy terminal */
void qt_terminal_destroy(qt_terminal_t *term) {
    if (!term) return;
    
    /* Close PTY */
    if (term->master_fd >= 0) close(term->master_fd);
    if (term->slave_fd >= 0) close(term->slave_fd);
    
    /* Kill child process */
    if (term->child_pid > 0) {
        kill(term->child_pid, SIGTERM);
    }
    
    /* Free buffers */
    free(term->buffer);
    free(term->alt_buffer);
    free(term->read_buffer);
    free(term->write_buffer);
    free(term);
}

/* Spawn shell process */
int qt_terminal_spawn_shell(qt_terminal_t *term, const char *shell) {
    if (!term) return -1;
    
    /* Create PTY */
    struct winsize ws = {
        .ws_row = term->rows,
        .ws_col = term->cols,
        .ws_xpixel = 0,
        .ws_ypixel = 0
    };
    
    if (openpty(&term->master_fd, &term->slave_fd, NULL, NULL, &ws) < 0) {
        return -1;
    }
    
    /* Make master non-blocking */
    int flags = fcntl(term->master_fd, F_GETFL, 0);
    fcntl(term->master_fd, F_SETFL, flags | O_NONBLOCK);
    
    /* Fork process */
    term->child_pid = fork();
    if (term->child_pid < 0) {
        close(term->master_fd);
        close(term->slave_fd);
        return -1;
    }
    
    if (term->child_pid == 0) {
        /* Child process */
        close(term->master_fd);
        
        /* Make slave the controlling terminal */
        setsid();
        ioctl(term->slave_fd, TIOCSCTTY, 0);
        
        /* Set up stdio */
        dup2(term->slave_fd, STDIN_FILENO);
        dup2(term->slave_fd, STDOUT_FILENO);
        dup2(term->slave_fd, STDERR_FILENO);
        
        if (term->slave_fd > 2) {
            close(term->slave_fd);
        }
        
        /* Set environment */
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        
        /* Execute shell */
        if (!shell) shell = getenv("SHELL");
        if (!shell) shell = "/bin/bash";
        
        execl(shell, shell, NULL);
        _exit(1);
    }
    
    /* Parent process */
    close(term->slave_fd);
    term->slave_fd = -1;
    
    return 0;
}

/* Resize terminal */
void qt_terminal_resize(qt_terminal_t *term, int cols, int rows) {
    if (!term || cols <= 0 || rows <= 0) return;
    
    /* Resize PTY */
    if (term->master_fd >= 0) {
        struct winsize ws = {
            .ws_row = rows,
            .ws_col = cols,
            .ws_xpixel = 0,
            .ws_ypixel = 0
        };
        ioctl(term->master_fd, TIOCSWINSZ, &ws);
    }
    
    /* Reallocate buffers if needed */
    if (cols != term->cols || rows != term->rows) {
        size_t new_size = cols * rows * sizeof(qt_cell_t);
        qt_cell_t *new_buffer = calloc(1, new_size);
        qt_cell_t *new_alt = calloc(1, new_size);
        
        if (!new_buffer || !new_alt) {
            free(new_buffer);
            free(new_alt);
            return;
        }
        
        /* Copy old content */
        int copy_cols = (cols < term->cols) ? cols : term->cols;
        int copy_rows = (rows < term->rows) ? rows : term->rows;
        
        for (int y = 0; y < copy_rows; y++) {
            memcpy(&new_buffer[y * cols], 
                   &term->buffer[y * term->cols],
                   copy_cols * sizeof(qt_cell_t));
            memcpy(&new_alt[y * cols],
                   &term->alt_buffer[y * term->cols],
                   copy_cols * sizeof(qt_cell_t));
        }
        
        free(term->buffer);
        free(term->alt_buffer);
        term->buffer = new_buffer;
        term->alt_buffer = new_alt;
        
        term->cols = cols;
        term->rows = rows;
        
        /* Adjust cursor */
        if (term->cursor_x >= cols) term->cursor_x = cols - 1;
        if (term->cursor_y >= rows) term->cursor_y = rows - 1;
    }
}

/* Send input to terminal */
void qt_terminal_input(qt_terminal_t *term, const char *data, size_t len) {
    if (!term || !data || len == 0 || term->master_fd < 0) return;
    
    /* Write to PTY master */
    ssize_t written = 0;
    while ((size_t)written < len) {
        ssize_t n = write(term->master_fd, data + written, len - written);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Buffer full, try again later */
                break;
            }
            return;
        }
        written += n;
    }
}

/* Simple ANSI parser - writes character at cursor */
static void write_char(qt_terminal_t *term, uint32_t ch) {
    if (term->cursor_x >= term->cols) {
        term->cursor_x = 0;
        term->cursor_y++;
    }
    
    if (term->cursor_y >= term->rows) {
        /* Scroll */
        memmove(term->buffer, 
                term->buffer + term->cols,
                (term->rows - 1) * term->cols * sizeof(qt_cell_t));
        
        /* Clear last line */
        qt_cell_t *last_line = &term->buffer[(term->rows - 1) * term->cols];
        for (int x = 0; x < term->cols; x++) {
            last_line[x].codepoint = ' ';
        }
        
        term->cursor_y = term->rows - 1;
    }
    
    /* Write character */
    qt_cell_t *cell = &term->buffer[term->cursor_y * term->cols + term->cursor_x];
    cell->codepoint = ch;
    
    term->cursor_x++;
}

/* Command buffer for easter egg detection */
static char command_buffer[256] = {0};
static int command_pos = 0;

/* Check for easter egg commands */
static qt_animation_type_t check_easter_egg_command(const char *cmd) {
    /* Matrix-style animation for 'ls' command */
    if (strstr(cmd, "ls") && strlen(cmd) < 10) {
        return QT_ANIM_MATRIX_RAIN;
    }
    
    /* Wormhole portal for 'cd' command */
    if (strncmp(cmd, "cd ", 3) == 0 || strcmp(cmd, "cd") == 0) {
        return QT_ANIM_WORMHOLE_PORTAL;
    }
    
    /* Quantum explosion for dangerous commands */
    if (strstr(cmd, "rm -rf") || strstr(cmd, "rm -Rf")) {
        return QT_ANIM_QUANTUM_EXPLOSION;
    }
    
    /* DNA helix for git commands */
    if (strncmp(cmd, "git ", 4) == 0) {
        return QT_ANIM_DNA_HELIX;
    }
    
    /* Glitch effect for sudo commands */
    if (strncmp(cmd, "sudo ", 5) == 0) {
        return QT_ANIM_GLITCH_TEXT;
    }
    
    /* Neural network for AI/ML commands */
    if (strstr(cmd, "python") || strstr(cmd, "jupyter") || strstr(cmd, "tensorfl")) {
        return QT_ANIM_NEURAL_NETWORK;
    }
    
    /* Cosmic rays for vim/emacs */
    if (strcmp(cmd, "vim") == 0 || strcmp(cmd, "emacs") == 0 || 
        strncmp(cmd, "vim ", 4) == 0 || strncmp(cmd, "emacs ", 6) == 0) {
        return QT_ANIM_COSMIC_RAYS;
    }
    
    /* Particle fountain for make/build commands */
    if (strcmp(cmd, "make") == 0 || strncmp(cmd, "make ", 5) == 0 ||
        strstr(cmd, "npm run") || strstr(cmd, "cargo build")) {
        return QT_ANIM_PARTICLE_FOUNTAIN;
    }
    
    /* Time warp for history command */
    if (strcmp(cmd, "history") == 0) {
        return QT_ANIM_TIME_WARP;
    }
    
    /* Quantum tunneling for ssh */
    if (strncmp(cmd, "ssh ", 4) == 0) {
        return QT_ANIM_QUANTUM_TUNNEL;
    }
    
    return QT_ANIM_NONE;
}

/* Update terminal - read from PTY and parse */
void qt_terminal_update(qt_terminal_t *term, float dt) {
    if (!term || term->master_fd < 0) return;
    
    /* Read from PTY */
    char buf[4096];
    ssize_t n = read(term->master_fd, buf, sizeof(buf));
    if (n <= 0) return;
    
    /* Simple parser - just handle printable chars and newlines */
    for (ssize_t i = 0; i < n; i++) {
        char ch = buf[i];
        
        if (ch == '\n') {
            /* Check for easter egg command before newline */
            if (command_pos > 0) {
                command_buffer[command_pos] = '\0';
                qt_animation_type_t anim = check_easter_egg_command(command_buffer);
                if (anim != QT_ANIM_NONE) {
                    /* Trigger animation at current cursor position */
                    qt_trigger_animation(term->renderer, anim, 
                                       term->cursor_x, term->cursor_y);
                }
                /* Reset command buffer */
                command_pos = 0;
                memset(command_buffer, 0, sizeof(command_buffer));
            }
            
            term->cursor_x = 0;
            term->cursor_y++;
            if (term->cursor_y >= term->rows) {
                term->cursor_y = term->rows - 1;
            }
        } else if (ch == '\r') {
            term->cursor_x = 0;
        } else if (ch == '\b') {
            if (term->cursor_x > 0) term->cursor_x--;
            if (command_pos > 0) command_pos--;
        } else if (ch >= 32 && ch < 127) {
            write_char(term, ch);
            
            /* Add to command buffer for easter egg detection */
            if (command_pos < sizeof(command_buffer) - 1) {
                command_buffer[command_pos++] = ch;
            }
        }
        /* TODO: Full ANSI escape sequence parser */
    }
}