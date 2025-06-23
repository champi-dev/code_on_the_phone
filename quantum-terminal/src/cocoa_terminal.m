#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <math.h>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

#define COLS 80
#define ROWS 24
#define CHAR_WIDTH 9
#define CHAR_HEIGHT 16

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float r, g, b, a;
    float life;
} Particle;

typedef struct {
    char ch;
    float r, g, b;
} Cell;

// Global state
Cell terminal[ROWS][COLS];
Particle particles[1000];
int particle_count = 0;
int master_fd = -1;
int cursor_x = 0, cursor_y = 0;

@interface QuantumView : NSOpenGLView {
    NSTimer *renderTimer;
    NSTimer *updateTimer;
    GLuint fontTexture;
    NSFont *terminalFont;
}
- (void)createFontTexture;
- (void)drawChar:(char)ch atX:(float)x y:(float)y r:(float)r g:(float)g b:(float)b;
@end

@implementation QuantumView

- (void)prepareOpenGL {
    [super prepareOpenGL];
    
    // Enable vsync
    GLint swapInt = 1;
    [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLContextParameterSwapInterval];
    
    // Setup OpenGL
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    
    // Create font texture
    [self createFontTexture];
    
    // Initialize terminal
    [self initTerminal];
    
    // Start timers
    renderTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0
                                                    target:self
                                                  selector:@selector(drawFrame)
                                                  userInfo:nil
                                                   repeats:YES];
    
    updateTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/30.0
                                                    target:self
                                                  selector:@selector(updateTerminal)
                                                  userInfo:nil
                                                   repeats:YES];
}

- (void)createFontTexture {
    // Create a bitmap context to render ASCII characters
    terminalFont = [NSFont fontWithName:@"Menlo" size:14.0];
    if (!terminalFont) {
        terminalFont = [NSFont systemFontOfSize:14.0];
    }
    
    int texWidth = 16 * CHAR_WIDTH;
    int texHeight = 16 * CHAR_HEIGHT;
    
    NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
        pixelsWide:texWidth
        pixelsHigh:texHeight
        bitsPerSample:8
        samplesPerPixel:4
        hasAlpha:YES
        isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace
        bytesPerRow:4 * texWidth
        bitsPerPixel:32];
    
    NSGraphicsContext *context = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:context];
    
    // Clear to transparent
    [[NSColor clearColor] set];
    NSRectFill(NSMakeRect(0, 0, texWidth, texHeight));
    
    // Draw ASCII characters
    NSDictionary *attrs = @{
        NSFontAttributeName: terminalFont,
        NSForegroundColorAttributeName: [NSColor whiteColor]
    };
    
    for (int i = 32; i < 127; i++) {
        int row = (i - 32) / 16;
        int col = (i - 32) % 16;
        
        NSString *str = [NSString stringWithFormat:@"%c", i];
        NSPoint point = NSMakePoint(col * CHAR_WIDTH + 1, texHeight - (row + 1) * CHAR_HEIGHT + 2);
        [str drawAtPoint:point withAttributes:attrs];
    }
    
    [NSGraphicsContext restoreGraphicsState];
    
    // Create OpenGL texture
    glGenTextures(1, &fontTexture);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, 
                 GL_RGBA, GL_UNSIGNED_BYTE, [bitmap bitmapData]);
}

- (void)drawChar:(char)ch atX:(float)x y:(float)y r:(float)r g:(float)g b:(float)b {
    if (ch < 32 || ch >= 127) return;
    
    int idx = ch - 32;
    int row = idx / 16;
    int col = idx % 16;
    
    float u0 = (float)(col * CHAR_WIDTH) / (16.0f * CHAR_WIDTH);
    float v0 = (float)(row * CHAR_HEIGHT) / (16.0f * CHAR_HEIGHT);
    float u1 = u0 + (float)CHAR_WIDTH / (16.0f * CHAR_WIDTH);
    float v1 = v0 + (float)CHAR_HEIGHT / (16.0f * CHAR_HEIGHT);
    
    glColor4f(r, g, b, 1.0f);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v1); glVertex2f(x, y);
    glTexCoord2f(u1, v1); glVertex2f(x + CHAR_WIDTH, y);
    glTexCoord2f(u1, v0); glVertex2f(x + CHAR_WIDTH, y + CHAR_HEIGHT);
    glTexCoord2f(u0, v0); glVertex2f(x, y + CHAR_HEIGHT);
    glEnd();
}

