#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

#include "static.h"
#include "mime.h"

/* ─── Path Safety ───────────────────────────────────────────────── */

static int is_safe_path(const char *path) {
    if (path[0] == '/') return 0;
    if (strstr(path, "..") != NULL) return 0;
    if (strlen(path) != strnlen(path, MAX_PATH)) return 0;
    return 1;
}

/* ─── Size Formatting ───────────────────────────────────────────── */

static void format_size(off_t size, char *buf, size_t cap) {
    if (size < 1024)
        snprintf(buf, cap, "%ld B", (long)size);
    else if (size < 1024 * 1024)
        snprintf(buf, cap, "%.1f KB", (double)size / 1024);
    else if (size < 1024LL * 1024 * 1024)
        snprintf(buf, cap, "%.1f MB", (double)size / (1024 * 1024));
    else
        snprintf(buf, cap, "%.1f GB", (double)size / (1024LL * 1024 * 1024));
}

/* ─── File Type Icon ────────────────────────────────────────────── */

static const char *file_icon(mode_t mode, const char *name) {
    if (S_ISDIR(mode)) return "\xF0\x9F\x93\x81";
    const char *ext = strrchr(name, '.');
    if (!ext) return "\xF0\x9F\x93\x84";
    if (strcmp(ext,".html")==0||strcmp(ext,".htm")==0) return "\xF0\x9F\x8C\x90";
    if (strcmp(ext,".css")==0) return "\xF0\x9F\x8E\xA8";
    if (strcmp(ext,".js")==0) return "\xE2\x9A\xA1";
    if (strcmp(ext,".json")==0||strcmp(ext,".xml")==0) return "\xF0\x9F\x93\x8B";
    if (strcmp(ext,".md")==0) return "\xF0\x9F\x93\x9D";
    if (strcmp(ext,".png")==0||strcmp(ext,".jpg")==0||strcmp(ext,".jpeg")==0) return "\xF0\x9F\x96\xBC";
    if (strcmp(ext,".gif")==0||strcmp(ext,".svg")==0) return "\xF0\x9F\x96\xBC";
    if (strcmp(ext,".pdf")==0) return "\xF0\x9F\x93\x95";
    if (strcmp(ext,".zip")==0||strcmp(ext,".tar")==0||strcmp(ext,".gz")==0) return "\xF0\x9F\x93\xA6";
    if (strcmp(ext,".c")==0||strcmp(ext,".h")==0) return "\xF0\x9F\x94\xA7";
    return "\xF0\x9F\x93\x84";
}

/* ─── Directory Listing ─────────────────────────────────────────── */

static int static_serve_dir(const char *dir_path, const char *url_path,
                            http_response_t *res) {
    DIR *d = opendir(dir_path);
    if (!d) return 403;

    char *buf = malloc(RESP_BUF_SIZE);
    if (!buf) { closedir(d); return 500; }
    int pos = 0;

#define D(fmt, ...) do { \
    int r = snprintf(buf + pos, RESP_BUF_SIZE - pos, fmt, ##__VA_ARGS__); \
    if (r < 0) { closedir(d); free(buf); return 500; } \
    pos += r; \
    if (pos >= RESP_BUF_SIZE) { closedir(d); free(buf); return 500; } \
} while(0)

    D("<!DOCTYPE html><html lang=\"zh-CN\"><head>"
      "<meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">"
      "<title>Index of %s</title>"
      "<style>"
      "*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}"
      "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI','PingFang SC',sans-serif;"
      "background:#0b1121;color:#e2e8f0;min-height:100vh}"
      ".container{max-width:800px;margin:0 auto;padding:40px 24px}"
      "h1{font-size:1.15rem;font-weight:600;color:#f1f5f9;margin-bottom:20px;display:flex;align-items:center;gap:8px}"
      "table{width:100%%;border-collapse:collapse}"
      "th{text-align:left;padding:10px 12px;font-size:0.68rem;text-transform:uppercase;"
      "letter-spacing:0.5px;color:#64748b;border-bottom:1px solid #1e293b;font-weight:600}"
      "td{padding:10px 12px;font-size:0.82rem;border-bottom:1px solid #0f172a}"
      "tr:hover td{background:rgba(255,255,255,0.02)}"
      "a{color:#e2e8f0;text-decoration:none;transition:color 0.15s}"
      "a:hover{color:#60a5fa}"
      ".size{color:#64748b;font-size:0.75rem;font-family:'SF Mono','Menlo',monospace}"
      ".date{color:#475569;font-size:0.72rem}"
      ".footer{text-align:center;padding:24px;font-size:0.7rem;color:#475569}"
      ".parent{color:#60a5fa}"
      "@media(max-width:640px){.container{padding:24px 12px}}"
      "</style></head><body><div class=\"container\">"
      "<h1>\xF0\x9F\x93\x81 Index of %s</h1><table>"
      "<tr><th>Type</th><th>Name</th><th>Size</th><th>Last Modified</th></tr>",
      url_path, url_path);

    /* Parent directory link */
    D("<tr><td>\xF0\x9F\x93\x81</td>"
      "<td><a class=\"parent\" href=\"../\">../</a></td>"
      "<td class=\"size\">-</td><td class=\"date\">-</td></tr>");

    /* Read directory entries (no sorting for simplicity) */
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char entry_path[MAX_PATH + 256];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", dir_path, entry->d_name);

        struct stat est;
        if (stat(entry_path, &est) < 0) continue;

        char size_str[32];
        if (S_ISDIR(est.st_mode))
            snprintf(size_str, sizeof(size_str), "-");
        else
            format_size(est.st_size, size_str, sizeof(size_str));

        char date_str[32];
        struct tm *tm = localtime(&est.st_mtime);
        strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M", tm);

        const char *icon = file_icon(est.st_mode, entry->d_name);
        char url_name[MAX_PATH + 16];
        if (S_ISDIR(est.st_mode))
            snprintf(url_name, sizeof(url_name), "%s/", entry->d_name);
        else
            snprintf(url_name, sizeof(url_name), "%s", entry->d_name);

        /* HTML-encode the display name for safety */
        char display[256];
        int di = 0;
        for (int si = 0; entry->d_name[si] && di < 250; si++) {
            if (entry->d_name[si] == '<') { display[di++] = '&'; display[di++] = 'l'; display[di++] = 't'; display[di++] = ';'; }
            else if (entry->d_name[si] == '>') { display[di++] = '&'; display[di++] = 'g'; display[di++] = 't'; display[di++] = ';'; }
            else if (entry->d_name[si] == '&') { display[di++] = '&'; display[di++] = 'a'; display[di++] = 'm'; display[di++] = 'p'; display[di++] = ';'; }
            else display[di++] = entry->d_name[si];
        }
        display[di] = '\0';

        D("<tr><td>%s</td><td><a href=\"%s\">%s</a></td>"
          "<td class=\"size\">%s</td><td class=\"date\">%s</td></tr>",
          icon, url_name, display, size_str, date_str);
    }

    D("</table></div><div class=\"footer\">mini-httpd</div></body></html>");

    closedir(d);

    res->status_code = 200;
    res->content_type = "text/html; charset=utf-8";
    res->body = buf;
    res->body_len = (size_t)pos;
    return 0;

