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
#define MAX_PARTICLES 10000
#define GRID_SIZE 64
#define QUANTUM_CONNECTIONS 5

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float r, g, b, a;
    float life;
    float size;
    float phase;
    float energy;
    int type; // 0=ambient, 1=quantum, 2=entangled
    int connections[QUANTUM_CONNECTIONS];
    int grid_x, grid_y; // Spatial hashing for O(1) lookup
} Particle;

typedef struct {
    char ch;
    float r, g, b;
} Cell;

// Global state
Cell terminal[ROWS][COLS];
Particle particles[MAX_PARTICLES];
int particle_count = 0;
int master_fd = -1;
int cursor_x = 0, cursor_y = 0;
float quantum_time = 0.0f;
float terminal_glow = 0.0f;
int escape_state = 0;
char escape_buffer[256];
int escape_len = 0;

// Mouse interaction state
float mouse_x = 0, mouse_y = 0;
float mouse_force = 0;
int mouse_down = 0;
float wave_amplitude = 0;
float wave_origin_x = 0, wave_origin_y = 0;

// Spatial hash grid for O(1) particle lookup
Particle* particle_grid[GRID_SIZE][GRID_SIZE][10];
int grid_counts[GRID_SIZE][GRID_SIZE];

@interface QuantumView : NSOpenGLView {
    NSTimer *renderTimer;
    NSTimer *updateTimer;
    GLuint fontTexture;
    NSFont *terminalFont;
}
- (void)createFontTexture;
- (void)drawChar:(char)ch atX:(float)x y:(float)y r:(float)r g:(float)g b:(float)b;
- (void)initQuantumField;
- (void)updateQuantumField:(float)dt;
- (void)updateSpatialGrid;
- (void)drawQuantumConnections;
- (void)drawTerminalGlow;
- (void)processEscapeSequence:(char)ch;
- (void)executeEscapeSequence;
- (void)checkEasterEggs:(const char*)command;
@end

@implementation QuantumView

- (void)prepareOpenGL {
    [super prepareOpenGL];
    
    // Enable vsync
    GLint swapInt = 1;
    [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLContextParameterSwapInterval];
    
    // Setup OpenGL
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f); // Darker background for better contrast
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glClearDepth(1.0f);
    
    // Create font texture
    [self createFontTexture];
    
    // Initialize terminal
    [self initTerminal];
    
    // Initialize quantum field
    [self initQuantumField];
    
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

- (void)initQuantumField {
    srand(time(NULL));
    
    // Clear spatial grid
    memset(grid_counts, 0, sizeof(grid_counts));
    
    // Create quantum field layers
    // Layer 1: Deep background nebula particles
    for (int i = 0; i < 200; i++) {
        if (particle_count >= MAX_PARTICLES) break;
        
        Particle *p = &particles[particle_count++];
        
        p->x = ((float)rand() / RAND_MAX - 0.5f) * 1200;
        p->y = ((float)rand() / RAND_MAX - 0.5f) * 800;
        p->z = 200 + (float)rand() / RAND_MAX * 300;
        
        p->vx = ((float)rand() / RAND_MAX - 0.5f) * 5;
        p->vy = ((float)rand() / RAND_MAX - 0.5f) * 5;
        p->vz = 0;
        
        // Deep space colors - dark purples and blues
        p->r = 0.1f + (float)rand() / RAND_MAX * 0.1f;
        p->g = 0.05f + (float)rand() / RAND_MAX * 0.1f;
        p->b = 0.2f + (float)rand() / RAND_MAX * 0.2f;
        p->a = 0.3f;
        
        p->life = 10000.0f;
        p->size = 20.0f + (float)rand() / RAND_MAX * 30.0f;
        p->phase = (float)rand() / RAND_MAX * M_PI * 2.0f;
        p->energy = 0.5f;
        p->type = 0;
    }
    
    // Layer 2: Quantum field particles
    for (int i = 0; i < 800; i++) {
        if (particle_count >= MAX_PARTICLES) break;
        
        Particle *p = &particles[particle_count++];
        
        // Grid distribution for even spacing
        float grid_x = (i % 40) * 30 - 600;
        float grid_y = (i / 40) * 30 - 300;
        
        p->x = grid_x + ((float)rand() / RAND_MAX - 0.5f) * 20;
        p->y = grid_y + ((float)rand() / RAND_MAX - 0.5f) * 20;
        p->z = ((float)rand() / RAND_MAX - 0.5f) * 200;
        
        p->vx = 0;
        p->vy = 0;
        p->vz = 0;
        
        // Quantum field colors - electric cyan with hints of violet
        float t = (float)rand() / RAND_MAX;
        p->r = 0.0f;
        p->g = 0.5f + t * 0.3f;
        p->b = 0.8f + t * 0.2f;
        p->a = 0.4f;
        
        p->life = 10000.0f;
        p->size = 1.0f;
        p->phase = (float)rand() / RAND_MAX * M_PI * 2.0f;
        p->energy = 1.0f + (float)rand() / RAND_MAX;
        p->type = 1;
        
        // Initialize connections for quantum entanglement
        for (int j = 0; j < QUANTUM_CONNECTIONS; j++) {
            p->connections[j] = -1;
        }
    }
    
    // Create quantum entanglements
    for (int i = 0; i < particle_count; i++) {
        if (particles[i].type == 1) {
            int connections_made = 0;
            for (int j = i + 1; j < particle_count && connections_made < 2; j++) {
                if (particles[j].type == 1) {
                    float dx = particles[i].x - particles[j].x;
                    float dy = particles[i].y - particles[j].y;
                    float dist = sqrtf(dx*dx + dy*dy);
                    
                    if (dist < 100 && dist > 20) {
                        particles[i].connections[connections_made] = j;
                        particles[i].type = 2;
                        particles[j].type = 2;
                        connections_made++;
                    }
                }
            }
        }
    }
}

