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

/* Forward declaration */
static void update_animation_particles(qt_renderer_t *renderer, float dt);

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
    
    /* Update special animation particles first */
    update_animation_particles(renderer, dt);
    
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

/* Trigger special animation effect */
void qt_trigger_animation(qt_renderer_t *renderer, qt_animation_type_t type, int x, int y) {
    if (!renderer || type == QT_ANIM_NONE) return;
    
    renderer->current_animation = type;
    renderer->animation_time = 0.0f;
    renderer->animation_x = (float)x;
    renderer->animation_y = (float)y;
    
    /* Convert terminal coords to screen coords */
    float char_width = renderer->width / 80.0f;  /* Assuming 80 cols */
    float char_height = renderer->height / 24.0f; /* Assuming 24 rows */
    float screen_x = x * char_width + char_width / 2;
    float screen_y = y * char_height + char_height / 2;
    
    /* Spawn initial particles based on animation type */
    switch (type) {
        case QT_ANIM_MATRIX_RAIN:
            /* Create falling green characters */
            for (int i = 0; i < 20; i++) {
                for (int j = 0; j < 50; j++) {
                    if (renderer->particle_count >= QT_MAX_PARTICLES) break;
                    qt_particle_t *p = &renderer->particles[renderer->particle_count++];
                    
                    /* Random position across screen width */
                    p->position.x = randf_signed();
                    p->position.y = 1.0f + randf() * 2.0f; /* Start above screen */
                    p->position.z = randf() * 0.5f - 0.25f;
                    
                    /* Falling velocity */
                    p->velocity.x = 0.0f;
                    p->velocity.y = -1.0f - randf() * 2.0f;
                    p->velocity.z = 0.0f;
                    
                    /* Matrix green color */
                    p->color.r = 0.0f;
                    p->color.g = 0.8f + randf() * 0.2f;
                    p->color.b = 0.2f;
                    p->color.a = 0.8f;
                    
                    p->energy = 0.5f + randf() * 0.5f;
                    p->lifetime = 5.0f + randf() * 3.0f;
                    p->phase = randf() * M_PI * 2.0f;
                    p->animation_type = type;
                    
                    /* No spin for matrix rain */
                    p->spin.x = p->spin.y = p->spin.z = 0.0f;
                }
            }
            break;
            
        case QT_ANIM_WORMHOLE_PORTAL:
            /* Create swirling portal effect */
            for (int i = 0; i < 200; i++) {
                if (renderer->particle_count >= QT_MAX_PARTICLES) break;
                qt_particle_t *p = &renderer->particles[renderer->particle_count++];
                
                /* Position in a circle around cursor */
                float angle = randf() * M_PI * 2.0f;
                float radius = randf() * 0.3f;
                p->position.x = (screen_x / renderer->width) * 2.0f - 1.0f;
                p->position.y = 1.0f - (screen_y / renderer->height) * 2.0f;
                p->position.z = 0.0f;
                
                /* Spiral outward velocity */
                p->velocity.x = cosf(angle) * radius * 3.0f;
                p->velocity.y = sinf(angle) * radius * 3.0f;
                p->velocity.z = randf_signed() * 2.0f;
                
                /* Portal colors - blue/purple */
                p->color.r = 0.2f + randf() * 0.3f;
                p->color.g = 0.0f;
                p->color.b = 0.8f + randf() * 0.2f;
                p->color.a = 1.0f;
                
                p->energy = 1.0f;
                p->lifetime = 2.0f;
                p->phase = angle;
                p->animation_type = type;
                
                /* Strong spin for portal effect */
                p->spin.x = p->spin.y = 0.0f;
                p->spin.z = 10.0f;
            }
            break;
            
        case QT_ANIM_QUANTUM_EXPLOSION:
            /* Create explosive burst */
            qt_quantum_spawn_burst(renderer, screen_x, screen_y, 500);
            /* Override colors for explosion */
            for (int i = renderer->particle_count - 500; i < renderer->particle_count; i++) {
                if (i < 0) continue;
                qt_particle_t *p = &renderer->particles[i];
                /* Red/orange/yellow explosion colors */
                float heat = randf();
                if (heat < 0.3f) {
                    p->color.r = 1.0f;
                    p->color.g = 0.0f;
                    p->color.b = 0.0f;
                } else if (heat < 0.7f) {
                    p->color.r = 1.0f;
                    p->color.g = 0.5f;
                    p->color.b = 0.0f;
                } else {
                    p->color.r = 1.0f;
                    p->color.g = 1.0f;
                    p->color.b = 0.2f;
                }
                /* Stronger initial velocity */
                float speed = randf() * 5.0f + 3.0f;
                float angle = randf() * M_PI * 2.0f;
                p->velocity.x = cosf(angle) * speed;
                p->velocity.y = sinf(angle) * speed;
                p->animation_type = type;
            }
            break;
            
        case QT_ANIM_DNA_HELIX:
            /* Create double helix structure */
            for (int i = 0; i < 100; i++) {
                if (renderer->particle_count >= QT_MAX_PARTICLES - 1) break;
                
                float t = (float)i / 100.0f;
                float angle = t * M_PI * 8.0f; /* 4 full rotations */
                
                /* First strand */
                qt_particle_t *p1 = &renderer->particles[renderer->particle_count++];
                p1->position.x = (screen_x / renderer->width) * 2.0f - 1.0f + cosf(angle) * 0.1f;
                p1->position.y = 1.0f - (screen_y / renderer->height) * 2.0f + (t - 0.5f) * 0.8f;
                p1->position.z = sinf(angle) * 0.1f;
                
                p1->velocity.x = 0.0f;
                p1->velocity.y = 0.5f;
                p1->velocity.z = 0.0f;
                
                /* DNA colors - ATCG bases */
                int base = i % 4;
                switch (base) {
                    case 0: /* Adenine - green */
                        p1->color.r = 0.0f; p1->color.g = 0.8f; p1->color.b = 0.2f;
                        break;
                    case 1: /* Thymine - red */
                        p1->color.r = 0.8f; p1->color.g = 0.2f; p1->color.b = 0.0f;
                        break;
                    case 2: /* Cytosine - blue */
                        p1->color.r = 0.0f; p1->color.g = 0.2f; p1->color.b = 0.8f;
                        break;
                    case 3: /* Guanine - yellow */
                        p1->color.r = 0.8f; p1->color.g = 0.8f; p1->color.b = 0.0f;
                        break;
                }
                p1->color.a = 1.0f;
                p1->energy = 1.0f;
                p1->lifetime = 3.0f;
                p1->animation_type = type;
                
                /* Second strand */
                qt_particle_t *p2 = &renderer->particles[renderer->particle_count++];
                *p2 = *p1;
                p2->position.x = (screen_x / renderer->width) * 2.0f - 1.0f - cosf(angle) * 0.1f;
                p2->position.z = -sinf(angle) * 0.1f;
                /* Complementary base colors */
                switch (base) {
                    case 0: /* T complements A */
                        p2->color.r = 0.8f; p2->color.g = 0.2f; p2->color.b = 0.0f;
                        break;
                    case 1: /* A complements T */
                        p2->color.r = 0.0f; p2->color.g = 0.8f; p2->color.b = 0.2f;
                        break;
                    case 2: /* G complements C */
                        p2->color.r = 0.8f; p2->color.g = 0.8f; p2->color.b = 0.0f;
                        break;
                    case 3: /* C complements G */
                        p2->color.r = 0.0f; p2->color.g = 0.2f; p2->color.b = 0.8f;
                        break;
                }
            }
            break;
            
        case QT_ANIM_GLITCH_TEXT:
            /* Create glitchy scattered particles */
            for (int i = 0; i < 100; i++) {
                if (renderer->particle_count >= QT_MAX_PARTICLES) break;
                qt_particle_t *p = &renderer->particles[renderer->particle_count++];
                
                /* Random positions around text */
                p->position.x = (screen_x / renderer->width) * 2.0f - 1.0f + randf_signed() * 0.2f;
                p->position.y = 1.0f - (screen_y / renderer->height) * 2.0f + randf_signed() * 0.1f;
                p->position.z = randf_signed() * 0.05f;
                
                /* Jittery movement */
                p->velocity.x = randf_signed() * 2.0f;
                p->velocity.y = randf_signed() * 2.0f;
                p->velocity.z = 0.0f;
                
                /* RGB glitch colors */
                int channel = i % 3;
                p->color.r = (channel == 0) ? 1.0f : 0.0f;
                p->color.g = (channel == 1) ? 1.0f : 0.0f;
                p->color.b = (channel == 2) ? 1.0f : 0.0f;
                p->color.a = 0.8f;
                
                p->energy = randf() * 2.0f; /* Flickering */
                p->lifetime = 0.5f + randf() * 0.5f;
                p->phase = randf() * M_PI * 2.0f;
                p->animation_type = type;
            }
            break;
            
        default:
            /* Regular burst for other animations */
            qt_quantum_spawn_burst(renderer, screen_x, screen_y, 100);
            break;
    }
}

