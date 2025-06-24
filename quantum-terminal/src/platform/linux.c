#include "quantum_terminal.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

typedef struct {
    GLFWwindow *window;
    qt_terminal_t *terminal;
    qt_renderer_t *renderer;
    double last_time;
    int width, height;
} linux_context_t;

static linux_context_t g_ctx = {0};

// Error callback
static void error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Key callback
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    
    char buf[32];
    int len = 0;
    
    // Handle special keys
    if (key == GLFW_KEY_ENTER) {
        buf[0] = '\r';
        len = 1;
    } else if (key == GLFW_KEY_BACKSPACE) {
        buf[0] = 127;
        len = 1;
    } else if (key == GLFW_KEY_ESCAPE) {
        buf[0] = 27;
        len = 1;
    } else if (key == GLFW_KEY_TAB) {
        buf[0] = '\t';
        len = 1;
    } else if (key >= GLFW_KEY_SPACE && key <= GLFW_KEY_GRAVE_ACCENT) {
        // Regular ASCII keys
        if (mods & GLFW_MOD_SHIFT) {
            // Simple shift mapping
            const char* shift_map = ")!@#$%^&*(";
            if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
                buf[0] = shift_map[key - GLFW_KEY_0];
                len = 1;
            } else if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                buf[0] = key; // Already uppercase
                len = 1;
            }
        } else if (mods & GLFW_MOD_CONTROL) {
            // Control sequences
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                buf[0] = key - GLFW_KEY_A + 1;
                len = 1;
            }
        } else {
            // Normal keys
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                buf[0] = key + 32; // lowercase
                len = 1;
            } else {
                buf[0] = key;
                len = 1;
            }
        }
    } else if (key == GLFW_KEY_UP) {
        strcpy(buf, "\033[A");
        len = 3;
    } else if (key == GLFW_KEY_DOWN) {
        strcpy(buf, "\033[B");
        len = 3;
    } else if (key == GLFW_KEY_RIGHT) {
        strcpy(buf, "\033[C");
        len = 3;
    } else if (key == GLFW_KEY_LEFT) {
        strcpy(buf, "\033[D");
        len = 3;
    }
    
    if (len > 0 && g_ctx.terminal) {
        qt_terminal_input(g_ctx.terminal, buf, len);
    }
}

// Character callback for proper text input
static void char_callback(GLFWwindow* window, unsigned int codepoint) {
    if (!g_ctx.terminal) return;
    
    char buf[4];
    int len = 0;
    
    // Convert Unicode codepoint to UTF-8
    if (codepoint < 0x80) {
        buf[0] = codepoint;
        len = 1;
    } else if (codepoint < 0x800) {
        buf[0] = 0xC0 | (codepoint >> 6);
        buf[1] = 0x80 | (codepoint & 0x3F);
        len = 2;
    } else if (codepoint < 0x10000) {
        buf[0] = 0xE0 | (codepoint >> 12);
        buf[1] = 0x80 | ((codepoint >> 6) & 0x3F);
        buf[2] = 0x80 | (codepoint & 0x3F);
        len = 3;
    }
    
    if (len > 0) {
        qt_terminal_input(g_ctx.terminal, buf, len);
    }
}

// Mouse button callback
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        
        if (g_ctx.renderer) {
            qt_quantum_spawn_burst(g_ctx.renderer, xpos, ypos, 50);
        }
    }
}

// Window resize callback
static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    g_ctx.width = width;
    g_ctx.height = height;
    
    if (g_ctx.renderer) {
        qt_renderer_resize(g_ctx.renderer, width, height);
    }
    
    glViewport(0, 0, width, height);
}

// Create window
void* qt_platform_create_window(const char* title, int width, int height) {
    glfwSetErrorCallback(error_callback);
    
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return NULL;
    }
    
    // Set OpenGL version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return NULL;
    }
    
    glfwMakeContextCurrent(window);
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return NULL;
    }
    
    // Set callbacks
    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    
    // Enable vsync
    glfwSwapInterval(1);
    
    // Store context
    g_ctx.window = window;
    g_ctx.width = width;
    g_ctx.height = height;
    g_ctx.last_time = glfwGetTime();
    
    // Create terminal
    g_ctx.terminal = qt_terminal_create(QT_DEFAULT_COLS, QT_DEFAULT_ROWS);
    if (!g_ctx.terminal) {
        fprintf(stderr, "Failed to create terminal\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return NULL;
    }
    
    // Spawn shell
    qt_terminal_spawn_shell(g_ctx.terminal, "/bin/bash");
    
    // Create renderer
    g_ctx.renderer = qt_renderer_create(window);
    if (!g_ctx.renderer) {
        fprintf(stderr, "Failed to create renderer\n");
        qt_terminal_destroy(g_ctx.terminal);
        glfwDestroyWindow(window);
        glfwTerminate();
        return NULL;
    }
    
    // Initialize quantum effects
    qt_quantum_init(g_ctx.renderer);
    
    // Initial particle burst
    qt_quantum_spawn_burst(g_ctx.renderer, width/2, height/2, 100);
    
    return window;
}

// Destroy window
void qt_platform_destroy_window(void* window) {
    if (g_ctx.renderer) {
        qt_renderer_destroy(g_ctx.renderer);
        g_ctx.renderer = NULL;
    }
    
    if (g_ctx.terminal) {
        qt_terminal_destroy(g_ctx.terminal);
        g_ctx.terminal = NULL;
    }
    
    if (window) {
        glfwDestroyWindow((GLFWwindow*)window);
    }
    
    glfwTerminate();
}

// Poll events
void qt_platform_poll_events(void* window) {
    glfwPollEvents();
    
    // Update terminal
    double current_time = glfwGetTime();
    float dt = current_time - g_ctx.last_time;
    g_ctx.last_time = current_time;
    
    if (g_ctx.terminal) {
        qt_terminal_update(g_ctx.terminal, dt);
    }
    
    if (g_ctx.renderer && g_ctx.terminal) {
        // Clear
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Render
        qt_renderer_render(g_ctx.renderer, g_ctx.terminal, dt);
        
        // Swap buffers
        glfwSwapBuffers((GLFWwindow*)window);
    }
    
    // Check if should close
    if (glfwWindowShouldClose((GLFWwindow*)window)) {
        exit(0);
    }
}

// Swap buffers
void qt_platform_swap_buffers(void* window) {
    // Already handled in poll_events
}

// Get time
double qt_platform_get_time(void) {
    return glfwGetTime();
}