- (void)updateQuantumField:(float)dt {
    quantum_time += dt;
    
    // Update terminal glow based on activity
    terminal_glow *= 0.95f;
    
    // Energy waves through quantum field
    float wave1 = sinf(quantum_time * 0.5f) * 0.5f + 0.5f;
    float wave2 = cosf(quantum_time * 0.7f) * 0.5f + 0.5f;
    
    // Spawn quantum burst on typing
    if (terminal_glow > 0.1f && particle_count < MAX_PARTICLES - 50) {
        for (int i = 0; i < 5; i++) {
            Particle *p = &particles[particle_count++];
            
            float angle = ((float)rand() / RAND_MAX) * M_PI * 2.0f;
            float radius = 100 + (float)rand() / RAND_MAX * 200;
            
            p->x = cursor_x * CHAR_WIDTH + cosf(angle) * radius;
            p->y = 400 - cursor_y * CHAR_HEIGHT + sinf(angle) * radius;
            p->z = ((float)rand() / RAND_MAX - 0.5f) * 100;
            
            p->vx = cosf(angle) * 50;
            p->vy = sinf(angle) * 50;
            p->vz = ((float)rand() / RAND_MAX - 0.5f) * 20;
            
            // Electric typing burst colors
            p->r = 0.2f + terminal_glow * 0.8f;
            p->g = 0.8f + terminal_glow * 0.2f;
            p->b = 1.0f;
            p->a = 0.8f;
            
            p->life = 3.0f;
            p->size = 2.0f;
            p->phase = quantum_time;
            p->energy = 2.0f;
            p->type = 1;
        }
    }
}

- (void)updateSpatialGrid {
    // Clear grid - O(1) with fixed grid size
    memset(grid_counts, 0, sizeof(grid_counts));
    
    // Update grid positions - O(n) where n is particle count
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        
        // Map world coordinates to grid
        int gx = (int)((p->x + 800) / 25.0f);
        int gy = (int)((p->y + 600) / 18.75f);
        
        if (gx >= 0 && gx < GRID_SIZE && gy >= 0 && gy < GRID_SIZE) {
            p->grid_x = gx;
            p->grid_y = gy;
            
            if (grid_counts[gx][gy] < 10) {
                particle_grid[gx][gy][grid_counts[gx][gy]++] = p;
            }
        }
    }
}

- (void)addParticle:(NSPoint)pos {
    if (particle_count >= 5000) return;
    
    // Add multiple particles for burst effect
    int burst = 3 + rand() % 5;
    for (int i = 0; i < burst && particle_count < 5000; i++) {
        Particle *p = &particles[particle_count++];
        p->x = pos.x;
        p->y = pos.y;
        p->z = 0;
        
        // 3D velocity - more realistic burst
        float angle = ((float)rand() / RAND_MAX) * M_PI * 2.0f;
        float speed = 50.0f + ((float)rand() / RAND_MAX) * 100.0f;  // Reduced speed
        p->vx = cosf(angle) * speed;
        p->vy = sinf(angle) * speed + 20.0f;  // Slight upward bias
        p->vz = ((float)rand() / RAND_MAX - 0.5f) * 30;
        
        // Subtle quantum colors
        float t = (float)rand() / RAND_MAX;
        if (t < 0.5f) {
            p->r = 0;
            p->g = (1 - t) * 0.6f;  // More subtle green
            p->b = 0.6f;  // Less intense blue
        } else {
            p->r = (t - 0.5f) * 0.6f;  // Subtle purple
            p->g = 0;
            p->b = 0.6f;
        }
        p->a = 0.8f;  // 80% opacity
        p->life = 2.0f + (float)rand() / RAND_MAX * 2.0f;
        p->size = 4.0f + (float)rand() / RAND_MAX * 2.0f;
        p->phase = quantum_time;
    }
}

