#include "cloudterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>

static ct_server_t *g_server = NULL;

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nShutting down server...\n");
        if (g_server) {
            ct_server_stop(g_server);
        }
    }
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -h, --host HOST          Listen host (default: 0.0.0.0)\n");
    printf("  -p, --port PORT          Listen port (default: 3000)\n");
    printf("  -d, --static-dir DIR     Static files directory\n");
    printf("  -t, --terminal HOST:PORT Terminal server address\n");
    printf("  -P, --password-hash HASH BCrypt password hash\n");
    printf("  -c, --max-connections N  Max connections (default: 10000)\n");
    printf("  -s, --max-sessions N     Max sessions (default: 1000)\n");
    printf("  -T, --session-timeout S  Session timeout in seconds (default: 86400)\n");
    printf("  -C, --compression        Enable compression\n");
    printf("  -S, --ssl                Enable SSL/TLS\n");
    printf("  -v, --version            Show version\n");
    printf("  -?, --help               Show this help\n");
}

static void print_version(void) {
    printf("CloudTerm C Port v1.0.0\n");
    printf("High-performance terminal server\n");
}

int main(int argc, char *argv[]) {
    ct_config_t config = {
        .host = "0.0.0.0",
        .port = 3000,
        .static_dir = "../render-app/public",
        .terminal_host = "142.93.249.123",
        .terminal_port = 7681,
        .password_hash = "$2a$10$YourHashHere",
        .max_connections = 10000,
        .max_sessions = 1000,
        .session_timeout = 86400,
        .enable_compression = false,
        .enable_ssl = false
    };
    
    /* Parse command line options */
    static struct option long_opts[] = {
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"static-dir", required_argument, 0, 'd'},
        {"terminal", required_argument, 0, 't'},
        {"password-hash", required_argument, 0, 'P'},
        {"max-connections", required_argument, 0, 'c'},
        {"max-sessions", required_argument, 0, 's'},
        {"session-timeout", required_argument, 0, 'T'},
        {"compression", no_argument, 0, 'C'},
        {"ssl", no_argument, 0, 'S'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, '?'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "h:p:d:t:P:c:s:T:CSv?", 
                             long_opts, NULL)) != -1) {
        switch (opt) {
            case 'h':
                config.host = optarg;
                break;
            case 'p':
                config.port = atoi(optarg);
                break;
            case 'd':
                config.static_dir = optarg;
                break;
            case 't': {
                char *colon = strchr(optarg, ':');
                if (colon) {
                    *colon = '\0';
                    config.terminal_host = optarg;
                    config.terminal_port = atoi(colon + 1);
                } else {
                    fprintf(stderr, "Invalid terminal address format\n");
                    return 1;
                }
                break;
            }
            case 'P':
                config.password_hash = optarg;
                break;
            case 'c':
                config.max_connections = atoi(optarg);
                break;
            case 's':
                config.max_sessions = atoi(optarg);
                break;
            case 'T':
                config.session_timeout = atoi(optarg);
                break;
            case 'C':
                config.enable_compression = true;
                break;
            case 'S':
                config.enable_ssl = true;
                break;
            case 'v':
                print_version();
                return 0;
            case '?':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    /* Create and start server */
    printf("CloudTerm C Port starting...\n");
    printf("Listen: %s:%d\n", config.host, config.port);
    printf("Terminal: %s:%d\n", config.terminal_host, config.terminal_port);
    printf("Static files: %s\n", config.static_dir);
    
    g_server = ct_server_create(&config);
    if (!g_server) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }
    
    int ret = ct_server_run(g_server);
    
    ct_server_destroy(g_server);
    
    return ret;
}