#undef D
}

/* ─── Static File Handler ────────────────────────────────────────── */

int static_serve(const char *root, const http_request_t *req,
                 http_response_t *res) {
    memset(res, 0, sizeof(*res));

    char path[MAX_PATH];
    strncpy(path, req->path, sizeof(path) - 1);

    /* Strip query string */
    char *qmark = strchr(path, '?');
    if (qmark) *qmark = '\0';

    /* URL decode (minimal — handles %20 etc.) */
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

    /* Build filesystem path */
    char full_path[MAX_PATH + 256];
    int is_dir_request = 0;

    if (clean_path[0] == '\0' || clean_path[strlen(clean_path) - 1] == '/') {
        /* Ends with / → always a directory request */
        is_dir_request = 1;
        snprintf(full_path, sizeof(full_path), "%s/%sindex.html",
                 root, clean_path);
    } else {
        const char *fs_path = clean_path;
        if (fs_path[0] == '/') fs_path++;

        if (!is_safe_path(fs_path)) {
            res->status_code = 403;
            return 403;
        }
        snprintf(full_path, sizeof(full_path), "%s/%s", root, fs_path);
    }

    struct stat st;
    if (stat(full_path, &st) < 0) {
        if (is_dir_request) {
            /* index.html not found — show directory listing */
            char dir_path[MAX_PATH + 256];
            snprintf(dir_path, sizeof(dir_path), "%s/%s", root, clean_path);
            if (stat(dir_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                const char *url = (clean_path[0] == '\0') ? "/" : clean_path;
                return static_serve_dir(dir_path, url, res);
            }
        }
        res->status_code = 404;
        return 404;
    }

    /* If it's a directory, look for index.html or show listing */
    if (S_ISDIR(st.st_mode)) {
        char idx_path[MAX_PATH + 256];
        snprintf(idx_path, sizeof(idx_path), "%s/index.html", full_path);
        if (stat(idx_path, &st) < 0) {
            /* No index.html — directory listing */
            return static_serve_dir(full_path, clean_path, res);
        }
        strncpy(full_path, idx_path, sizeof(full_path) - 1);
    }

    /* Read file */
    FILE *f = fopen(full_path, "rb");
    if (!f) { res->status_code = 403; return 403; }

    if (st.st_size > 16 * 1024 * 1024) {
        fclose(f); res->status_code = 413; return 413;
    }

    char *body = malloc((size_t)st.st_size + 1);
    if (!body) { fclose(f); res->status_code = 500; return 500; }

    size_t nread = fread(body, 1, (size_t)st.st_size, f);
    fclose(f);
    body[nread] = '\0';

    res->status_code = 200;
    res->content_type = mime_type(full_path);
    res->body = body;
    res->body_len = nread;

    return 0;
}
