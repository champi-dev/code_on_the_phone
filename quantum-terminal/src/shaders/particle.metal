#include <metal_stdlib>
using namespace metal;

struct Particle {
    float3 position;
    float3 velocity;
    float4 color;
    float size;
    float energy;
};

struct VertexOut {
    float4 position [[position]];
    float4 color;
    float2 uv;
    float energy;
};

// Vertex shader for particle billboards
vertex VertexOut particle_vertex(uint vid [[vertex_id]],
                                uint iid [[instance_id]],
                                constant Particle *particles [[buffer(0)]],
                                constant float4x4 &viewProjection [[buffer(1)]]) {
    // Billboard corners
    float2 corners[6] = {
        float2(-1, -1), float2(1, -1), float2(1, 1),
        float2(-1, -1), float2(1, 1), float2(-1, 1)
    };
    
    Particle p = particles[iid];
    float2 corner = corners[vid % 6];
    
    // Calculate world position
    float3 worldPos = p.position + float3(corner * p.size, 0.0);
    
    VertexOut out;
    out.position = viewProjection * float4(worldPos, 1.0);
    out.color = p.color;
    out.uv = corner * 0.5 + 0.5;
    out.energy = p.energy;
    
    return out;
}

// Fragment shader with quantum glow effect
fragment float4 particle_fragment(VertexOut in [[stage_in]]) {
    // Distance from center
    float2 center = in.uv - 0.5;
    float dist = length(center);
    
    // Soft circle with glow
    float circle = 1.0 - smoothstep(0.0, 0.5, dist);
    float glow = exp(-dist * 3.0) * in.energy;
    
    // Quantum fluctuation
    float flicker = sin(in.energy * 10.0) * 0.1 + 0.9;
    
    // Final color with additive blending
    float4 color = in.color;
    color.rgb *= (circle + glow * 2.0) * flicker;
    color.a *= circle;
    
    return color;
}