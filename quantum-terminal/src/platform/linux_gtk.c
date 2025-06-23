// Linux GTK Platform Implementation for Quantum Terminal
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <vte/vte.h>
#include <sys/time.h>
#include "../quantum_terminal.h"

// Global variables
GtkWidget *window;
GtkWidget *gl_area;
GtkWidget *terminal;
VteTerminal *vte;

// Mouse state
float mouse_x = 0, mouse_y = 0;
int mouse_down = 0;

// Time tracking
struct timeval last_time;

// External functions (from quantum implementation)
extern void init_quantum_field(void);
extern void update_particles(float dt);
extern void render_particles(void);
extern void handle_mouse_motion(float x, float y);
extern void handle_mouse_click(float x, float y, int button, int pressed);

// Get delta time
float get_delta_time() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    
    float dt = (current_time.tv_sec - last_time.tv_sec) + 
               (current_time.tv_usec - last_time.tv_usec) / 1000000.0f;
    
    last_time = current_time;
    return dt;
}

// OpenGL initialization
static void gl_init(GtkGLArea *area) {
    gtk_gl_area_make_current(area);
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    
    // Initialize quantum particles
    init_quantum_field();
    
    gettimeofday(&last_time, NULL);
}

// Render callback
static gboolean gl_render(GtkGLArea *area, GdkGLContext *context) {
    // Get delta time
    float dt = get_delta_time();
    
    // Update particles
    update_particles(dt);
    
    // Clear screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Setup projection
    int width = gtk_widget_get_allocated_width(GTK_WIDGET(area));
    int height = gtk_widget_get_allocated_height(GTK_WIDGET(area));
    
    glViewport(0, 0, width, height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (GLfloat)width/(GLfloat)height, 0.1, 1000.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -500);
    
    // Render 3D particles
    render_particles();
    
    // Draw black backdrop
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    glColor4f(0.0f, 0.0f, 0.0f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(width, 0);
    glVertex2f(width, height);
    glVertex2f(0, height);
    glEnd();
    
    glEnable(GL_DEPTH_TEST);
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    return TRUE;
}

// Mouse motion handler
static gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    mouse_x = event->x;
    mouse_y = event->y;
    handle_mouse_motion(mouse_x, mouse_y);
    return TRUE;
}

// Mouse button handler
static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1) {
        mouse_down = 1;
        handle_mouse_click(event->x, event->y, 0, 1);
    } else if (event->button == 3) {
        handle_mouse_click(event->x, event->y, 1, 1);
    }
    return TRUE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == 1) {
        mouse_down = 0;
        handle_mouse_click(event->x, event->y, 0, 0);
    }
    return TRUE;
}

// Animation timer
static gboolean animation_timer(gpointer data) {
    gtk_widget_queue_draw(gl_area);
    return G_SOURCE_CONTINUE;
}

// Create custom terminal widget with OpenGL background
static GtkWidget* create_quantum_terminal() {
    // Create overlay container
    GtkWidget *overlay = gtk_overlay_new();
    
    // Create OpenGL area for particle effects
    gl_area = gtk_gl_area_new();
    gtk_widget_set_hexpand(gl_area, TRUE);
    gtk_widget_set_vexpand(gl_area, TRUE);
    
    g_signal_connect(gl_area, "realize", G_CALLBACK(gl_init), NULL);
    g_signal_connect(gl_area, "render", G_CALLBACK(gl_render), NULL);
    
    // Add mouse event handlers
    gtk_widget_add_events(gl_area, GDK_POINTER_MOTION_MASK | 
                                   GDK_BUTTON_PRESS_MASK | 
                                   GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(gl_area, "motion-notify-event", G_CALLBACK(on_motion_notify), NULL);
    g_signal_connect(gl_area, "button-press-event", G_CALLBACK(on_button_press), NULL);
    g_signal_connect(gl_area, "button-release-event", G_CALLBACK(on_button_release), NULL);
    
    // Create VTE terminal
    terminal = vte_terminal_new();
    vte = VTE_TERMINAL(terminal);
    
    // Configure terminal
    vte_terminal_set_font_from_string(vte, "Monospace 12");
    vte_terminal_set_scrollback_lines(vte, 10000);
    vte_terminal_set_mouse_autohide(vte, TRUE);
    
    // Set terminal colors (semi-transparent background)
    GdkRGBA bg = {0.0, 0.0, 0.0, 0.8};
    GdkRGBA fg = {0.9, 0.9, 0.9, 1.0};
    vte_terminal_set_color_background(vte, &bg);
    vte_terminal_set_color_foreground(vte, &fg);
    
    // Spawn shell
    vte_terminal_spawn_sync(vte,
                           VTE_PTY_DEFAULT,
                           NULL,      // working directory
                           (char*[]){"/bin/bash", NULL},
                           NULL,      // environment
                           G_SPAWN_DEFAULT,
                           NULL,      // child setup
                           NULL,      // child setup data
                           NULL,      // child pid
                           NULL,      // cancellable
                           NULL);     // error
    
    // Add widgets to overlay
    gtk_container_add(GTK_CONTAINER(overlay), gl_area);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), terminal);
    
    return overlay;
}

// Main function
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Quantum Terminal");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Create quantum terminal
    GtkWidget *quantum_terminal = create_quantum_terminal();
    gtk_container_add(GTK_CONTAINER(window), quantum_terminal);
    
    // Show window
    gtk_widget_show_all(window);
    
    // Start animation timer (60 FPS)
    g_timeout_add(16, animation_timer, NULL);
    
    // Run GTK main loop
    gtk_main();
    
    return 0;
}

// Makefile addition for Linux GTK build:
// linux-gtk: 
//     gcc -o quantum-terminal-gtk src/platform/linux_gtk.c \
//         src/quantum.c src/renderer.c \
//         `pkg-config --cflags --libs gtk+-3.0 gtkglext-3.0 vte-2.91` \
//         -lGL -lGLU -lm