#include <metal_stdlib>
using namespace metal;

struct TerminalUniforms {
    float4x4 projection;
    float2 cellSize;
    float2 terminalSize;
    float time;
    float cursorBlink;
};

struct VertexOut {
    float4 position [[position]];
    float2 uv;
    float4 fgColor;
    float4 bgColor;
    uint character;
};

// Vertex shader for terminal cells
vertex VertexOut terminal_vertex(uint vid [[vertex_id]],
                                constant TerminalUniforms &uniforms [[buffer(0)]],
                                constant uint *characters [[buffer(1)]],
                                constant float4 *fgColors [[buffer(2)]],
                                constant float4 *bgColors [[buffer(3)]]) {
    uint cellIndex = vid / 6;
    uint cornerIndex = vid % 6;
    
    // Calculate cell position
    uint cols = uint(uniforms.terminalSize.x);
    uint x = cellIndex % cols;
    uint y = cellIndex / cols;
    
    // Corner positions for quad
    float2 corners[6] = {
        float2(0, 0), float2(1, 0), float2(1, 1),
        float2(0, 0), float2(1, 1), float2(0, 1)
    };
    
    float2 corner = corners[cornerIndex];
    float2 position = float2(x, y) + corner;
    position *= uniforms.cellSize;
    
    VertexOut out;
    out.position = uniforms.projection * float4(position, 0.0, 1.0);
    out.uv = corner;
    out.fgColor = fgColors[cellIndex];
    out.bgColor = bgColors[cellIndex];
    out.character = characters[cellIndex];
    
    return out;
}

// Fragment shader with CRT effects
fragment float4 terminal_fragment(VertexOut in [[stage_in]],
                                 texture2d<float> fontTexture [[texture(0)]],
                                 constant TerminalUniforms &uniforms [[buffer(0)]]) {
    constexpr sampler textureSampler(mag_filter::linear,
                                   min_filter::linear);
    
    // Sample font texture
    uint charX = in.character % 16;
    uint charY = in.character / 16;
    float2 charUV = (float2(charX, charY) + in.uv) / 16.0;
    float4 fontSample = fontTexture.sample(textureSampler, charUV);
    
    // Mix foreground and background
    float4 color = mix(in.bgColor, in.fgColor, fontSample.r);
    
    // CRT scanline effect
    float scanline = sin(in.position.y * 2.0) * 0.02 + 0.98;
    color.rgb *= scanline;
    
    // Phosphor glow
    float glow = fontSample.r * 0.3;
    color.rgb += in.fgColor.rgb * glow;
    
    // Subtle chromatic aberration
    float2 screenUV = in.position.xy / uniforms.terminalSize;
    float aberration = length(screenUV - 0.5) * 0.01;
    color.r *= 1.0 + aberration;
    color.b *= 1.0 - aberration;
    
    return color;
}