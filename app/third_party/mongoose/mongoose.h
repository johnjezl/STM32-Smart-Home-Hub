/**
 * Mongoose HTTP Library - Placeholder Header
 *
 * This is a minimal placeholder header for development.
 * For actual functionality, download the real mongoose library:
 *   https://github.com/cesanta/mongoose
 *
 * Replace this file with the real mongoose.h and mongoose.c
 */

#ifndef MONGOOSE_H
#define MONGOOSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <string.h>

// Mongoose structures (minimal definitions for compilation)
struct mg_str {
    const char* buf;
    size_t len;
};

struct mg_mgr {
    void* padding[16];
};

struct mg_connection {
    void* padding[32];
};

struct mg_http_message {
    struct mg_str method;
    struct mg_str uri;
    struct mg_str query;
    struct mg_str proto;
    struct mg_str body;
    struct mg_str headers[32];
};

struct mg_ws_message {
    struct mg_str data;
    unsigned char flags;
};

struct mg_http_serve_opts {
    const char* root_dir;
    const char* ssi_pattern;
    const char* extra_headers;
    const char* mime_types;
    const char* page404;
};

// Event types
#define MG_EV_ERROR 0
#define MG_EV_OPEN 1
#define MG_EV_POLL 2
#define MG_EV_RESOLVE 3
#define MG_EV_CONNECT 4
#define MG_EV_ACCEPT 5
#define MG_EV_TLS_HS 6
#define MG_EV_READ 7
#define MG_EV_WRITE 8
#define MG_EV_CLOSE 9
#define MG_EV_HTTP_MSG 10
#define MG_EV_HTTP_CHUNK 11
#define MG_EV_WS_OPEN 12
#define MG_EV_WS_MSG 13
#define MG_EV_WS_CTL 14
#define MG_EV_MQTT_CMD 15
#define MG_EV_MQTT_MSG 16
#define MG_EV_MQTT_OPEN 17
#define MG_EV_SNTP_TIME 18
#define MG_EV_USER 100

typedef void (*mg_event_handler_t)(struct mg_connection*, int ev, void* ev_data);

// Function declarations (stubs)
static inline void mg_mgr_init(struct mg_mgr* mgr) { (void)mgr; }
static inline void mg_mgr_free(struct mg_mgr* mgr) { (void)mgr; }
static inline void mg_mgr_poll(struct mg_mgr* mgr, int ms) { (void)mgr; (void)ms; }

static inline struct mg_connection* mg_http_listen(struct mg_mgr* mgr, const char* url,
                                                    mg_event_handler_t fn, void* data) {
    (void)mgr; (void)url; (void)fn; (void)data;
    return NULL;  // Will fail to start - replace with real mongoose
}

static inline void mg_http_reply(struct mg_connection* c, int status, const char* headers,
                                  const char* fmt, ...) {
    (void)c; (void)status; (void)headers; (void)fmt;
}

static inline void mg_http_serve_dir(struct mg_connection* c, struct mg_http_message* hm,
                                      const struct mg_http_serve_opts* opts) {
    (void)c; (void)hm; (void)opts;
}

static inline struct mg_str mg_str(const char* s) {
    struct mg_str str = {s, s ? strlen(s) : 0};
    return str;
}

static inline int mg_strcmp(struct mg_str a, struct mg_str b) {
    if (a.len != b.len) return (int)a.len - (int)b.len;
    return memcmp(a.buf, b.buf, a.len);
}

#ifdef __cplusplus
}
#endif

#endif // MONGOOSE_H
