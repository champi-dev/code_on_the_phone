#include "quantum_terminal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Constants for quantum effects */
#define QUANTUM_GRAVITY -9.8f
#define QUANTUM_DRAG 0.98f
#define QUANTUM_TURBULENCE 0.5f
#define QUANTUM_GLOW_SPEED 2.0f
#define QUANTUM_SPIN_SPEED 3.0f

/* Random float between 0 and 1 */
static float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

/* Random float between -1 and 1 */
static float randf_signed(void) {
    return randf() * 2.0f - 1.0f;
}

/* Initialize quantum particle system */
void qt_quantum_init(qt_renderer_t *renderer) {
    if (!renderer) return;
    
    renderer->particles = calloc(QT_MAX_PARTICLES, sizeof(qt_particle_t));
    if (!renderer->particles) return;
    
    renderer->particle_count = 0;
    renderer->particle_time = 0.0f;
}

/* Spawn particle burst at position */
void qt_quantum_spawn_burst(qt_renderer_t *renderer, float x, float y, int count) {
    if (!renderer || !renderer->particles) return;
    
    /* Convert screen coords to world coords */
    float world_x = (x / renderer->width) * 2.0f - 1.0f;
    float world_y = 1.0f - (y / renderer->height) * 2.0f;
    
    for (int i = 0; i < count && renderer->particle_count < QT_MAX_PARTICLES; i++) {
        qt_particle_t *p = &renderer->particles[renderer->particle_count++];
        
        /* Position with small random offset */
        p->position.x = world_x + randf_signed() * 0.02f;
        p->position.y = world_y + randf_signed() * 0.02f;
        p->position.z = randf_signed() * 0.1f;
        
        /* Velocity in random direction */
        float angle = randf() * M_PI * 2.0f;
        float speed = randf() * 2.0f + 1.0f;
        p->velocity.x = cosf(angle) * speed;
        p->velocity.y = sinf(angle) * speed + 2.0f; /* Upward bias */
        p->velocity.z = randf_signed() * 0.5f;
        
        /* Random spin */
        p->spin.x = randf_signed() * QUANTUM_SPIN_SPEED;
        p->spin.y = randf_signed() * QUANTUM_SPIN_SPEED;
        p->spin.z = randf_signed() * QUANTUM_SPIN_SPEED;
        
        /* Quantum color - cyan to purple gradient */
        float hue = randf();
        if (hue < 0.5f) {
            /* Cyan to blue */
            p->color.r = 0.0f;
            p->color.g = 1.0f - hue;
            p->color.b = 1.0f;
        } else {
            /* Blue to purple */
            p->color.r = (hue - 0.5f) * 2.0f;
            p->color.g = 0.0f;
            p->color.b = 1.0f;
        }
        p->color.a = 1.0f;
        
        /* Energy and lifetime */
        p->energy = randf() * 0.5f + 0.5f;
        p->lifetime = QT_PARTICLE_LIFETIME * (0.8f + randf() * 0.4f);
        p->phase = randf() * M_PI * 2.0f;
    }
}