- (void)updateParticles:(float)dt {
    // Update quantum field
    [self updateQuantumField:dt];
    
    // Update spatial grid for O(1) lookups
    [self updateSpatialGrid];
    
    // Update wave amplitude
    if (wave_amplitude > 0) {
        wave_amplitude *= 0.95f;
    }
    
    // Update mouse force
    if (!mouse_down) {
        mouse_force *= 0.9f;
    }
    
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        
        // Update phase
        p->phase += dt * (0.5f + p->energy * 0.5f);
        
        // Mouse interaction
        float dx = p->x - mouse_x;
        float dy = p->y - mouse_y;
        float dist = sqrtf(dx*dx + dy*dy);
        
        if (dist > 0.1f && dist < 300.0f) {
            float force = mouse_force / (dist + 50.0f);
            
            if (mouse_down) {
                // Attraction when mouse is down
                p->vx -= (dx / dist) * force * 500.0f * dt;
                p->vy -= (dy / dist) * force * 500.0f * dt;
            } else if (mouse_force > 0.1f) {
                // Repulsion when mouse moves fast
                p->vx += (dx / dist) * force * 300.0f * dt;
                p->vy += (dy / dist) * force * 300.0f * dt;
            }
        }
        
        // Wave propagation effect
        if (wave_amplitude > 0.1f) {
            float wave_dx = p->x - wave_origin_x;
            float wave_dy = p->y - wave_origin_y;
            float wave_dist = sqrtf(wave_dx*wave_dx + wave_dy*wave_dy);
            float wave_time = quantum_time * 10.0f - wave_dist * 0.02f;
            float wave_effect = sinf(wave_time) * wave_amplitude * expf(-wave_dist * 0.002f);
            
            p->z += wave_effect * 50.0f * dt;
            p->energy = fmaxf(0.5f, p->energy + wave_effect * 0.5f);
        }
        
        switch (p->type) {
            case 0: // Nebula particles
                // Slow drift with mouse influence
                p->x += p->vx * dt;
                p->y += p->vy * dt;
                
                // React to mouse proximity
                if (dist < 100.0f) {
                    p->size = p->size * (1.0f + (100.0f - dist) * 0.002f);
                    p->a = fminf(0.8f, p->a + (100.0f - dist) * 0.001f);
                }
                
                // Subtle size pulsing
                p->size = p->size * (1.0f + sinf(p->phase * 0.3f) * 0.1f);
                p->a = fmaxf(0.1f, p->a - dt * 0.1f);
                break;
                
            case 1: // Quantum field particles
            case 2: // Entangled particles
                {
                    // Quantum wave function collapse simulation
                    float wave = sinf(quantum_time * 1.5f + p->phase) * cosf(quantum_time * 0.7f);
                    
                    // Energy field influence
                    float field_x = sinf(p->x * 0.01f + quantum_time) * 10.0f;
                    float field_y = cosf(p->y * 0.01f + quantum_time) * 10.0f;
                    
                    // Apply forces with damping
                    p->vx = p->vx * 0.92f + field_x * dt;
                    p->vy = p->vy * 0.92f + field_y * dt;
                    p->vz = p->vz * 0.95f + wave * 5.0f * dt;
                    
                    // Limit velocity
                    float speed = sqrtf(p->vx*p->vx + p->vy*p->vy + p->vz*p->vz);
                    if (speed > 200.0f) {
                        float scale = 200.0f / speed;
                        p->vx *= scale;
                        p->vy *= scale;
                        p->vz *= scale;
                    }
                    
                    p->x += p->vx * dt;
                    p->y += p->vy * dt;
                    p->z += p->vz * dt;
                    
                    // Contain Z axis
                    if (p->z < -200) p->z = -200;
                    if (p->z > 200) p->z = 200;
                    
                    // Energy oscillation
                    p->energy = 1.0f + sinf(p->phase) * 0.5f + mouse_force * 0.5f;
                    
                    // Dynamic opacity based on energy
                    if (p->type == 2) {
                        p->a = 0.6f + p->energy * 0.2f;
                    } else {
                        p->a = 0.4f + wave * 0.1f;
                    }
                    
                    // Size based on energy
                    p->size = 1.0f + p->energy * 0.5f;
                }
                break;
        }
        
        // Life management
        p->life -= dt;
        if (p->life < 3.0f && p->life > 0) {
            p->a *= p->life / 3.0f;
        }
        
        // Remove dead particles
        if (p->life <= 0) {
            particles[i] = particles[--particle_count];
            i--;
        }
    }
}

