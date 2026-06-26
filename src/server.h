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
