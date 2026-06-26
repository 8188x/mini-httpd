#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/event.h>
#include <sys/time.h>

#include "server.h"
#include "http.h"

/* ─── Socket Helpers ─────────────────────────────────────────────── */

static int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* ─── Initialization ─────────────────────────────────────────────── */

int server_init(server_t *srv, int port, const char *root) {
    srv->port = port;
    srv->fd   = -1;
    strncpy(srv->root, root, sizeof(srv->root) - 1);

    /* Create TCP socket */
    srv->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(srv->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    set_nonblock(srv->fd);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(port),
        .sin_addr   = { htonl(INADDR_ANY) },
    };

    if (bind(srv->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(srv->fd); return -1;
    }
    if (listen(srv->fd, 128) < 0) {
        perror("listen"); close(srv->fd); return -1;
    }

    return 0;
}

/* ─── Event Loop ─────────────────────────────────────────────────── */

void server_run(server_t *srv) {
    int kq = kqueue();
    if (kq < 0) { perror("kqueue"); return; }

    struct kevent ev;
    EV_SET(&ev, srv->fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &ev, 1, NULL, 0, NULL);

    struct kevent events[MAX_EVENTS];
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    for (;;) {
        int n = kevent(kq, NULL, 0, events, MAX_EVENTS, NULL);
        if (n < 0) { perror("kevent"); break; }

        for (int i = 0; i < n; i++) {
            if (events[i].flags & EV_EOF) {
                /* Client disconnected */
                close((int)events[i].ident);
                continue;
            }

            if (events[i].ident == (uintptr_t)srv->fd) {
                /* New connection */
                int client_fd = accept(srv->fd,
                    (struct sockaddr*)&client_addr, &addrlen);
                if (client_fd < 0) continue;
                set_nonblock(client_fd);
                EV_SET(&ev, client_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                kevent(kq, &ev, 1, NULL, 0, NULL);
            } else {
                /* Client has data to read */
                int client_fd = events[i].ident;
                handle_client(client_fd, srv->root);
                close(client_fd);
            }
        }
    }

    close(kq);
}

/* ─── Cleanup ────────────────────────────────────────────────────── */

void server_close(server_t *srv) {
    if (srv->fd >= 0) close(srv->fd);
}