- (void)writeChar:(char)ch {
    // Handle escape sequences
    if (escape_state > 0) {
        [self processEscapeSequence:ch];
        return;
    }
    
    // Start escape sequence
    if (ch == 27) { // ESC
        escape_state = 1;
        escape_len = 0;
        return;
    }
    
    // Trigger glow effect on character input
    if (ch >= 32 && ch < 127) {
        terminal_glow = 1.0f;
    }
    
    if (ch == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= ROWS) {
            // Scroll with smooth effect
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
    } else if (ch == '\b' || ch == 127) { // Backspace or DEL
        if (cursor_x > 0) {
            cursor_x--;
            terminal[cursor_y][cursor_x].ch = ' ';
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = COLS - 1;
            while (cursor_x > 0 && terminal[cursor_y][cursor_x].ch == ' ') {
                cursor_x--;
            }
            if (terminal[cursor_y][cursor_x].ch != ' ') {
                cursor_x++;
            }
        }
    } else if (ch == '\t') {
        // Tab - move to next 8-column boundary
        cursor_x = ((cursor_x / 8) + 1) * 8;
        if (cursor_x >= COLS) {
            cursor_x = 0;
            cursor_y++;
        }
    } else if (ch == 12) { // Ctrl+L (form feed) - clear screen
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) {
                terminal[y][x].ch = ' ';
                terminal[y][x].r = 0.9f;
                terminal[y][x].g = 0.9f;
                terminal[y][x].b = 0.9f;
            }
        }
        cursor_x = cursor_y = 0;
    } else if (ch >= 32 && ch < 127) {
        if (cursor_y < ROWS && cursor_x < COLS) {
            terminal[cursor_y][cursor_x].ch = ch;
            // Typing glow effect
            terminal[cursor_y][cursor_x].r = 0.9f + terminal_glow * 0.1f;
            terminal[cursor_y][cursor_x].g = 0.9f + terminal_glow * 0.05f;
            terminal[cursor_y][cursor_x].b = 0.9f;
            cursor_x++;
            if (cursor_x >= COLS) {
                cursor_x = 0;
                cursor_y++;
                if (cursor_y >= ROWS) {
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
        // Check for easter egg commands
        static char cmd_buffer[256];
        static int cmd_len = 0;
        
        for (ssize_t i = 0; i < n; i++) {
            [self writeChar:buf[i]];
            
            // Build command buffer
            if (buf[i] == '\n' || buf[i] == '\r') {
                cmd_buffer[cmd_len] = '\0';
                [self checkEasterEggs:cmd_buffer];
                cmd_len = 0;
            } else if (buf[i] >= 32 && buf[i] < 127 && cmd_len < 255) {
                cmd_buffer[cmd_len++] = buf[i];
            }
        }
    }
}

- (void)checkEasterEggs:(const char*)command {
    if (strstr(command, "quantum") || strstr(command, "QUANTUM")) {
        // Quantum storm effect
        for (int i = 0; i < 200 && particle_count < MAX_PARTICLES; i++) {
            Particle *p = &particles[particle_count++];
            p->x = ((float)rand() / RAND_MAX - 0.5f) * 1000;
            p->y = ((float)rand() / RAND_MAX - 0.5f) * 800;
            p->z = ((float)rand() / RAND_MAX - 0.5f) * 400;
            p->vx = ((float)rand() / RAND_MAX - 0.5f) * 200;
            p->vy = ((float)rand() / RAND_MAX - 0.5f) * 200;
            p->vz = ((float)rand() / RAND_MAX - 0.5f) * 100;
            p->r = 0.0f;
            p->g = 0.8f + (float)rand() / RAND_MAX * 0.2f;
            p->b = 1.0f;
            p->a = 1.0f;
            p->life = 5.0f;
            p->size = 5.0f;
            p->energy = 3.0f;
            p->type = 2;
        }
        wave_amplitude = 2.0f;
        wave_origin_x = 400;
        wave_origin_y = 300;
    }
    
    if (strstr(command, "matrix") || strstr(command, "MATRIX")) {
        // Matrix rain effect
        for (int x = 0; x < COLS; x += 2) {
            if (rand() % 100 < 30) {
                for (int y = 0; y < ROWS; y++) {
                    terminal[y][x].r = 0.0f;
                    terminal[y][x].g = 1.0f - (float)y / ROWS;
                    terminal[y][x].b = 0.0f;
                }
            }
        }
    }
    
    if (strstr(command, "neon") || strstr(command, "NEON")) {
        // Neon color mode
        for (int i = 0; i < particle_count; i++) {
            particles[i].r = 1.0f;
            particles[i].g = 0.0f;
            particles[i].b = 1.0f;
        }
    }
    
    if (strstr(command, "galaxy") || strstr(command, "GALAXY")) {
        // Create spiral galaxy
        for (int i = 0; i < 500 && particle_count < MAX_PARTICLES; i++) {
            Particle *p = &particles[particle_count++];
            float angle = ((float)i / 50.0f) * M_PI * 2.0f;
            float radius = 50.0f + i * 0.8f;
            p->x = cosf(angle) * radius;
            p->y = sinf(angle) * radius;
            p->z = ((float)rand() / RAND_MAX - 0.5f) * 50;
            p->vx = -sinf(angle) * 20;
            p->vy = cosf(angle) * 20;
            p->vz = 0;
            p->r = 0.8f;
            p->g = 0.5f + (float)i / 1000.0f;
            p->b = 1.0f;
            p->a = 0.8f;
            p->life = 1000.0f;
            p->size = 2.0f;
            p->energy = 1.0f;
            p->type = 2;
        }
    }
    
    if (strstr(command, "clear particles")) {
        // Clear all particles except nebula
        for (int i = 0; i < particle_count; i++) {
            if (particles[i].type != 0) {
                particles[i].life = 0.1f;
            }
        }
    }
}

- (void)processEscapeSequence:(char)ch {
    if (escape_state == 1) {
        if (ch == '[') {
            escape_state = 2;
            escape_len = 0;
        } else {
            escape_state = 0; // Invalid sequence
        }
    } else if (escape_state == 2) {
        if (ch >= '0' && ch <= '9' || ch == ';') {
            if (escape_len < 255) {
                escape_buffer[escape_len++] = ch;
            }
        } else {
            escape_buffer[escape_len] = '\0';
            [self executeEscapeSequence];
            escape_state = 0;
        }
    }
}

- (void)executeEscapeSequence {
    // Parse ANSI escape sequences
    int params[16] = {0};
    int param_count = 0;
    
    char *p = escape_buffer;
    while (*p && param_count < 16) {
        params[param_count++] = atoi(p);
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
    }
    
    char cmd = escape_buffer[escape_len - 1];
    
    switch (cmd) {
        case 'H': // Cursor position
        case 'f':
            cursor_y = (params[0] > 0 ? params[0] - 1 : 0);
            cursor_x = (params[1] > 0 ? params[1] - 1 : 0);
            if (cursor_y >= ROWS) cursor_y = ROWS - 1;
            if (cursor_x >= COLS) cursor_x = COLS - 1;
            break;
            
        case 'A': // Cursor up
            cursor_y -= (params[0] > 0 ? params[0] : 1);
            if (cursor_y < 0) cursor_y = 0;
            break;
            
        case 'B': // Cursor down
            cursor_y += (params[0] > 0 ? params[0] : 1);
            if (cursor_y >= ROWS) cursor_y = ROWS - 1;
            break;
            
        case 'C': // Cursor forward
            cursor_x += (params[0] > 0 ? params[0] : 1);
            if (cursor_x >= COLS) cursor_x = COLS - 1;
            break;
            
        case 'D': // Cursor back
            cursor_x -= (params[0] > 0 ? params[0] : 1);
            if (cursor_x < 0) cursor_x = 0;
            break;
            
        case 'J': // Clear screen
            if (params[0] == 2 || param_count == 0) {
                // Clear entire screen
                for (int y = 0; y < ROWS; y++) {
                    for (int x = 0; x < COLS; x++) {
                        terminal[y][x].ch = ' ';
                        terminal[y][x].r = 0.9f;
                        terminal[y][x].g = 0.9f;
                        terminal[y][x].b = 0.9f;
                    }
                }
                cursor_x = cursor_y = 0;
            } else if (params[0] == 0) {
                // Clear from cursor to end
                for (int y = cursor_y; y < ROWS; y++) {
                    for (int x = (y == cursor_y ? cursor_x : 0); x < COLS; x++) {
                        terminal[y][x].ch = ' ';
                    }
                }
            } else if (params[0] == 1) {
                // Clear from start to cursor
                for (int y = 0; y <= cursor_y; y++) {
                    for (int x = 0; x < (y == cursor_y ? cursor_x + 1 : COLS); x++) {
                        terminal[y][x].ch = ' ';
                    }
                }
            }
            break;
            
        case 'K': // Clear line
            for (int x = cursor_x; x < COLS; x++) {
                terminal[cursor_y][x].ch = ' ';
            }
            break;
    }
}

- (void)drawFrame {
    [[self openGLContext] makeCurrentContext];
    
    NSRect bounds = [self bounds];
    glViewport(0, 0, bounds.size.width, bounds.size.height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, bounds.size.width / bounds.size.height, 0.1, 1000.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -500);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    
    // Update particles
    static NSTimeInterval lastTime = 0;
    NSTimeInterval currentTime = [NSDate timeIntervalSinceReferenceDate];
    float dt = lastTime ? (currentTime - lastTime) : 0.016f;
    lastTime = currentTime;
    [self updateParticles:dt];
    
    // FIRST: Draw 3D particles and connections in the background
    [self drawQuantumConnections];
    
    // Draw particles in 3D
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    
    // Render particles by type for proper layering
    // First pass: Nebula particles (background)
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        if (p->type == 0) {
            float perspective = 1.0f / (1.0f + p->z * 0.001f);
            float screenX = p->x * perspective;
            float screenY = p->y * perspective;
            float screenSize = p->size * perspective;
            
            glPointSize(screenSize);
            glBegin(GL_POINTS);
            glColor4f(p->r, p->g, p->b, p->a);
            glVertex3f(screenX, screenY, p->z);
            glEnd();
        }
    }
    
    // Second pass: Quantum field particles
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        if (p->type >= 1) {
            float perspective = 1.0f / (1.0f + p->z * 0.002f);
            float screenX = p->x * perspective;
            float screenY = p->y * perspective;
            float screenSize = (p->size + p->energy) * perspective * 2.0f;
            
            // Quantum glow effect
            glPointSize(screenSize * 3.0f);
            glBegin(GL_POINTS);
            glColor4f(p->r * 0.5f, p->g * 0.5f, p->b * 0.5f, p->a * 0.2f);
            glVertex3f(screenX, screenY, p->z);
            glEnd();
            
            // Core particle
            glPointSize(screenSize);
            glBegin(GL_POINTS);
            glColor4f(p->r, p->g, p->b, p->a);
            glVertex3f(screenX, screenY, p->z);
            glEnd();
        }
    }
    
    // SECOND: Switch to 2D and draw black backdrop
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, bounds.size.width, 0, bounds.size.height, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    
    // Draw full screen black backdrop at 80% opacity
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.0f, 0.0f, 0.0f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(bounds.size.width, 0);
    glVertex2f(bounds.size.width, bounds.size.height);
    glVertex2f(0, bounds.size.height);
    glEnd();
    
    // THIRD: Draw terminal text on top
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
    
    // Draw terminal glow effect on top
    [self drawTerminalGlow];
    
    // Restore matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    [[self openGLContext] flushBuffer];
}

