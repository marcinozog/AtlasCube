/*
 * icy_http_stream — minimal HTTP audio reader for esp-adf with Shoutcast/Icecast
 * ICY metadata support.
 *
 * Replacement for esp-adf's http_stream that does not require patching the
 * framework. Supports plain MP3/AAC/WAV streams over HTTP/HTTPS with optional
 * ICY (Icy-MetaData) parsing. Does NOT implement HLS, AES decryption, gzip
 * content-encoding or playlist (PLS/M3U) resolving.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "audio_element.h"
#include "esp_err.h"
#include "esp_http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ICY_HTTP_EVENT_PRE_REQUEST,    /*!< Before HTTP request is sent */
    ICY_HTTP_EVENT_POST_REQUEST,   /*!< After HTTP response headers parsed */
    ICY_HTTP_EVENT_FINISH_TRACK,   /*!< Stream ended (server closed / EOF) */
} icy_http_event_id_t;

typedef struct {
    icy_http_event_id_t       event_id;
    esp_http_client_handle_t  client;
    audio_element_handle_t    el;
    void                     *user_data;
    int                       icy_metaint;  /*!< Meta interval in bytes; 0 if no ICY */
} icy_http_event_msg_t;

typedef int  (*icy_http_event_cb_t)(icy_http_event_msg_t *msg);
typedef void (*icy_http_metadata_cb_t)(const char *meta);

typedef struct {
    int                 task_stack;        /*!< Internal element task stack */
    int                 task_prio;         /*!< Element task priority */
    int                 task_core;         /*!< Element task core */
    int                 out_rb_size;       /*!< Output ringbuffer size */
    int                 buffer_len;        /*!< Internal read buffer */
    int                 timeout_ms;        /*!< HTTP timeout */
    const char         *user_agent;        /*!< HTTP User-Agent (NULL = default) */
    const char         *cert_pem;          /*!< Optional CA cert (PEM) for HTTPS */
    esp_err_t         (*crt_bundle_attach)(void *conf); /*!< esp_crt_bundle_attach for HTTPS */
    icy_http_event_cb_t event_handle;      /*!< User event callback (optional) */
    void               *user_data;         /*!< User pointer passed in event callback */
    bool                stack_in_ext;      /*!< Run element task with stack in PSRAM */
} icy_http_stream_cfg_t;

#define ICY_HTTP_STREAM_CFG_DEFAULT() {     \
    .task_stack        = 6 * 1024,          \
    .task_prio         = 4,                 \
    .task_core         = 0,                 \
    .out_rb_size       = 32 * 1024,         \
    .buffer_len        = 2048,              \
    .timeout_ms        = 30 * 1000,         \
    .user_agent        = "Mozilla/5.0",     \
    .cert_pem          = NULL,              \
    .crt_bundle_attach = NULL,              \
    .event_handle      = NULL,              \
    .user_data         = NULL,              \
    .stack_in_ext      = false,             \
}

/**
 * @brief Create an audio_element that reads an HTTP/HTTPS audio stream and
 *        parses optional ICY (Shoutcast) metadata.
 *
 * Set the source URL with audio_element_set_uri() before running the pipeline.
 */
audio_element_handle_t icy_http_stream_init(const icy_http_stream_cfg_t *cfg);

/**
 * @brief Register a callback to receive raw ICY metadata strings as they
 *        arrive (e.g. "StreamTitle='Artist - Track';").
 *
 * The callback runs on the element's read task, keep it short.
 */
void icy_http_stream_set_metadata_cb(audio_element_handle_t el,
                                     icy_http_metadata_cb_t cb);

#ifdef __cplusplus
}
#endif
