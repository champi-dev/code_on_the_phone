#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <math.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
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

void add_particle(float x, float y) {
    if (particle_count >= 1000) return;
    
    Particle *p = &particles[particle_count++];
    p->x = x;
    p->y = y;
    p->z = 0;
    p->vx = (rand() / (float)RAND_MAX - 0.5) * 200;
    p->vy = (rand() / (float)RAND_MAX - 0.5) * 200 + 100;
    p->vz = 0;
    
    // Quantum colors - cyan to purple
    float t = rand() / (float)RAND_MAX;
    if (t < 0.5) {
        p->r = 0;
        p->g = 1 - t;
        p->b = 1;
    } else {
        p->r = (t - 0.5) * 2;
        p->g = 0;
        p->b = 1;
    }
    p->a = 1.0;
    p->life = 3.0;
}

void update_particles(float dt) {
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        
        // Physics
        p->vy -= 400 * dt; // Gravity
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        
        // Fade out
        p->life -= dt;
        p->a = p->life / 3.0;
        
        // Remove dead particles
        if (p->life <= 0) {
            particles[i] = particles[--particle_count];
            i--;
        }
    }
}

void init_terminal() {
    // Clear terminal
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            terminal[y][x].ch = ' ';
            terminal[y][x].r = 0.9;
            terminal[y][x].g = 0.9;
            terminal[y][x].b = 0.9;
        }
    }
    
    // Welcome message
    const char *msg = "Quantum Terminal - Move mouse for particles!";
    for (int i = 0; msg[i] && i < COLS; i++) {
        terminal[0][i].ch = msg[i];
        terminal[0][i].r = 0;
        terminal[0][i].g = 1;
        terminal[0][i].b = 1;
    }
    
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
    
    cursor_y = 2;
}

void write_char(char ch) {
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

void update_terminal() {
    if (master_fd < 0) return;
    
    char buf[1024];
    ssize_t n = read(master_fd, buf, sizeof(buf));
    if (n > 0) {
        for (ssize_t i = 0; i < n; i++) {
            write_char(buf[i]);
        }
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Draw terminal
    glColor3f(1, 1, 1);
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            if (terminal[y][x].ch != ' ') {
                glRasterPos2f(x * CHAR_WIDTH, 600 - (y + 1) * CHAR_HEIGHT);
                glColor3f(terminal[y][x].r, terminal[y][x].g, terminal[y][x].b);
                glutBitmapCharacter(GLUT_BITMAP_9_BY_15, terminal[y][x].ch);
            }
        }
    }
    
    // Draw cursor
    if (cursor_x < COLS && cursor_y < ROWS) {
        glColor3f(0, 1, 0);
        glBegin(GL_LINES);
        glVertex2f(cursor_x * CHAR_WIDTH, 600 - cursor_y * CHAR_HEIGHT);
        glVertex2f(cursor_x * CHAR_WIDTH, 600 - (cursor_y + 1) * CHAR_HEIGHT);
        glEnd();
    }
    
    // Draw particles
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glPointSize(3.0);
    
    glBegin(GL_POINTS);
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        glColor4f(p->r, p->g, p->b, p->a);
        glVertex2f(p->x, p->y);
    }
    glEnd();
    
    glutSwapBuffers();
}

void idle() {
    static double last_time = 0;
    double current_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
    float dt = current_time - last_time;
    last_time = current_time;
    
    update_terminal();
    update_particles(dt);
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    if (master_fd >= 0) {
        write(master_fd, &key, 1);
    }
}

void mouse_motion(int x, int y) {
    static double last_particle_time = 0;
    double current_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
    
    if (current_time - last_particle_time > 0.05) {
        add_particle(x, 600 - y);
        last_particle_time = current_time;
    }
}

void mouse_click(int button, int state, int x, int y) {
    if (state == GLUT_DOWN) {
        for (int i = 0; i < 20; i++) {
            add_particle(x + (rand() % 20 - 10), 600 - y + (rand() % 20 - 10));
        }
    }
}

int main(int argc, char *argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(COLS * CHAR_WIDTH, ROWS * CHAR_HEIGHT);
    glutCreateWindow("Quantum Terminal");
    
    glClearColor(0.05, 0.05, 0.1, 1.0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, COLS * CHAR_WIDTH, 0, ROWS * CHAR_HEIGHT, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    
    init_terminal();
    
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutKeyboardFunc(keyboard);
    glutMotionFunc(mouse_motion);
    glutPassiveMotionFunc(mouse_motion);
    glutMouseFunc(mouse_click);
    
    glutMainLoop();
    return 0;
}