- (void)drawQuantumConnections {
    glLineWidth(1.0f);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    
    glBegin(GL_LINES);
    
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        
        if (p->type == 2) { // Entangled particles
            for (int j = 0; j < QUANTUM_CONNECTIONS; j++) {
                int conn = p->connections[j];
                if (conn >= 0 && conn < particle_count) {
                    Particle *p2 = &particles[conn];
                    
                    // Energy-based connection strength
                    float strength = (p->energy + p2->energy) * 0.25f;
                    
                    // Quantum entanglement visualization
                    glColor4f(0.2f, 0.8f, 1.0f, strength * 0.3f);
                    glVertex3f(p->x, p->y, p->z);
                    glVertex3f(p2->x, p2->y, p2->z);
                }
            }
        }
    }
    
    glEnd();
}

- (void)drawTerminalGlow {
    if (terminal_glow > 0.01f) {
        // Switch to 2D overlay
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        NSRect bounds = [self bounds];
        glOrtho(0, bounds.size.width, 0, bounds.size.height, -1, 1);
        
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        
        // Draw glow around cursor
        float glow_radius = 50.0f * terminal_glow;
        float cx = cursor_x * CHAR_WIDTH + CHAR_WIDTH/2;
        float cy = bounds.size.height - cursor_y * CHAR_HEIGHT - CHAR_HEIGHT/2;
        
        for (int i = 0; i < 10; i++) {
            float alpha = terminal_glow * 0.05f * (1.0f - i/10.0f);
            float radius = glow_radius * (1.0f + i * 0.2f);
            
            glColor4f(0.2f, 0.8f, 1.0f, alpha);
            glBegin(GL_LINE_LOOP);
            for (int angle = 0; angle < 360; angle += 30) {
                float rad = angle * M_PI / 180.0f;
                glVertex2f(cx + cosf(rad) * radius, cy + sinf(rad) * radius);
            }
            glEnd();
        }
        
        // Restore matrices
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glEnable(GL_DEPTH_TEST);
    }
}