/* Update animations with special behaviors */
static void update_animation_particles(qt_renderer_t *renderer, float dt) {
    if (renderer->current_animation == QT_ANIM_NONE) return;
    
    renderer->animation_time += dt;
    
    /* Update particles based on their animation type */
    for (int i = 0; i < renderer->particle_count; i++) {
        qt_particle_t *p = &renderer->particles[i];
        
        switch (p->animation_type) {
            case QT_ANIM_MATRIX_RAIN:
                /* Reset position when particle falls off screen */
                if (p->position.y < -1.2f) {
                    p->position.y = 1.2f;
                    p->position.x = randf_signed();
                    p->lifetime = 5.0f + randf() * 3.0f;
                }
                /* Flicker effect */
                p->energy = 0.5f + 0.5f * sinf(renderer->particle_time * 10.0f + p->phase);
                break;
                
            case QT_ANIM_WORMHOLE_PORTAL:
                /* Spiral motion */
                p->phase += dt * 5.0f;
                float radius = renderer->animation_time * 0.5f;
                p->velocity.x = cosf(p->phase) * radius;
                p->velocity.y = sinf(p->phase) * radius;
                /* Color shift */
                p->color.r = 0.5f + 0.5f * sinf(renderer->animation_time * 2.0f);
                p->color.b = 0.5f + 0.5f * cosf(renderer->animation_time * 2.0f);
                break;
                
            case QT_ANIM_DNA_HELIX:
                /* Rotate around helix axis */
                p->phase += dt * 2.0f;
                break;
                
            case QT_ANIM_GLITCH_TEXT:
                /* Random teleportation */
                if (randf() < 0.1f) {
                    p->position.x += randf_signed() * 0.1f;
                    p->position.y += randf_signed() * 0.1f;
                }
                /* Flashing */
                p->color.a = (randf() < 0.9f) ? 0.8f : 0.0f;
                break;
                
            default:
                break;
        }
    }
    
    /* Stop animation after certain duration */
    if (renderer->animation_time > 5.0f) {
        renderer->current_animation = QT_ANIM_NONE;
    }
}