#ifndef STATIC_H
#define STATIC_H

#include "http.h"

/* Serve a static file from document root.
 * Returns 0 on success, negative on error, or an HTTP status code (404, 403).
 * On success, res->body is malloc'd and must be freed by caller. */
int static_serve(const char *root, const http_request_t *req, http_response_t *res);

#endif