- (void)keyDown:(NSEvent *)event {
    if (master_fd < 0) return;
    
    NSUInteger modifiers = [event modifierFlags];
    NSString *chars = [event characters];
    NSString *charsIgnoringMods = [event charactersIgnoringModifiers];
    
    if (charsIgnoringMods.length == 0) return;
    
    unichar key = [charsIgnoringMods characterAtIndex:0];
    
    // Handle special keys first
    switch (key) {
        case NSUpArrowFunctionKey:
            write(master_fd, "\033[A", 3);
            return;
        case NSDownArrowFunctionKey:
            write(master_fd, "\033[B", 3);
            return;
        case NSRightArrowFunctionKey:
            write(master_fd, "\033[C", 3);
            return;
        case NSLeftArrowFunctionKey:
            write(master_fd, "\033[D", 3);
            return;
        case NSHomeFunctionKey:
            write(master_fd, "\033[H", 3);
            return;
        case NSEndFunctionKey:
            write(master_fd, "\033[F", 3);
            return;
        case NSPageUpFunctionKey:
            write(master_fd, "\033[5~", 4);
            return;
        case NSPageDownFunctionKey:
            write(master_fd, "\033[6~", 4);
            return;
        case NSDeleteFunctionKey:
            write(master_fd, "\033[3~", 4);
            return;
        case NSF1FunctionKey:
            write(master_fd, "\033OP", 3);
            return;
        case NSF2FunctionKey:
            write(master_fd, "\033OQ", 3);
            return;
        case NSF3FunctionKey:
            write(master_fd, "\033OR", 3);
            return;
        case NSF4FunctionKey:
            write(master_fd, "\033OS", 3);
            return;
    }
    
    // Handle Tab key
    if (key == '\t') {
        write(master_fd, "\t", 1);
        return;
    }
    
    // Handle Enter/Return
    if (key == '\r' || key == '\n') {
        write(master_fd, "\r", 1);
        return;
    }
    
    // Handle Escape
    if (key == 27) {
        write(master_fd, "\033", 1);
        return;
    }
    
    // Handle Backspace
    if (key == 127 || key == 8) {
        write(master_fd, "\x7f", 1);
        return;
    }
    
    // Handle Ctrl combinations
    if (modifiers & NSEventModifierFlagControl) {
        // Special Ctrl combinations
        if (key == ' ') {
            write(master_fd, "\0", 1); // Ctrl+Space
            return;
        }
        
        if (key >= '@' && key <= '_') {
            char ctrl_char = key - '@';
            write(master_fd, &ctrl_char, 1);
            return;
        }
        
        if (key >= 'a' && key <= 'z') {
            char ctrl_char = key - 'a' + 1;
            write(master_fd, &ctrl_char, 1);
            return;
        }
        
        if (key >= 'A' && key <= 'Z') {
            char ctrl_char = key - 'A' + 1;
            write(master_fd, &ctrl_char, 1);
            return;
        }
    }
    
    // Handle Alt/Option combinations
    if (modifiers & NSEventModifierFlagOption) {
        // Send ESC followed by the character for Alt combinations
        write(master_fd, "\033", 1);
        if (chars.length > 0) {
            const char *utf8 = [chars UTF8String];
            write(master_fd, utf8, strlen(utf8));
        }
        return;
    }
    
    // Handle Cmd combinations (pass through some useful ones)
    if (modifiers & NSEventModifierFlagCommand) {
        // Cmd+C (copy), Cmd+V (paste) handled by system
        // But we can add custom handling here if needed
        if (key == 'k' || key == 'K') {
            // Cmd+K - clear terminal
            for (int y = 0; y < ROWS; y++) {
                for (int x = 0; x < COLS; x++) {
                    terminal[y][x].ch = ' ';
                    terminal[y][x].r = 0.9f;
                    terminal[y][x].g = 0.9f;
                    terminal[y][x].b = 0.9f;
                }
            }
            cursor_x = cursor_y = 0;
            return;
        }
    }
    
    // Handle normal characters
    if (chars.length > 0) {
        const char *utf8 = [chars UTF8String];
        write(master_fd, utf8, strlen(utf8));
    }
}

