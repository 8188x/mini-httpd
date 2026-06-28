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

/* ─── Config file parser ─────────────────────────────────────── */
static void parse_config(const char *path, int *port, char *root, int *max_events) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r') continue;
        char key[64], val[192];
        if (sscanf(p, "%63[^=]=%191s", key, val) == 2) {
            if (strcmp(key, "port") == 0) {
                int v = atoi(val);
                if (v > 0 && v <= 65535) *port = v;
            } else if (strcmp(key, "root") == 0) {
                strncpy(root, val, MAX_PATH - 1);
            } else if (strcmp(key, "max_events") == 0) {
                *max_events = atoi(val);
            }
        }
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    int port = 8080;
    int max_events = MAX_EVENTS;
    char root[MAX_PATH] = "www";
    char config_path[MAX_PATH] = "server.conf";

    /* Parse CLI args first to find config file */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            strncpy(config_path, argv[++i], sizeof(config_path) - 1);
        }
    }

    /* Read config file (overrides hardcoded defaults) */
    parse_config(config_path, &port, root, &max_events);

    /* Re-parse CLI args (override config file values) */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            i++; continue;  /* already handled */
        }
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "Invalid port: %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            strncpy(root, argv[++i], sizeof(root) - 1);
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [-p port] [-r root] [-c config]\n", argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            fprintf(stderr, "Usage: %s [-p port] [-r root] [-c config]\n", argv[0]);
            return 1;
        }
    }
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
