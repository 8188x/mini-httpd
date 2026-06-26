#ifndef HTTP_H
#define HTTP_H
#define MAX_METHOD 16
#define MAX_PATH 1024
#define MAX_HEADERS 64
#define READ_BUF_SIZE 16384
#define RESP_BUF_SIZE 65536

#include <stddef.h>

#define MAX_HEADERS 64
#define MAX_METHOD 16
#define MAX_PATH 1024

/* Parsed HTTP request */
typedef struct {
    char method[MAX_METHOD];
    char path[MAX_PATH];
    char version[16];
    char headers[MAX_HEADERS][2][256];
    int  nheaders;
} http_request_t;

/* HTTP response (status + content) */
typedef struct {
    int   status_code;
    const char *status_text;
    const char *content_type;
    const char *body;
    size_t body_len;
} http_response_t;

/* Parse an HTTP request from raw text */
int http_parse(const char *raw, http_request_t *req);

/* Build full HTTP response string (headers + body) into buf */
int http_build_response(const http_response_t *res, char *buf, size_t cap);

/* Read request from fd, parse, handle, and write response */
void handle_client(int client_fd, const char *root);

/* Standard status texts */
extern const char *http_status_text(int code);

#endif
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
