#include "quantum_terminal.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Vertex shader for particles
static const char* particle_vertex_shader = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 position;\n"
    "layout (location = 1) in vec4 color;\n"
    "layout (location = 2) in float size;\n"
    "uniform mat4 projection;\n"
    "uniform mat4 view;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    gl_Position = projection * view * vec4(position, 1.0);\n"
    "    gl_PointSize = size;\n"
    "    fragColor = color;\n"
    "}\n";

// Fragment shader for particles  
static const char* particle_fragment_shader =
    "#version 330 core\n"
    "in vec4 fragColor;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "    vec2 coord = gl_PointCoord - vec2(0.5);\n"
    "    float dist = length(coord);\n"
    "    if (dist > 0.5) discard;\n"
    "    float alpha = 1.0 - smoothstep(0.0, 0.5, dist);\n"
    "    outColor = vec4(fragColor.rgb, fragColor.a * alpha);\n"
    "}\n";

// Vertex shader for terminal
static const char* terminal_vertex_shader =
    "#version 330 core\n"
    "layout (location = 0) in vec2 position;\n"
    "layout (location = 1) in vec2 texCoord;\n"
    "layout (location = 2) in vec4 color;\n"
    "uniform mat4 projection;\n"
    "out vec2 fragTexCoord;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    gl_Position = projection * vec4(position, 0.0, 1.0);\n"
    "    fragTexCoord = texCoord;\n"
    "    fragColor = color;\n"
    "}\n";

// Fragment shader for terminal
static const char* terminal_fragment_shader =
    "#version 330 core\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D fontTexture;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "    float alpha = texture(fontTexture, fragTexCoord).r;\n"
    "    outColor = vec4(fragColor.rgb, fragColor.a * alpha);\n"
    "}\n";

typedef struct {
    GLuint vao, vbo;
    GLuint particle_vao, particle_vbo;
    GLuint particle_shader;
    GLuint terminal_shader;
    GLuint font_texture;
    
    // Uniform locations
    GLint particle_projection_loc;
    GLint particle_view_loc;
    GLint terminal_projection_loc;
    GLint terminal_font_loc;
    
    // Particle data
    float *particle_data;
    int particle_data_size;
} gl_renderer_data_t;

// Compile shader
static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, NULL, info);
        fprintf(stderr, "Shader compilation failed: %s\n", info);
        return 0;
    }
    
    return shader;
}

// Create shader program
static GLuint create_shader_program(const char* vertex_src, const char* fragment_src) {
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_src);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    
    if (!vertex || !fragment) {
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(program, 512, NULL, info);
        fprintf(stderr, "Shader linking failed: %s\n", info);
        return 0;
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    
    return program;
}

