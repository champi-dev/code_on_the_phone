#include "quantum_terminal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

qt_renderer_t *qt_renderer_create(void *native_window) {
    qt_renderer_t *renderer = calloc(1, sizeof(qt_renderer_t));
    if (!renderer) return NULL;
    
    renderer->native_window = native_window;
    renderer->width = 1024;
    renderer->height = 768;
    renderer->dpi_scale = 1.0f;
    
    // Initialize matrices
    qt_mat4_identity(renderer->projection);
    qt_mat4_identity(renderer->view);
    qt_mat4_identity(renderer->model);
    
    return renderer;
}

void qt_renderer_destroy(qt_renderer_t *renderer) {
    if (!renderer) return;
    free(renderer->particles);
    free(renderer);
}

void qt_renderer_resize(qt_renderer_t *renderer, int width, int height) {
    if (!renderer) return;
    renderer->width = width;
    renderer->height = height;
}

void qt_renderer_render(qt_renderer_t *renderer, qt_terminal_t *term, float dt) {
    // Platform-specific rendering handled elsewhere
    (void)renderer;
    (void)term;
    (void)dt;
}

// Matrix operations
void qt_mat4_identity(float *m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void qt_mat4_multiply(float *out, const float *a, const float *b) {
    float temp[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                temp[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
            }
        }
    }
    memcpy(out, temp, 16 * sizeof(float));
}

void qt_mat4_perspective(float *m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    float nf = 1.0f / (near - far);
    
    memset(m, 0, 16 * sizeof(float));
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + near) * nf;
    m[11] = -1.0f;
    m[14] = 2.0f * far * near * nf;
}

void qt_mat4_lookat(float *m, qt_vec3_t eye, qt_vec3_t center, qt_vec3_t up) {
    // Simple lookat implementation
    qt_mat4_identity(m);
}

void qt_mat4_translate(float *m, float x, float y, float z) {
    qt_mat4_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

void qt_mat4_rotate(float *m, float angle, float x, float y, float z) {
    // Simple rotation
    qt_mat4_identity(m);
}