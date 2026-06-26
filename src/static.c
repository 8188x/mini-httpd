#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "static.h"
#include "mime.h"

/* ─── Path Safety: prevent directory traversal ───────────────────── */

static int is_safe_path(const char *path) {
    /* Reject absolute paths */
    if (path[0] == '/') return 0;
    /* Reject paths containing ".." */
    if (strstr(path, "..") != NULL) return 0;
    /* Reject paths with null bytes */
    if (strlen(path) != strnlen(path, MAX_PATH)) return 0;
    return 1;
}

/* ─── Static File Handler ────────────────────────────────────────── */

int static_serve(const char *root, const http_request_t *req,
                 http_response_t *res) {
    memset(res, 0, sizeof(*res));

    /* Decode URL path: strip query string and leading slash for safety */
    char path[MAX_PATH];
    strncpy(path, req->path, sizeof(path) - 1);

    /* Strip query string */
    char *qmark = strchr(path, '?');
    if (qmark) *qmark = '\0';

    /* URL decode: replace %20 etc. (minimal — just handle spaces for now) */
    char decoded[MAX_PATH];
    int di = 0;
    for (int si = 0; path[si] && di < MAX_PATH - 1; si++) {
        if (path[si] == '%' && path[si+1] && path[si+2]) {
            char hex[3] = {path[si+1], path[si+2], '\0'};
            char *end;
            long val = strtol(hex, &end, 16);
            if (*end == '\0') {
                decoded[di++] = (char)val;
                si += 2;
                continue;
            }
        }
        decoded[di++] = path[si];
    }
    decoded[di] = '\0';

    const char *clean_path = decoded;

    /* If path ends with / or is empty, serve index.html */
    char full_path[MAX_PATH + 256];
    if (clean_path[0] == '\0' || clean_path[strlen(clean_path) - 1] == '/') {
        snprintf(full_path, sizeof(full_path), "%s/%sindex.html",
                 root, clean_path);
    } else {
        /* Strip leading / from clean_path for the filesystem */
        const char *fs_path = clean_path;
        if (fs_path[0] == '/') fs_path++;

        if (!is_safe_path(fs_path)) {
            res->status_code = 403;
            return 403;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", root, fs_path);
    }

    /* Check file exists and get size */
    struct stat st;
    if (stat(full_path, &st) < 0) {
        res->status_code = 404;
        return 404;
    }

    /* Don't serve directories (except via index.html above) */
    if (S_ISDIR(st.st_mode)) {
        /* Try index.html inside the directory */
        char dir_path[MAX_PATH + 256];
        snprintf(dir_path, sizeof(dir_path), "%s/index.html", full_path);
        if (stat(dir_path, &st) < 0) {
            res->status_code = 404;
            return 404;
        }
        strncpy(full_path, dir_path, sizeof(full_path) - 1);
    }

    /* Read file */
    FILE *f = fopen(full_path, "rb");
    if (!f) {
        res->status_code = 403;
        return 403;
    }

    /* Limit to 16MB to avoid OOM */
    if (st.st_size > 16 * 1024 * 1024) {
        fclose(f);
        res->status_code = 413;
        return 413;
    }

    char *body = malloc((size_t)st.st_size + 1);
    if (!body) {
        fclose(f);
        res->status_code = 500;
        return 500;
    }

    size_t nread = fread(body, 1, (size_t)st.st_size, f);
    fclose(f);
    body[nread] = '\0';

    res->status_code = 200;
    res->content_type = mime_type(full_path);
    res->body = body;
    res->body_len = nread;

    return 0;
}
