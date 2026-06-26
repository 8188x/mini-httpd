#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "server.h"

static volatile int running = 1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static void print_banner(int port, const char *root) {
    printf("\n");
    printf("  ⚡  mini-httpd  —  Static File HTTP Server\n");
    printf("  ─────────────────────────────────────────\n");
    printf("  🚀  http://localhost:%d\n", port);
    printf("  📁  Serving: %s\n", root);
    printf("  🔧  Press Ctrl+C to stop\n");
    printf("\n");
}

int main(int argc, char *argv[]) {
    int port = 8080;
    char root[MAX_PATH] = "www";

    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "Invalid port: %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            strncpy(root, argv[++i], sizeof(root) - 1);
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [-p port] [-r document_root]\n", argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            fprintf(stderr, "Usage: %s [-p port] [-r document_root]\n", argv[0]);
            return 1;
        }
    }

    /* Ignore SIGPIPE so write() to closed sockets doesn't crash us */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_signal);

    /* Start server */
    server_t srv;
    if (server_init(&srv, port, root) != 0) {
        fprintf(stderr, "Failed to start server on port %d\n", port);
        return 1;
    }

    print_banner(port, root);
    server_run(&srv);
    server_close(&srv);

    printf("\n  👋  Server stopped.\n\n");
    return 0;
}
