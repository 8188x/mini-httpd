#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>

#include "http.h"
#include "static.h"
#include "server.h"

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

    char line[1024];
    int i = 0;
    while (raw[i] && raw[i] != '\r' && raw[i] != '\n' && i < (int)sizeof(line)-1) {
        line[i] = raw[i]; i++;
    }
    line[i] = '\0';

    char method[32], path[1024], version[32];
    if (sscanf(line, "%31s %1023s %31s", method, path, version) < 2)
        return -1;

    strncpy(req->method, method, sizeof(req->method) - 1);
    strncpy(req->path,   path,   sizeof(req->path)   - 1);
    strncpy(req->version,version,sizeof(req->version)- 1);

    const char *p = raw + i;
    while (*p == '\r' || *p == '\n') p++;

    while (*p && *p != '\r' && *p != '\n' && req->nheaders < MAX_HEADERS) {
        char hbuf[512];
        int j = 0;
        while (*p && *p != '\r' && *p != '\n' && j < 511)
            hbuf[j++] = *p++;
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
        "Server: mini-httpd/0.2\r\n"
        "\r\n",
        res->status_code, status,
        res->content_type ? res->content_type : "text/plain",
        res->body_len);

    if (n < 0 || (size_t)n >= cap) return -1;

    if (res->body && res->body_len > 0) {
        size_t remaining = cap - (size_t)n;
        size_t to_copy = res->body_len < remaining ? res->body_len : remaining - 1;
        memcpy(buf + n, res->body, to_copy);
        n += (int)to_copy;
    }
    return n;
}

/* ─── Styled error page generation ───────────────────────────────── */

static void build_error_page(char *buf, size_t cap, int code, const char *msg) {
    const char *hint = "";
    if (code == 404) hint = "The requested resource could not be found.";
    else if (code == 403) hint = "You don't have permission to access this resource.";
    else if (code == 400) hint = "The server could not understand the request.";
    else if (code == 413) hint = "The request entity is too large.";
    else if (code == 500) hint = "An unexpected condition was encountered.";
    else if (code == 501) hint = "The requested method is not supported.";

    snprintf(buf, cap,
        "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">"
        "<title>%d %s</title>"
        "<style>"
        "*{margin:0;padding:0;box-sizing:border-box}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI','PingFang SC',sans-serif;"
        "background:#0b1121;color:#e2e8f0;display:flex;justify-content:center;"
        "align-items:center;height:100vh;margin:0;"
        "background-image:radial-gradient(ellipse 80%% 50%% at 50%% -20%%,rgba(239,68,68,0.06),transparent)}"
        ".c{text-align:center}"
        ".c h1{font-size:5rem;font-weight:800;line-height:1;margin-bottom:4px;"
        "background:linear-gradient(135deg,#f87171,#ef4444);"
        "-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}"
        ".c .msg{color:#94a3b8;font-size:0.9rem;margin-bottom:4px}"
        ".c .hint{color:#475569;font-size:0.75rem}"
        ".c .footer{color:#475569;font-size:0.7rem;margin-top:24px}"
        "</style></head><body>"
        "<div class=\"c\"><h1>%d</h1>"
        "<p class=\"msg\">%s</p>"
        "<p class=\"hint\">%s</p>"
        "<p class=\"footer\">mini-httpd</p>"
        "</div></body></html>",
        code, msg, code, msg, hint);
}

/* ─── Access Log ─────────────────────────────────────────────────── */

static void write_access_log(const http_request_t *req,
                             const http_response_t *res,
                             const struct timeval *start) {
    g_server_stats.total_requests++;
    struct timeval end;
    gettimeofday(&end, NULL);

    long elapsed = (end.tv_sec - start->tv_sec) * 1000000L
                 + (end.tv_usec - start->tv_usec);

    struct tm *lt = localtime(&end.tv_sec);
    char tb[16];
    strftime(tb, sizeof(tb), "%H:%M:%S", lt);

    fprintf(stderr, "\033[90m[%s.%03d]\033[0m %s %s \033[36m%d\033[0m",
            tb, (int)(end.tv_usec / 1000), req->method, req->path, res->status_code);

    if (res->body_len > 0) {
        if (res->body_len < 1024)
            fprintf(stderr, " \033[90m(%zu B)\033[0m", res->body_len);
        else
            fprintf(stderr, " \033[90m(%.1f KB)\033[0m",
                    (double)res->body_len / 1024);
    }
    fprintf(stderr, " \033[90m(%ld μs)\033[0m\n", elapsed);

    /* File access log */
    {
        static FILE *log_fp = NULL;
        if (!log_fp) {
            const char *p = getenv("MINI_HTTPD_LOG");
            log_fp = fopen(p ? p : "/tmp/mini-httpd-access.log", "a");
        }
        if (log_fp) {
            char ip[64] = "-";
            char dbuf[32];
            time_t now_sec = end.tv_sec;
            struct tm *ltm = localtime(&now_sec);
            strftime(dbuf, sizeof(dbuf), "%d/%b/%Y:%H:%M:%S", ltm);
            fprintf(log_fp, "%s - - [%s +0800] \"%s %s HTTP/1.1\" %d %zu %ld\n",
                    ip, dbuf, req->method, req->path,
                    res->status_code, res->body_len, elapsed);
            fflush(log_fp);
        }
    }
}

/* ─── Client Handler ─────────────────────────────────────────────── */

void handle_client(int client_fd, const char *root) {
    struct timeval tv_start;
    gettimeofday(&tv_start, NULL);

    char rbuf[READ_BUF_SIZE];
    ssize_t nread = read(client_fd, rbuf, sizeof(rbuf) - 1);
    if (nread <= 0) return;
    rbuf[nread] = '\0';

    http_request_t req;
    if (http_parse(rbuf, &req) < 0) {
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
        write_access_log(&req, &res, &tv_start);
        return;
    }

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
        write_access_log(&req, &res, &tv_start);
        return;
    }

    /* API: Server Status */
    if (strcmp(req.path, "/api/status") == 0) {
        double uptime = difftime(time(NULL), g_server_stats.start_time);
        int h = (int)uptime / 3600;
        int m = ((int)uptime % 3600) / 60;
        int ss = (int)uptime % 60;
        char json[1024];
        int n = 0;
        n += snprintf(json + n, sizeof(json) - n, "{");
        n += snprintf(json + n, sizeof(json) - n, "\"version\":\"0.2\",");
        n += snprintf(json + n, sizeof(json) - n, "\"requests\":%lu,", g_server_stats.total_requests);
        n += snprintf(json + n, sizeof(json) - n, "\"bytes\":%lu,", g_server_stats.total_bytes);
        n += snprintf(json + n, sizeof(json) - n, "\"uptime\":%.0f,", uptime);
        n += snprintf(json + n, sizeof(json) - n, "\"uptime_str\":\"%dh %dm %ds\",", h, m, ss);
        n += snprintf(json + n, sizeof(json) - n, "\"status\":\"running\"");
        n += snprintf(json + n, sizeof(json) - n, "}");
        http_response_t res = {
            .status_code = 200,
            .content_type = "application/json; charset=utf-8",
            .body = json,
            .body_len = (size_t)n,
        };
        char wbuf[4096];
        int wlen = http_build_response(&res, wbuf, sizeof(wbuf));
        if (wlen > 0) write(client_fd, wbuf, (size_t)wlen);
        g_server_stats.total_bytes += (unsigned long)(wlen > 0 ? wlen : 0);
        write_access_log(&req, &res, &tv_start);
        return;
    }

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

    if (err == 0 && res.body)
        free((void*)res.body);

    write_access_log(&req, &res, &tv_start);
}
