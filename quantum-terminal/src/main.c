#include "quantum_terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

static volatile bool g_running = true;

void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Quantum Terminal - High Performance Terminal Emulator\n");
    printf("========================================\n\n");
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create window
    void *window = qt_platform_create_window("Quantum Terminal", 1024, 768);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        return 1;
    }
    
    // Main loop
    double last_time = qt_platform_get_time();
    
    while (g_running) {
        double current_time = qt_platform_get_time();
        float dt = (float)(current_time - last_time);
        last_time = current_time;
        
        // Poll events
        qt_platform_poll_events(window);
        
        // Cap frame time
        if (dt > 0.1f) dt = 0.1f;
    }
    
    // Cleanup
    qt_platform_destroy_window(window);
    
    printf("\nQuantum Terminal shut down successfully.\n");
    return 0;
}