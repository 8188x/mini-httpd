#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "http.h"
#include "static.h"

/* ─── Status Texts ───────────────────────────────────────────────── */

static const char *status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default:  return "Unknown";
    }
}

const char *http_status_text(int code) { return status_text(code); }

/* ─── HTTP Request Parsing ───────────────────────────────────────── */

static char *strtrim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s) - 1;
    while (e > s && (*e == ' ' || *e == '\t' || *e == '\r')) e--;
    *(e + 1) = '\0';
    return s;
}

int http_parse(const char *raw, http_request_t *req) {
    memset(req, 0, sizeof(*req));

    /* Parse request line: "METHOD /path HTTP/1.1\r\n" */
    char line[1024];
    int i = 0;
    while (raw[i] && raw[i] != '\r' && raw[i] != '\n' && i < (int)sizeof(line)-1) {
        line[i] = raw[i]; i++;
    }
    line[i] = '\0';

    char method[32], path[1024], version[32];
    if (sscanf(line, "%31s %1023s %31s", method, path, version) < 2) {
        return -1;
    }

    strncpy(req->method, method, sizeof(req->method) - 1);
    strncpy(req->path,   path,   sizeof(req->path)   - 1);
    strncpy(req->version,version,sizeof(req->version)- 1);

    /* Parse headers */
    const char *p = raw + i;
    while (*p == '\r' || *p == '\n') p++;

    while (*p && *p != '\r' && *p != '\n' && req->nheaders < MAX_HEADERS) {
        char hbuf[512];
        int j = 0;
        while (*p && *p != '\r' && *p != '\n' && j < 511) {
            hbuf[j++] = *p++;
        }
        hbuf[j] = '\0';

        char *colon = strchr(hbuf, ':');
        if (colon) {
            *colon = '\0';
            strncpy(req->headers[req->nheaders][0], strtrim(hbuf),
                    sizeof(req->headers[0][0]) - 1);
            strncpy(req->headers[req->nheaders][1], strtrim(colon + 1),
                    sizeof(req->headers[0][1]) - 1);
            req->nheaders++;
        }

        while (*p == '\r' || *p == '\n') p++;
    }

    return 0;
}

/* ─── Response Building ──────────────────────────────────────────── */

int http_build_response(const http_response_t *res, char *buf, size_t cap) {
    const char *status = status_text(res->status_code);

    int n = snprintf(buf, cap,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Server: mini-httpd/0.1\r\n"
        "\r\n",
        res->status_code, status,
        res->content_type ? res->content_type : "text/plain",
        res->body_len);

    if (n < 0 || (size_t)n >= cap) return -1;

    /* Append body if it fits */
    if (res->body && res->body_len > 0) {
        size_t remaining = cap - (size_t)n;
        size_t to_copy = res->body_len < remaining ? res->body_len : remaining - 1;
        memcpy(buf + n, res->body, to_copy);
        n += (int)to_copy;
    }

    return n;
}

/* ─── Build error page body ──────────────────────────────────────── */

static void build_error_page(char *buf, size_t cap, int code, const char *msg) {
    snprintf(buf, cap,
        "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"UTF-8\">"
        "<title>%d %s</title>"
        "<style>body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;"
        "background:#f0f2f7;display:flex;justify-content:center;"
        "align-items:center;height:100vh;margin:0;color:#1e293b}"
        ".c{text-align:center}.c h1{font-size:4rem;margin:0;color:#2c3e7a}"
        ".c p{font-size:1rem;color:#64748b}</style></head><body>"
        "<div class=\"c\"><h1>%d</h1><p>%s</p></div></body></html>",
        code, msg, code, msg);
}

/* ─── Client Handler ─────────────────────────────────────────────── */

void handle_client(int client_fd, const char *root) {
    char rbuf[READ_BUF_SIZE];
    ssize_t nread = read(client_fd, rbuf, sizeof(rbuf) - 1);

    if (nread <= 0) return;

    rbuf[nread] = '\0';

    /* Parse request */
    http_request_t req;
    if (http_parse(rbuf, &req) < 0) {
        /* Bad Request */
        char body[1024];
        build_error_page(body, sizeof(body), 400, "Bad Request");
        http_response_t res = {
            .status_code = 400,
            .content_type = "text/html; charset=utf-8",
            .body = body,
            .body_len = strlen(body),
        };
        char wbuf[RESP_BUF_SIZE];
        int len = http_build_response(&res, wbuf, sizeof(wbuf));
        if (len > 0) write(client_fd, wbuf, (size_t)len);
        return;
    }

    /* Only GET and HEAD are supported */
    if (strcmp(req.method, "GET") != 0 && strcmp(req.method, "HEAD") != 0) {
        char body[1024];
        build_error_page(body, sizeof(body), 501, "Not Implemented");
        http_response_t res = {
            .status_code = 501,
            .content_type = "text/html; charset=utf-8",
            .body = body,
            .body_len = strlen(body),
        };
        char wbuf[RESP_BUF_SIZE];
        int len = http_build_response(&res, wbuf, sizeof(wbuf));
        if (len > 0) write(client_fd, wbuf, (size_t)len);
        return;
    }

    /* Serve static file */
    http_response_t res;
    int err = static_serve(root, &req, &res);

    if (err != 0) {
        char body[1024];
        build_error_page(body, sizeof(body), err,
            err == 404 ? "Not Found" :
            err == 403 ? "Forbidden" : "Internal Server Error");
        res.status_code = err;
        res.content_type = "text/html; charset=utf-8";
        res.body = body;
        res.body_len = strlen(body);
    }

    char wbuf[RESP_BUF_SIZE];
    int len = http_build_response(&res, wbuf, sizeof(wbuf));
    if (len > 0) write(client_fd, wbuf, (size_t)len);

    /* Free dynamically allocated body */
    if (err == 0 && res.body) {
        free((void*)res.body);
    }
}
