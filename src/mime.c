#include <string.h>
#include "mime.h"

static const struct {
    const char *ext;
    const char *mime;
} mime_map[] = {
    {".html", "text/html; charset=utf-8"},
    {".htm",  "text/html; charset=utf-8"},
    {".css",  "text/css; charset=utf-8"},
    {".js",   "application/javascript; charset=utf-8"},
    {".json", "application/json; charset=utf-8"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".svg",  "image/svg+xml"},
    {".ico",  "image/x-icon"},
    {".webp", "image/webp"},
    {".woff", "font/woff"},
    {".woff2","font/woff2"},
    {".ttf",  "font/ttf"},
    {".pdf",  "application/pdf"},
    {".zip",  "application/zip"},
    {".md",   "text/markdown; charset=utf-8"},
    {".txt",  "text/plain; charset=utf-8"},
    {".xml",  "application/xml; charset=utf-8"},
    {".mp4",  "video/mp4"},
    {".mp3",  "audio/mpeg"},
    {".wasm", "application/wasm"},
    {NULL, NULL}
};

const char *mime_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";

    for (int i = 0; mime_map[i].ext; i++) {
        if (strcasecmp(dot, mime_map[i].ext) == 0)
            return mime_map[i].mime;
    }
    return "application/octet-stream";
}