- (void)initTerminal {
    // Clear terminal
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            terminal[y][x].ch = ' ';
            terminal[y][x].r = 0.9f;
            terminal[y][x].g = 0.9f;
            terminal[y][x].b = 0.9f;
        }
    }
    
    // No welcome message - start with clean terminal
    
    // Create PTY
    struct winsize ws = {ROWS, COLS, 0, 0};
    int slave_fd;
    if (openpty(&master_fd, &slave_fd, NULL, NULL, &ws) == 0) {
        // Make non-blocking
        fcntl(master_fd, F_SETFL, O_NONBLOCK);
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child - run shell
            close(master_fd);
            
            // Set up slave as stdin/stdout/stderr
            dup2(slave_fd, STDIN_FILENO);
            dup2(slave_fd, STDOUT_FILENO);
            dup2(slave_fd, STDERR_FILENO);
            
            // Close original slave fd
            if (slave_fd > 2) {
                close(slave_fd);
            }
            
            // Make it a controlling terminal
            setsid();
            ioctl(STDIN_FILENO, TIOCSCTTY, 0);
            
            execl("/bin/bash", "bash", NULL);
            exit(1);
        }
        // Parent - close slave fd
        close(slave_fd);
    }
    
    cursor_x = 0;
    cursor_y = 0;
}

- (void)addParticle:(NSPoint)pos {
    if (particle_count >= 1000) return;
    
    Particle *p = &particles[particle_count++];
    p->x = pos.x;
    p->y = pos.y;
    p->z = 0;
    p->vx = ((float)rand() / RAND_MAX - 0.5f) * 200;
    p->vy = ((float)rand() / RAND_MAX - 0.5f) * 200 + 100;
    p->vz = 0;
    
    // Quantum colors
    float t = (float)rand() / RAND_MAX;
    if (t < 0.5f) {
        p->r = 0;
        p->g = 1 - t;
        p->b = 1;
    } else {
        p->r = (t - 0.5f) * 2;
        p->g = 0;
        p->b = 1;
    }
    p->a = 1.0f;
    p->life = 3.0f;
}

- (void)updateParticles:(float)dt {
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        
        // Physics
        p->vy -= 400 * dt;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        
        // Fade out
        p->life -= dt;
        p->a = p->life / 3.0f;
        
        // Remove dead particles
        if (p->life <= 0) {
            particles[i] = particles[--particle_count];
            i--;
        }
    }
}

- (void)writeChar:(char)ch {
    if (ch == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= ROWS) {
            // Scroll
            for (int y = 0; y < ROWS - 1; y++) {
                for (int x = 0; x < COLS; x++) {
                    terminal[y][x] = terminal[y + 1][x];
                }
            }
            for (int x = 0; x < COLS; x++) {
                terminal[ROWS - 1][x].ch = ' ';
                terminal[ROWS - 1][x].r = 0.9f;
                terminal[ROWS - 1][x].g = 0.9f;
                terminal[ROWS - 1][x].b = 0.9f;
            }
            cursor_y = ROWS - 1;
        }
    } else if (ch == '\r') {
        cursor_x = 0;
    } else if (ch == '\b') {
        // Backspace
        if (cursor_x > 0) {
            cursor_x--;
            terminal[cursor_y][cursor_x].ch = ' ';
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = COLS - 1;
            terminal[cursor_y][cursor_x].ch = ' ';
        }
    } else if (ch == '\t') {
        // Tab - move to next 8-column boundary
        cursor_x = ((cursor_x / 8) + 1) * 8;
        if (cursor_x >= COLS) {
            cursor_x = 0;
            cursor_y++;
        }
    } else if (ch >= 32 && ch < 127) {
        if (cursor_y < ROWS && cursor_x < COLS) {
            terminal[cursor_y][cursor_x].ch = ch;
            terminal[cursor_y][cursor_x].r = 0.9f;
            terminal[cursor_y][cursor_x].g = 0.9f;
            terminal[cursor_y][cursor_x].b = 0.9f;
            cursor_x++;
            if (cursor_x >= COLS) {
                cursor_x = 0;
                cursor_y++;
                if (cursor_y >= ROWS) {
                    // Trigger scroll by simulating newline
                    cursor_y--;
                    [self writeChar:'\n'];
                }
            }
        }
    }
}

- (void)updateTerminal {
    if (master_fd < 0) return;
    
    char buf[1024];
    ssize_t n = read(master_fd, buf, sizeof(buf));
    if (n > 0) {
        for (ssize_t i = 0; i < n; i++) {
            [self writeChar:buf[i]];
        }
    }
}

