#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include "quantum_terminal.h"

// Metal renderer implementation
// Most of the rendering is handled in macos.m
// This file provides additional Metal-specific utilities

void qt_metal_init_shaders(id<MTLDevice> device) {
    // Shader initialization would go here
}

void qt_metal_render_particles(id<MTLRenderCommandEncoder> encoder, 
                              qt_renderer_t *renderer) {
    // Particle rendering implementation
}

void qt_metal_render_terminal(id<MTLRenderCommandEncoder> encoder,
                             qt_terminal_t *terminal) {
    // Terminal rendering implementation
}