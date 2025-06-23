#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>

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
    float x, y;
    float vx, vy;
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

@interface QuantumView : NSView {
    NSTimer *renderTimer;
    NSTimer *updateTimer;
}
@end

@implementation QuantumView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        [self initTerminal];
        
        // Start timers
        renderTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0
                                                        target:self
                                                      selector:@selector(setNeedsDisplay)
                                                      userInfo:nil
                                                       repeats:YES];
        
        updateTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/30.0
                                                        target:self
                                                      selector:@selector(updateTerminal)
                                                      userInfo:nil
                                                       repeats:YES];
    }
    return self;
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
    
    // Welcome message
    const char *lines[] = {
        "=== Quantum Terminal ===",
        "",
        "Move mouse for particle trails!",
        "Click for particle bursts!",
        "Type to use the terminal!",
        "",
        "$ "
    };
    
    for (int i = 0; i < 7; i++) {
        const char *line = lines[i];
        for (int j = 0; line[j] && j < COLS; j++) {
            terminal[i][j].ch = line[j];
            terminal[i][j].r = 0.0f;
            terminal[i][j].g = 1.0f;
            terminal[i][j].b = 1.0f;
        }
    }
    
    cursor_y = 6;
    cursor_x = 2;
    
    // Create PTY
    struct winsize ws = {ROWS, COLS, 0, 0};
    if (openpty(&master_fd, NULL, NULL, NULL, &ws) == 0) {
        // Make non-blocking
        fcntl(master_fd, F_SETFL, O_NONBLOCK);
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child - run shell
            close(master_fd);
            execl("/bin/bash", "bash", NULL);
            exit(1);
        }
    }
}

- (void)addParticle:(NSPoint)pos {
    if (particle_count >= 1000) return;
    
    Particle *p = &particles[particle_count++];
    p->x = pos.x;
    p->y = pos.y;
    p->vx = ((float)rand() / RAND_MAX - 0.5f) * 200;
    p->vy = ((float)rand() / RAND_MAX - 0.5f) * 200 + 100;
    
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
            }
            cursor_y = ROWS - 1;
        }
    } else if (ch == '\r') {
        cursor_x = 0;
    } else if (ch == '\b') {
        if (cursor_x > 0) cursor_x--;
    } else if (ch >= 32 && ch < 127) {
        terminal[cursor_y][cursor_x].ch = ch;
        cursor_x++;
        if (cursor_x >= COLS) {
            cursor_x = 0;
            cursor_y++;
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

- (void)drawRect:(NSRect)dirtyRect {
    // Background
    [[NSColor colorWithRed:0.05 green:0.05 blue:0.1 alpha:1.0] setFill];
    NSRectFill(dirtyRect);
    
    // Update particles
    static NSTimeInterval lastTime = 0;
    NSTimeInterval currentTime = [NSDate timeIntervalSinceReferenceDate];
    float dt = lastTime ? (currentTime - lastTime) : 0.016f;
    lastTime = currentTime;
    [self updateParticles:dt];
    
    // Draw terminal text using NSString
    NSFont *font = [NSFont fontWithName:@"Monaco" size:14];
    if (!font) font = [NSFont monospacedSystemFontOfSize:14 weight:NSFontWeightRegular];
    
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            if (terminal[y][x].ch != ' ') {
                NSString *str = [NSString stringWithFormat:@"%c", terminal[y][x].ch];
                NSColor *color = [NSColor colorWithRed:terminal[y][x].r 
                                                 green:terminal[y][x].g 
                                                  blue:terminal[y][x].b 
                                                 alpha:1.0];
                
                NSDictionary *attrs = @{
                    NSFontAttributeName: font,
                    NSForegroundColorAttributeName: color
                };
                
                NSPoint point = NSMakePoint(x * CHAR_WIDTH, 
                                          self.bounds.size.height - (y + 1) * CHAR_HEIGHT);
                [str drawAtPoint:point withAttributes:attrs];
            }
        }
    }
    
    // Draw cursor
    if (cursor_x < COLS && cursor_y < ROWS) {
        NSRect cursorRect = NSMakeRect(cursor_x * CHAR_WIDTH,
                                      self.bounds.size.height - (cursor_y + 1) * CHAR_HEIGHT,
                                      2, CHAR_HEIGHT);
        [[NSColor greenColor] setFill];
        NSRectFill(cursorRect);
    }
    
    // Draw particles
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        NSColor *particleColor = [NSColor colorWithRed:p->r green:p->g blue:p->b alpha:p->a];
        [particleColor setFill];
        
        NSRect particleRect = NSMakeRect(p->x - 2, p->y - 2, 4, 4);
        NSBezierPath *path = [NSBezierPath bezierPathWithOvalInRect:particleRect];
        [path fill];
    }
}

- (BOOL)acceptsFirstResponder {
    return YES;
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

- (void)dealloc {
    [renderTimer invalidate];
    [updateTimer invalidate];
    if (master_fd >= 0) close(master_fd);
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSRect frame = NSMakeRect(0, 0, COLS * CHAR_WIDTH, ROWS * CHAR_HEIGHT);
    
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:(NSWindowStyleMaskTitled |
                                                        NSWindowStyleMaskClosable |
                                                        NSWindowStyleMaskMiniaturizable |
                                                        NSWindowStyleMaskResizable)
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    
    [self.window setTitle:@"Quantum Terminal"];
    [self.window center];
    
    QuantumView *view = [[QuantumView alloc] initWithFrame:frame];
    [self.window setContentView:view];
    [self.window makeFirstResponder:view];
    
    // Set up tracking for mouse events
    NSTrackingArea *trackingArea = [[NSTrackingArea alloc]
        initWithRect:[view bounds]
        options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow)
        owner:view
        userInfo:nil];
    [view addTrackingArea:trackingArea];
    
    // Show window and bring to front
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    
    // Ensure window is visible
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self.window orderFrontRegardless];
        [self.window makeKeyWindow];
        [self.window makeMainWindow];
    });
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
        
        // Launch the app
        [app run];
    }
    return 0;
}