- (void)drawFrame {
    [[self openGLContext] makeCurrentContext];
    
    NSRect bounds = [self bounds];
    glViewport(0, 0, bounds.size.width, bounds.size.height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, bounds.size.width, 0, bounds.size.height, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Draw terminal text
    glEnable(GL_TEXTURE_2D);
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            char ch = terminal[y][x].ch;
            if (ch != ' ' && ch != '\0') {
                float px = x * CHAR_WIDTH;
                float py = bounds.size.height - (y + 1) * CHAR_HEIGHT;
                [self drawChar:ch atX:px y:py 
                         r:terminal[y][x].r 
                         g:terminal[y][x].g 
                         b:terminal[y][x].b];
            }
        }
    }
    glDisable(GL_TEXTURE_2D);
    
    // Draw cursor
    if (cursor_x < COLS && cursor_y < ROWS) {
        glColor3f(0, 1, 0);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        float cx = cursor_x * CHAR_WIDTH;
        float cy = bounds.size.height - cursor_y * CHAR_HEIGHT;
        glVertex2f(cx, cy);
        glVertex2f(cx, cy - CHAR_HEIGHT);
        glEnd();
    }
    
    // Update particles
    static NSTimeInterval lastTime = 0;
    NSTimeInterval currentTime = [NSDate timeIntervalSinceReferenceDate];
    float dt = lastTime ? (currentTime - lastTime) : 0.016f;
    lastTime = currentTime;
    [self updateParticles:dt];
    
    // Draw particles
    glPointSize(3.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        glColor4f(p->r, p->g, p->b, p->a);
        glVertex2f(p->x, p->y);
    }
    glEnd();
    
    [[self openGLContext] flushBuffer];
}

- (void)keyDown:(NSEvent *)event {
    NSString *chars = [event charactersIgnoringModifiers];
    if (chars.length > 0 && master_fd >= 0) {
        const char *utf8 = [chars UTF8String];
        write(master_fd, utf8, strlen(utf8));
    }
}

- (void)mouseMoved:(NSEvent *)event {
    NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
    
    static NSTimeInterval lastParticleTime = 0;
    NSTimeInterval currentTime = [NSDate timeIntervalSinceReferenceDate];
    
    if (currentTime - lastParticleTime > 0.05) {
        [self addParticle:location];
        lastParticleTime = currentTime;
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
    for (int i = 0; i < 20; i++) {
        NSPoint p = NSMakePoint(location.x + (rand() % 20 - 10),
                               location.y + (rand() % 20 - 10));
        [self addParticle:p];
    }
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)dealloc {
    [renderTimer invalidate];
    [updateTimer invalidate];
    if (master_fd >= 0) close(master_fd);
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate> {
    NSWindow *window;
}
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSRect frame = NSMakeRect(0, 0, COLS * CHAR_WIDTH, ROWS * CHAR_HEIGHT);
    
    window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:(NSWindowStyleMaskTitled |
                                                   NSWindowStyleMaskClosable |
                                                   NSWindowStyleMaskMiniaturizable |
                                                   NSWindowStyleMaskResizable)
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
    
    [window setTitle:@"Quantum Terminal"];
    [window center];
    
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        0
    };
    
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    QuantumView *view = [[QuantumView alloc] initWithFrame:frame pixelFormat:pixelFormat];
    // ARC handles release
    
    [window setContentView:view];
    [window makeFirstResponder:view];
    
    // Make window visible and active
    [window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    
    // Set up tracking for mouse events
    NSTrackingArea *trackingArea = [[NSTrackingArea alloc]
        initWithRect:[view bounds]
        options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow)
        owner:view
        userInfo:nil];
    [view addTrackingArea:trackingArea];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        
        [app setDelegate:delegate];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        // Create menu bar
        NSMenu *menubar = [[NSMenu alloc] init];
        NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
        [menubar addItem:appMenuItem];
        [app setMainMenu:menubar];
        
        NSMenu *appMenu = [[NSMenu alloc] init];
        NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:@"Quit Quantum Terminal"
                                                              action:@selector(terminate:)
                                                       keyEquivalent:@"q"];
        [appMenu addItem:quitMenuItem];
        [appMenuItem setSubmenu:appMenu];
        
        [app activateIgnoringOtherApps:YES];
        [app run];
    }
    return 0;
}