/* Update quantum particles with physics */
void qt_quantum_update(qt_renderer_t *renderer, float dt) {
    if (!renderer || !renderer->particles) return;
    
    renderer->particle_time += dt;
    
    /* Update each particle */
    int active_count = 0;
    for (int i = 0; i < renderer->particle_count; i++) {
        qt_particle_t *p = &renderer->particles[i];
        
        /* Update lifetime */
        p->lifetime -= dt;
        if (p->lifetime <= 0.0f) continue;
        
        /* Apply physics */
        p->velocity.y += QUANTUM_GRAVITY * dt;
        p->velocity.x *= powf(QUANTUM_DRAG, dt);
        p->velocity.z *= powf(QUANTUM_DRAG, dt);
        
        /* Add turbulence */
        float turb_phase = p->phase + renderer->particle_time * 2.0f;
        p->velocity.x += sinf(turb_phase) * QUANTUM_TURBULENCE * dt;
        p->velocity.z += cosf(turb_phase * 1.3f) * QUANTUM_TURBULENCE * dt;
        
        /* Update position */
        p->position.x += p->velocity.x * dt;
        p->position.y += p->velocity.y * dt;
        p->position.z += p->velocity.z * dt;
        
        /* Update spin */
        p->spin.x += randf_signed() * 0.5f * dt;
        p->spin.y += randf_signed() * 0.5f * dt;
        p->spin.z += randf_signed() * 0.5f * dt;
        
        /* Update energy (quantum fluctuation) */
        p->energy = 0.5f + 0.5f * sinf(renderer->particle_time * QUANTUM_GLOW_SPEED + p->phase);
        
        /* Fade out */
        float fade = p->lifetime / QT_PARTICLE_LIFETIME;
        p->color.a = fade * fade; /* Quadratic fade */
        
        /* Bounce off ground */
        if (p->position.y < -1.0f) {
            p->position.y = -1.0f;
            p->velocity.y = -p->velocity.y * 0.6f; /* Energy loss */
            
            /* Spawn secondary particles on impact */
            if (fabsf(p->velocity.y) > 2.0f && randf() < 0.3f) {
                qt_quantum_spawn_burst(renderer, 
                    (p->position.x + 1.0f) * 0.5f * renderer->width,
                    (1.0f - p->position.y) * 0.5f * renderer->height,
                    3);
            }
        }
        
        /* Keep particle if still alive */
        if (p->lifetime > 0.0f) {
            if (active_count != i) {
                renderer->particles[active_count] = *p;
            }
            active_count++;
        }
    }
    
    renderer->particle_count = active_count;
}

/* Get particle vertex data for rendering */
void qt_quantum_get_vertices(qt_renderer_t *renderer, float *vertices, int *count) {
    if (!renderer || !renderer->particles || !vertices) return;
    
    int vertex_count = 0;
    
    for (int i = 0; i < renderer->particle_count; i++) {
        qt_particle_t *p = &renderer->particles[i];
        
        /* Each particle is a billboard quad (6 vertices) */
        float size = 0.02f * p->energy;
        
        /* Calculate billboard corners */
        qt_vec3_t corners[4] = {
            {-size, -size, 0.0f},
            { size, -size, 0.0f},
            { size,  size, 0.0f},
            {-size,  size, 0.0f}
        };
        
        /* Apply rotation based on spin */
        float angle = renderer->particle_time * (p->spin.x + p->spin.y + p->spin.z);
        float cos_a = cosf(angle);
        float sin_a = sinf(angle);
        
        for (int j = 0; j < 4; j++) {
            float x = corners[j].x * cos_a - corners[j].y * sin_a;
            float y = corners[j].x * sin_a + corners[j].y * cos_a;
            corners[j].x = x;
            corners[j].y = y;
        }
        
        /* Triangle 1: 0, 1, 2 */
        for (int j = 0; j < 3; j++) {
            int idx = (i * 6 + j) * 9; /* 9 floats per vertex */
            vertices[idx + 0] = p->position.x + corners[j].x;
            vertices[idx + 1] = p->position.y + corners[j].y;
            vertices[idx + 2] = p->position.z + corners[j].z;
            vertices[idx + 3] = p->color.r;
            vertices[idx + 4] = p->color.g;
            vertices[idx + 5] = p->color.b;
            vertices[idx + 6] = p->color.a * p->energy;
            vertices[idx + 7] = (j == 1 || j == 2) ? 1.0f : 0.0f; /* U */
            vertices[idx + 8] = (j >= 2) ? 1.0f : 0.0f; /* V */
        }
        
        /* Triangle 2: 0, 2, 3 */
        int indices[3] = {0, 2, 3};
        for (int j = 0; j < 3; j++) {
            int k = indices[j];
            int idx = (i * 6 + 3 + j) * 9;
            vertices[idx + 0] = p->position.x + corners[k].x;
            vertices[idx + 1] = p->position.y + corners[k].y;
            vertices[idx + 2] = p->position.z + corners[k].z;
            vertices[idx + 3] = p->color.r;
            vertices[idx + 4] = p->color.g;
            vertices[idx + 5] = p->color.b;
            vertices[idx + 6] = p->color.a * p->energy;
            vertices[idx + 7] = (k == 1 || k == 2) ? 1.0f : 0.0f;
            vertices[idx + 8] = (k >= 2) ? 1.0f : 0.0f;
        }
        
        vertex_count += 6;
    }
    
    *count = vertex_count;
}