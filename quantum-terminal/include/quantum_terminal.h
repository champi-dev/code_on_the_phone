#ifndef QUANTUM_TERMINAL_H
#define QUANTUM_TERMINAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform detection */
#if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #define QT_IOS
    #else
        #define QT_MACOS
    #endif
#elif defined(__ANDROID__)
    #define QT_ANDROID
#elif defined(__EMSCRIPTEN__)
    #define QT_WEB
#elif defined(__linux__)
    #define QT_LINUX
#elif defined(_WIN32)
    #define QT_WINDOWS
#endif

/* Terminal dimensions */
#define QT_DEFAULT_COLS 80
#define QT_DEFAULT_ROWS 24
#define QT_MAX_COLS 500
#define QT_MAX_ROWS 200

/* Quantum particle system */
#define QT_MAX_PARTICLES 10000
#define QT_PARTICLE_LIFETIME 3.0f

/* Colors */
typedef struct {
    float r, g, b, a;
} qt_color_t;

/* 3D Vector */
typedef struct {
    float x, y, z;
} qt_vec3_t;

/* Quantum particle */
typedef struct {
    qt_vec3_t position;
    qt_vec3_t velocity;
    qt_vec3_t spin;
    qt_color_t color;
    float energy;
    float lifetime;
    float phase;
} qt_particle_t;

/* Terminal cell */
typedef struct {
    uint32_t codepoint;
    qt_color_t fg;
    qt_color_t bg;
    uint8_t attrs;
} qt_cell_t;

/* Terminal state */
typedef struct {
    qt_cell_t *buffer;
    qt_cell_t *alt_buffer;
    int cols, rows;
    int cursor_x, cursor_y;
    bool cursor_visible;
    bool use_alt_buffer;
    
    /* PTY */
    int master_fd;
    int slave_fd;
    int child_pid;
    
    /* Ring buffers */
    uint8_t *read_buffer;
    uint8_t *write_buffer;
    size_t read_pos, write_pos;
    size_t read_size, write_size;
} qt_terminal_t;

/* Renderer state */
typedef struct {
    /* Window */
    void *native_window;
    int width, height;
    float dpi_scale;
    
    /* OpenGL/Metal objects */
    uint32_t vao, vbo, ebo;
    uint32_t shader_program;
    uint32_t particle_shader;
    uint32_t font_texture;
    
    /* Particle system */
    qt_particle_t *particles;
    int particle_count;
    float particle_time;
    
    /* Matrices */
    float projection[16];
    float view[16];
    float model[16];
} qt_renderer_t;

/* Input event */
typedef struct {
    enum {
        QT_EVENT_KEY,
        QT_EVENT_MOUSE,
        QT_EVENT_TOUCH,
        QT_EVENT_RESIZE,
        QT_EVENT_PASTE
    } type;
    
    union {
        struct {
            uint32_t key;
            uint32_t mods;
            bool pressed;
        } key;
        
        struct {
            float x, y;
            int button;
            bool pressed;
        } mouse;
        
        struct {
            float x, y;
            int finger_id;
            float pressure;
        } touch;
        
        struct {
            int width, height;
        } resize;
        
        struct {
            const char *text;
            size_t length;
        } paste;
    };
} qt_input_event_t;

/* Core API */
qt_terminal_t *qt_terminal_create(int cols, int rows);
void qt_terminal_destroy(qt_terminal_t *term);
int qt_terminal_spawn_shell(qt_terminal_t *term, const char *shell);
void qt_terminal_resize(qt_terminal_t *term, int cols, int rows);
void qt_terminal_input(qt_terminal_t *term, const char *data, size_t len);
void qt_terminal_update(qt_terminal_t *term, float dt);

/* Renderer API */
qt_renderer_t *qt_renderer_create(void *native_window);
void qt_renderer_destroy(qt_renderer_t *renderer);
void qt_renderer_resize(qt_renderer_t *renderer, int width, int height);
void qt_renderer_render(qt_renderer_t *renderer, qt_terminal_t *term, float dt);

/* Quantum effects */
void qt_quantum_init(qt_renderer_t *renderer);
void qt_quantum_spawn_burst(qt_renderer_t *renderer, float x, float y, int count);
void qt_quantum_update(qt_renderer_t *renderer, float dt);
void qt_quantum_render(qt_renderer_t *renderer);

/* Platform-specific */
void *qt_platform_create_window(const char *title, int width, int height);
void qt_platform_destroy_window(void *window);
void qt_platform_poll_events(void *window);
void qt_platform_swap_buffers(void *window);
double qt_platform_get_time(void);

/* Utility */
void qt_mat4_identity(float *m);
void qt_mat4_multiply(float *out, const float *a, const float *b);
void qt_mat4_perspective(float *m, float fov, float aspect, float near, float far);
void qt_mat4_lookat(float *m, qt_vec3_t eye, qt_vec3_t center, qt_vec3_t up);
void qt_mat4_translate(float *m, float x, float y, float z);
void qt_mat4_rotate(float *m, float angle, float x, float y, float z);

#ifdef __cplusplus
}
#endif

#endif /* QUANTUM_TERMINAL_H */