- (void)mouseMoved:(NSEvent *)event {
    NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
    
    // Calculate mouse velocity for force
    float dx = location.x - mouse_x;
    float dy = location.y - mouse_y;
    float velocity = sqrtf(dx*dx + dy*dy);
    
    mouse_x = location.x;
    mouse_y = location.y;
    mouse_force = fminf(1.0f, velocity * 0.02f);
    
    static NSTimeInterval lastParticleTime = 0;
    NSTimeInterval currentTime = [NSDate timeIntervalSinceReferenceDate];
    
    if (currentTime - lastParticleTime > 0.05 && velocity > 5.0f) {
        [self addParticle:location];
        lastParticleTime = currentTime;
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
    mouse_x = location.x;
    mouse_y = location.y;
    mouse_down = 1;
    mouse_force = 1.0f;
    
    // Create quantum wave on click
    wave_origin_x = location.x;
    wave_origin_y = location.y;
    wave_amplitude = 1.0f;
    
    // Spawn explosion of particles
    for (int i = 0; i < 50 && particle_count < MAX_PARTICLES; i++) {
        Particle *p = &particles[particle_count++];
        
        float angle = ((float)rand() / RAND_MAX) * M_PI * 2.0f;
        float speed = 100.0f + ((float)rand() / RAND_MAX) * 300.0f;
        
        p->x = location.x;
        p->y = location.y;
        p->z = ((float)rand() / RAND_MAX - 0.5f) * 100;
        
        p->vx = cosf(angle) * speed;
        p->vy = sinf(angle) * speed;
        p->vz = ((float)rand() / RAND_MAX - 0.5f) * 100;
        
        // Electric burst colors
        p->r = 0.4f + (float)rand() / RAND_MAX * 0.6f;
        p->g = 0.8f + (float)rand() / RAND_MAX * 0.2f;
        p->b = 1.0f;
        p->a = 1.0f;
        
        p->life = 2.0f + (float)rand() / RAND_MAX * 2.0f;
        p->size = 3.0f + (float)rand() / RAND_MAX * 3.0f;
        p->phase = quantum_time;
        p->energy = 2.0f;
        p->type = 1;
    }
}

- (void)mouseUp:(NSEvent *)event {
    mouse_down = 0;
}

- (void)mouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
    
    // Create trail of particles when dragging
    NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
    for (int i = 0; i < 10 && particle_count < MAX_PARTICLES; i++) {
        Particle *p = &particles[particle_count++];
        
        p->x = location.x + ((float)rand() / RAND_MAX - 0.5f) * 20;
        p->y = location.y + ((float)rand() / RAND_MAX - 0.5f) * 20;
        p->z = ((float)rand() / RAND_MAX - 0.5f) * 50;
        
        p->vx = ((float)rand() / RAND_MAX - 0.5f) * 50;
        p->vy = ((float)rand() / RAND_MAX - 0.5f) * 50;
        p->vz = ((float)rand() / RAND_MAX - 0.5f) * 30;
        
        // Trail colors
        p->r = 0.2f;
        p->g = 0.9f;
        p->b = 1.0f;
        p->a = 0.8f;
        
        p->life = 1.5f;
        p->size = 2.0f;
        p->phase = quantum_time;
        p->energy = 1.5f;
        p->type = 1;
    }
}

