#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include "quantum_terminal.h"

@interface QuantumTerminalView : MTKView <MTKViewDelegate>
@property (nonatomic) qt_terminal_t *terminal;
@property (nonatomic) qt_renderer_t *renderer;
@property (nonatomic) id<MTLDevice> device;
@property (nonatomic) id<MTLCommandQueue> commandQueue;
@property (nonatomic) id<MTLRenderPipelineState> pipelineState;
@property (nonatomic) id<MTLRenderPipelineState> particlePipelineState;
@property (nonatomic) CVDisplayLinkRef displayLink;
@end

@implementation QuantumTerminalView

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.device = MTLCreateSystemDefaultDevice();
        self.delegate = self;
        self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        self.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
        self.sampleCount = 1;
        
        self.commandQueue = [self.device newCommandQueue];
        
        // Create terminal
        self.terminal = qt_terminal_create(QT_DEFAULT_COLS, QT_DEFAULT_ROWS);
        
        // Add some test text to the terminal
        const char *welcome = "Welcome to Quantum Terminal!\n$ ";
        for (int i = 0; welcome[i]; i++) {
            if (welcome[i] == '\n') {
                self.terminal->cursor_x = 0;
                self.terminal->cursor_y++;
            } else {
                int idx = self.terminal->cursor_y * self.terminal->cols + self.terminal->cursor_x;
                self.terminal->buffer[idx].codepoint = welcome[i];
                self.terminal->cursor_x++;
            }
        }
        
        // Spawn shell
        qt_terminal_spawn_shell(self.terminal, "/bin/bash");
        
        // Create renderer
        self.renderer = qt_renderer_create((__bridge void *)self);
        
        // Link terminal to renderer for animations
        self.terminal->renderer = self.renderer;
        
        qt_quantum_init(self.renderer);
        
        // Enable vsync
        self.enableSetNeedsDisplay = NO;
        self.paused = NO;
        self.preferredFramesPerSecond = 60;
        
        // Setup display link for smooth animation
        CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
        CVDisplayLinkSetOutputCallback(_displayLink, displayLinkCallback, (__bridge void *)self);
        CVDisplayLinkStart(_displayLink);
        
        // Track mouse for particle effects
        NSTrackingArea *trackingArea = [[NSTrackingArea alloc] 
            initWithRect:self.bounds
            options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow)
            owner:self
            userInfo:nil];
        [self addTrackingArea:trackingArea];
        
        // Initial particle burst
        qt_quantum_spawn_burst(self.renderer, frame.size.width/2, frame.size.height/2, 100);
    }
    return self;
}

static CVReturn displayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp *now,
                                  const CVTimeStamp *outputTime, CVOptionFlags flagsIn,
                                  CVOptionFlags *flagsOut, void *displayLinkContext) {
    @autoreleasepool {
        QuantumTerminalView *view = (__bridge QuantumTerminalView *)displayLinkContext;
        dispatch_async(dispatch_get_main_queue(), ^{
            [view setNeedsDisplay:YES];
        });
    }
    return kCVReturnSuccess;
}

- (void)dealloc {
    CVDisplayLinkRelease(_displayLink);
    qt_terminal_destroy(self.terminal);
    qt_renderer_destroy(self.renderer);
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    qt_renderer_resize(self.renderer, size.width, size.height);
}