// Initialize OpenGL renderer
void qt_gl_renderer_init(qt_renderer_t *renderer) {
    if (!renderer) return;
    
    gl_renderer_data_t *data = calloc(1, sizeof(gl_renderer_data_t));
    if (!data) return;
    
    // Set OpenGL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);  // Lighter background
    
    // Create particle shader
    data->particle_shader = create_shader_program(particle_vertex_shader, particle_fragment_shader);
    data->particle_projection_loc = glGetUniformLocation(data->particle_shader, "projection");
    data->particle_view_loc = glGetUniformLocation(data->particle_shader, "view");
    
    // Create terminal shader
    data->terminal_shader = create_shader_program(terminal_vertex_shader, terminal_fragment_shader);
    data->terminal_projection_loc = glGetUniformLocation(data->terminal_shader, "projection");
    data->terminal_font_loc = glGetUniformLocation(data->terminal_shader, "fontTexture");
    
    // Create particle VAO/VBO
    glGenVertexArrays(1, &data->particle_vao);
    glGenBuffers(1, &data->particle_vbo);
    
    glBindVertexArray(data->particle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, data->particle_vbo);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Size attribute
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Create terminal VAO/VBO
    glGenVertexArrays(1, &data->vao);
    glGenBuffers(1, &data->vbo);
    
    glBindVertexArray(data->vao);
    glBindBuffer(GL_ARRAY_BUFFER, data->vbo);
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // TexCoord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Color attribute
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Create simple 8x8 bitmap font texture
    glGenTextures(1, &data->font_texture);
    glBindTexture(GL_TEXTURE_2D, data->font_texture);
    
    // Create a basic ASCII font atlas (16x8 grid of 8x8 characters)
    int atlas_width = 128;  // 16 chars wide
    int atlas_height = 64;  // 8 chars tall
    unsigned char *font_data = calloc(atlas_width * atlas_height, 1);
    
    // Simple 8x8 font patterns for basic ASCII (space to ~)
    // This is a minimal subset - just enough to show something
    unsigned char char_data[][8] = {
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
        {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}, // !
        {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, // "
        {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // #
        {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // $
        {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, // %
        {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // &
        {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // '
        {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // (
        {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // )
        {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
        {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // +
        {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // ,
        {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // -
        {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // .
        {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // /
        {0x7C,0xC6,0xCE,0xD6,0xE6,0xC6,0x7C,0x00}, // 0
        {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
        {0x7C,0xC6,0x06,0x1C,0x30,0x66,0xFE,0x00}, // 2
        {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00}, // 3
        {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00}, // 4
        {0xFE,0xC0,0xC0,0xFC,0x06,0xC6,0x7C,0x00}, // 5
        {0x38,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00}, // 6
        {0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00}, // 7
        {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00}, // 8
        {0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00}, // 9
        {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // :
        {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // ;
        {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // <
        {0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00}, // =
        {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, // >
        {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00}, // ?
        {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00}, // @
        {0x38,0x6C,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // A
        {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, // B
        {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, // C
        {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, // D
        {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, // E
        {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, // F
        {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3A,0x00}, // G
        {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // H
        {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I
        {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00}, // J
        {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00}, // K
        {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, // L
        {0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00}, // M
        {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, // N
        {0x38,0x6C,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, // O
        {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, // P
        {0x7C,0xC6,0xC6,0xC6,0xD6,0x7C,0x0E,0x00}, // Q
        {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00}, // R
        {0x7C,0xC6,0xE0,0x78,0x0E,0xC6,0x7C,0x00}, // S
        {0x7E,0x7E,0x5A,0x18,0x18,0x18,0x3C,0x00}, // T
        {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // U
        {0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00}, // V
        {0xC6,0xC6,0xD6,0xD6,0xFE,0xEE,0xC6,0x00}, // W
        {0xC6,0x6C,0x38,0x38,0x38,0x6C,0xC6,0x00}, // X
        {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00}, // Y
        {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00}, // Z
        {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // [
        {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, // backslash
        {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // ]
        {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, // ^
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
        {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // `
        {0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00}, // a
        {0xE0,0x60,0x7C,0x66,0x66,0x66,0xDC,0x00}, // b
        {0x00,0x00,0x7C,0xC6,0xC0,0xC6,0x7C,0x00}, // c
        {0x1C,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00}, // d
        {0x00,0x00,0x7C,0xC6,0xFE,0xC0,0x7C,0x00}, // e
        {0x3C,0x66,0x60,0xF8,0x60,0x60,0xF0,0x00}, // f
        {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8}, // g
        {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, // h
        {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // i
        {0x06,0x00,0x06,0x06,0x06,0x66,0x66,0x3C}, // j
        {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, // k
        {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // l
        {0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xD6,0x00}, // m
        {0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x00}, // n
        {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00}, // o
        {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, // p
        {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, // q
        {0x00,0x00,0xDC,0x76,0x60,0x60,0xF0,0x00}, // r
        {0x00,0x00,0x7E,0xC0,0x7C,0x06,0xFC,0x00}, // s
        {0x30,0x30,0xFC,0x30,0x30,0x36,0x1C,0x00}, // t
        {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, // u
        {0x00,0x00,0xC6,0xC6,0x6C,0x38,0x10,0x00}, // v
        {0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x6C,0x00}, // w
        {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, // x
        {0x00,0x00,0xC6,0xC6,0xC6,0x7E,0x06,0xFC}, // y
        {0x00,0x00,0x7E,0x4C,0x18,0x32,0x7E,0x00}, // z
        {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // {
        {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // |
        {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // }
        {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
    };
    
    // Fill the font atlas
    for (int ch = 0; ch < 95; ch++) {  // ASCII 32-126
        int atlas_x = (ch % 16) * 8;
        int atlas_y = (ch / 16) * 8;
        
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if (char_data[ch][y] & (1 << (7 - x))) {
                    font_data[(atlas_y + y) * atlas_width + (atlas_x + x)] = 255;
                }
            }
        }
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas_width, atlas_height, 0, GL_RED, GL_UNSIGNED_BYTE, font_data);
    free(font_data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // Allocate particle data buffer
    data->particle_data_size = QT_MAX_PARTICLES * 8;
    data->particle_data = malloc(data->particle_data_size * sizeof(float));
    
    renderer->native_window = data;
}

// Render particles
void qt_gl_renderer_render_particles(qt_renderer_t *renderer) {
    if (!renderer || !renderer->native_window) return;
    
    gl_renderer_data_t *data = (gl_renderer_data_t*)renderer->native_window;
    
    // Prepare particle data
    int vertex_count = 0;
    float *ptr = data->particle_data;
    
    for (int i = 0; i < renderer->particle_count && vertex_count < QT_MAX_PARTICLES; i++) {
        qt_particle_t *p = &renderer->particles[i];
        if (p->lifetime <= 0) continue;
        
        // Position
        *ptr++ = p->position.x;
        *ptr++ = p->position.y;
        *ptr++ = p->position.z;
        
        // Color with alpha based on lifetime
        float alpha = p->lifetime / QT_PARTICLE_LIFETIME;
        *ptr++ = p->color.r;
        *ptr++ = p->color.g;
        *ptr++ = p->color.b;
        *ptr++ = p->color.a * alpha;
        
        // Size based on energy
        *ptr++ = 5.0f + p->energy * 10.0f;
        
        vertex_count++;
    }
    
    if (vertex_count == 0) return;
    
    // Setup matrices
    qt_mat4_perspective(renderer->projection, 60.0f * M_PI / 180.0f, 
                       (float)renderer->width / renderer->height, 0.1f, 1000.0f);
    
    qt_vec3_t eye = {0, 0, 5};
    qt_vec3_t center = {0, 0, 0};
    qt_vec3_t up = {0, 1, 0};
    qt_mat4_lookat(renderer->view, eye, center, up);
    
    // Render particles
    glUseProgram(data->particle_shader);
    glUniformMatrix4fv(data->particle_projection_loc, 1, GL_FALSE, renderer->projection);
    glUniformMatrix4fv(data->particle_view_loc, 1, GL_FALSE, renderer->view);
    
    glBindVertexArray(data->particle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, data->particle_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * 8 * sizeof(float), 
                 data->particle_data, GL_DYNAMIC_DRAW);
    
    glDrawArrays(GL_POINTS, 0, vertex_count);
}

// Render terminal
void qt_gl_renderer_render_terminal(qt_renderer_t *renderer, qt_terminal_t *term) {
    if (!renderer || !renderer->native_window || !term) return;
    
    gl_renderer_data_t *data = (gl_renderer_data_t*)renderer->native_window;
    
    // Setup orthographic projection
    qt_mat4_identity(renderer->projection);
    float left = 0, right = renderer->width;
    float bottom = renderer->height, top = 0;
    float near = -1, far = 1;
    
    renderer->projection[0] = 2.0f / (right - left);
    renderer->projection[5] = 2.0f / (top - bottom);
    renderer->projection[10] = -2.0f / (far - near);
    renderer->projection[12] = -(right + left) / (right - left);
    renderer->projection[13] = -(top + bottom) / (top - bottom);
    renderer->projection[14] = -(far + near) / (far - near);
    
    // Calculate cell size
    float cell_width = (float)renderer->width / term->cols;
    float cell_height = (float)renderer->height / term->rows;
    
    // Prepare vertex data for visible cells
    float vertices[6 * 8]; // 6 vertices per quad, 8 floats per vertex
    
    glUseProgram(data->terminal_shader);
    glUniformMatrix4fv(data->terminal_projection_loc, 1, GL_FALSE, renderer->projection);
    glUniform1i(data->terminal_font_loc, 0);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, data->font_texture);
    
    glBindVertexArray(data->vao);
    glBindBuffer(GL_ARRAY_BUFFER, data->vbo);
    
    // Render each cell
    for (int y = 0; y < term->rows; y++) {
        for (int x = 0; x < term->cols; x++) {
            qt_cell_t *cell = &term->buffer[y * term->cols + x];
            if (cell->codepoint == 0 || cell->codepoint == ' ') continue;
            
            float x0 = x * cell_width;
            float y0 = y * cell_height;
            float x1 = x0 + cell_width;
            float y1 = y0 + cell_height;
            
            // Create quad vertices
            float *v = vertices;
            
            // Top-left
            *v++ = x0; *v++ = y0; *v++ = 0; *v++ = 0;
            *v++ = cell->fg.r; *v++ = cell->fg.g; *v++ = cell->fg.b; *v++ = cell->fg.a;
            
            // Top-right
            *v++ = x1; *v++ = y0; *v++ = 1; *v++ = 0;
            *v++ = cell->fg.r; *v++ = cell->fg.g; *v++ = cell->fg.b; *v++ = cell->fg.a;
            
            // Bottom-right
            *v++ = x1; *v++ = y1; *v++ = 1; *v++ = 1;
            *v++ = cell->fg.r; *v++ = cell->fg.g; *v++ = cell->fg.b; *v++ = cell->fg.a;
            
            // Bottom-right
            *v++ = x1; *v++ = y1; *v++ = 1; *v++ = 1;
            *v++ = cell->fg.r; *v++ = cell->fg.g; *v++ = cell->fg.b; *v++ = cell->fg.a;
            
            // Bottom-left
            *v++ = x0; *v++ = y1; *v++ = 0; *v++ = 1;
            *v++ = cell->fg.r; *v++ = cell->fg.g; *v++ = cell->fg.b; *v++ = cell->fg.a;
            
            // Top-left
            *v++ = x0; *v++ = y0; *v++ = 0; *v++ = 0;
            *v++ = cell->fg.r; *v++ = cell->fg.g; *v++ = cell->fg.b; *v++ = cell->fg.a;
            
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }
    
    // Draw cursor
    if (term->cursor_visible && term->cursor_x < term->cols && term->cursor_y < term->rows) {
        float x0 = term->cursor_x * cell_width;
        float y0 = term->cursor_y * cell_height;
        float x1 = x0 + cell_width;
        float y1 = y0 + cell_height;
        
        // Create cursor quad with blinking effect
        float alpha = 0.5f + 0.5f * sinf(glfwGetTime() * 6.0f);
        
        float *v = vertices;
        
        // Top-left
        *v++ = x0; *v++ = y0; *v++ = 0; *v++ = 0;
        *v++ = 1; *v++ = 1; *v++ = 1; *v++ = alpha;
        
        // Top-right
        *v++ = x1; *v++ = y0; *v++ = 1; *v++ = 0;
        *v++ = 1; *v++ = 1; *v++ = 1; *v++ = alpha;
        
        // Bottom-right
        *v++ = x1; *v++ = y1; *v++ = 1; *v++ = 1;
        *v++ = 1; *v++ = 1; *v++ = 1; *v++ = alpha;
        
        // Bottom-right
        *v++ = x1; *v++ = y1; *v++ = 1; *v++ = 1;
        *v++ = 1; *v++ = 1; *v++ = 1; *v++ = alpha;
        
        // Bottom-left
        *v++ = x0; *v++ = y1; *v++ = 0; *v++ = 1;
        *v++ = 1; *v++ = 1; *v++ = 1; *v++ = alpha;
        
        // Top-left
        *v++ = x0; *v++ = y0; *v++ = 0; *v++ = 0;
        *v++ = 1; *v++ = 1; *v++ = 1; *v++ = alpha;
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

// Main render function
void qt_gl_renderer_render(qt_renderer_t *renderer, qt_terminal_t *term, float dt) {
    if (!renderer || !term) return;
    
    // Update particles
    qt_quantum_update(renderer, dt);
    
    // Clear screen with visible color
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Render particles first
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for glow
    glDisable(GL_DEPTH_TEST);
    qt_gl_renderer_render_particles(renderer);
    
    // Render terminal text
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    qt_gl_renderer_render_terminal(renderer, term);
    
    // Ensure we rendered something
    glFlush();
}

// Cleanup
void qt_gl_renderer_cleanup(qt_renderer_t *renderer) {
    if (!renderer || !renderer->native_window) return;
    
    gl_renderer_data_t *data = (gl_renderer_data_t*)renderer->native_window;
    
    glDeleteProgram(data->particle_shader);
    glDeleteProgram(data->terminal_shader);
    glDeleteVertexArrays(1, &data->particle_vao);
    glDeleteBuffers(1, &data->particle_vbo);
    glDeleteVertexArrays(1, &data->vao);
    glDeleteBuffers(1, &data->vbo);
    glDeleteTextures(1, &data->font_texture);
    
    free(data->particle_data);
    free(data);
    
    renderer->native_window = NULL;
}