- (void)rightMouseDown:(NSEvent *)event {
    NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
    
    // Create black hole effect
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        if (p->type != 0) { // Don't affect nebula
            float dx = p->x - location.x;
            float dy = p->y - location.y;
            float dist = sqrtf(dx*dx + dy*dy);
            
            if (dist < 400 && dist > 1) {
                // Create swirl effect
                float angle = atan2f(dy, dx) + M_PI/2;
                float force = 1000.0f / (dist + 10);
                
                p->vx = cosf(angle) * force - dx * 0.5f;
                p->vy = sinf(angle) * force - dy * 0.5f;
                p->vz = (dist < 100) ? ((float)rand() / RAND_MAX - 0.5f) * 100 : 0;
                
                // Color shift based on distance
                p->r = fminf(1.0f, dist / 400.0f);
                p->g = 0.0f;
                p->b = fmaxf(0.0f, 1.0f - dist / 400.0f);
            }
        }
    }
    
    // Add event horizon particles
    for (int i = 0; i < 100 && particle_count < MAX_PARTICLES; i++) {
        Particle *p = &particles[particle_count++];
        
        float angle = ((float)rand() / RAND_MAX) * M_PI * 2.0f;
        float radius = 50 + ((float)rand() / RAND_MAX) * 150;
        
        p->x = location.x + cosf(angle) * radius;
        p->y = location.y + sinf(angle) * radius;
        p->z = ((float)rand() / RAND_MAX - 0.5f) * 50;
        
        // Spiral inward
        p->vx = -sinf(angle) * 100 - cosf(angle) * 50;
        p->vy = cosf(angle) * 100 - sinf(angle) * 50;
        p->vz = 0;
        
        p->r = 1.0f;
        p->g = 0.0f;
        p->b = 0.5f;
        p->a = 0.8f;
        
        p->life = 3.0f;
        p->size = 2.0f;
        p->phase = quantum_time;
        p->energy = 2.0f;
        p->type = 1;
    }
}

- (void)scrollWheel:(NSEvent *)event {
    float deltaY = [event deltaY];
    
    // Zoom effect on particles
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        if (p->type != 0) {
            p->z += deltaY * 10;
            if (p->z < -300) p->z = -300;
            if (p->z > 300) p->z = 300;
        }
    }
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)canBecomeKeyView {
    return YES;
}

- (void)paste:(id)sender {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSString *string = [pasteboard stringForType:NSPasteboardTypeString];
    
    if (string && master_fd >= 0) {
        const char *utf8 = [string UTF8String];
        write(master_fd, utf8, strlen(utf8));
    }
}

- (void)copy:(id)sender {
    // TODO: Implement text selection and copy
    // For now, just beep
    NSBeep();
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
    
    // Make the view the first responder for keyboard input
    [window makeFirstResponder:view];
    
    // Set up tracking for mouse events
    NSTrackingArea *trackingArea = [[NSTrackingArea alloc]
        initWithRect:[view bounds]
        options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect)
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