- (void)drawInMTKView:(MTKView *)view {
    static double lastTime = 0.0;
    double currentTime = CACurrentMediaTime();
    float dt = lastTime ? (currentTime - lastTime) : 0.016f;
    lastTime = currentTime;
    
    // Update terminal and particles
    qt_terminal_update(self.terminal, dt);
    qt_quantum_update(self.renderer, dt);
    
    // Render
    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    MTLRenderPassDescriptor *renderPassDescriptor = view.currentRenderPassDescriptor;
    
    if (renderPassDescriptor != nil) {
        // Animated background color to verify rendering
        float time = currentTime * 0.5;
        float r = 0.05 + 0.05 * sin(time);
        float g = 0.05 + 0.05 * sin(time + 2.0);
        float b = 0.1 + 0.1 * sin(time + 4.0);
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(r, g, b, 1.0);
        
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        
        // Set viewport
        MTLViewport viewport = {0, 0, view.drawableSize.width, view.drawableSize.height, 0.0, 1.0};
        [renderEncoder setViewport:viewport];
        
        // For now, just draw a test triangle to verify rendering works
        float vertices[] = {
            // x,    y,   r,   g,   b
             0.0,  0.5, 1.0, 0.0, 0.0,  // Top (red)
            -0.5, -0.5, 0.0, 1.0, 0.0,  // Bottom left (green)
             0.5, -0.5, 0.0, 0.0, 1.0,  // Bottom right (blue)
        };
        
        id<MTLBuffer> vertexBuffer = [self.device newBufferWithBytes:vertices 
                                                               length:sizeof(vertices) 
                                                              options:MTLResourceStorageModeShared];
        
        // Draw the terminal content as colored rectangles for each character
        if (self.terminal && self.terminal->buffer) {
            // Simple rendering - draw a green rectangle for each non-space character
            for (int y = 0; y < self.terminal->rows && y < 10; y++) {
                for (int x = 0; x < self.terminal->cols && x < 40; x++) {
                    qt_cell_t *cell = &self.terminal->buffer[y * self.terminal->cols + x];
                    if (cell->codepoint != ' ') {
                        // Character is present - would render here
                    }
                }
            }
        }
        
        [renderEncoder endEncoding];
        [commandBuffer presentDrawable:view.currentDrawable];
    }
    
    [commandBuffer commit];
}

- (void)keyDown:(NSEvent *)event {
    NSString *chars = [event charactersIgnoringModifiers];
    if (chars.length > 0) {
        const char *utf8 = [chars UTF8String];
        qt_terminal_input(self.terminal, utf8, strlen(utf8));
    }
}

- (void)mouseMoved:(NSEvent *)event {
    NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
    
    // Spawn particles at mouse position
    static double lastParticleTime = 0.0;
    double currentTime = CACurrentMediaTime();
    if (currentTime - lastParticleTime > 0.05) { // 20 Hz
        qt_quantum_spawn_burst(self.renderer, location.x, self.bounds.size.height - location.y, 5);
        lastParticleTime = currentTime;
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
    qt_quantum_spawn_burst(self.renderer, location.x, self.bounds.size.height - location.y, 50);
}

@end

@interface QuantumTerminalWindow : NSWindow
@end

@implementation QuantumTerminalWindow

- (BOOL)canBecomeKeyWindow {
    return YES;
}

- (BOOL)canBecomeMainWindow {
    return YES;
}

@end

// Platform implementation
void *qt_platform_create_window(const char *title, int width, int height) {
    @autoreleasepool {
        // Initialize NSApplication
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        NSRect frame = NSMakeRect(0, 0, width, height);
        
        QuantumTerminalWindow *window = [[QuantumTerminalWindow alloc]
            initWithContentRect:frame
            styleMask:(NSWindowStyleMaskTitled |
                      NSWindowStyleMaskClosable |
                      NSWindowStyleMaskMiniaturizable |
                      NSWindowStyleMaskResizable)
            backing:NSBackingStoreBuffered
            defer:NO];
        
        [window setTitle:[NSString stringWithUTF8String:title]];
        [window center];
        
        QuantumTerminalView *view = [[QuantumTerminalView alloc] initWithFrame:frame];
        [window setContentView:view];
        [window makeKeyAndOrderFront:nil];
        
        return (__bridge_retained void *)window;
    }
}

void qt_platform_destroy_window(void *window) {
    @autoreleasepool {
        NSWindow *nsWindow = (__bridge_transfer NSWindow *)window;
        [nsWindow close];
    }
}

void qt_platform_poll_events(void *window) {
    @autoreleasepool {
        NSEvent *event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:nil
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES])) {
            [NSApp sendEvent:event];
        }
    }
}

void qt_platform_swap_buffers(void *window) {
    // Metal handles this automatically
}

double qt_platform_get_time(void) {
    return CACurrentMediaTime();
}