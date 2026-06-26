#ifndef SERVER_H
#define SERVER_H

#include "http.h"

typedef struct {
    int fd;
    int port;
    char root[MAX_PATH];
} server_t;

int  server_init(server_t *srv, int port, const char *root);
void server_run(server_t *srv);
void server_close(server_t *srv);

#endif
#define MAX_EVENTS 64
#include <time.h>
/* ─── Server Stats (tracked globally, accessed via /api/status) ─── */

typedef struct {
    unsigned long total_requests;
    unsigned long total_bytes;
    time_t start_time;
    char root[MAX_PATH];
} server_stats_t;

extern server_stats_t g_server_stats;
