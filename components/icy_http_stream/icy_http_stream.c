/*
 * icy_http_stream — minimal HTTP audio reader for esp-adf with ICY metadata.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>

#include "esp_log.h"
#include "esp_http_client.h"

#include "audio_element.h"
#include "audio_common.h"
#include "audio_mem.h"

#include "icy_http_stream.h"

static const char *TAG = "ICY_HTTP";

#define ICY_META_BUF_SIZE 512
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    esp_http_client_handle_t  client;
    bool                      is_open;
    int                       _errno;

    const char               *cert_pem;
    esp_err_t               (*crt_bundle_attach)(void *conf);
    const char               *user_agent;
    int                       timeout_ms;
    int                       buffer_len;

    icy_http_event_cb_t       event_cb;
    void                     *user_data;

    /* ICY (Shoutcast) parsing state */
    int                       icy_metaint;
    int                       bytes_until_meta;
    int                       meta_expected;
    int                       meta_received;
    char                      meta_buf[ICY_META_BUF_SIZE];
    icy_http_metadata_cb_t    meta_cb;
} icy_http_t;

static esp_codec_type_t _content_type_to_codec(const char *ct)
{
    if (strcasecmp(ct, "mp3") == 0 ||
        strcasecmp(ct, "audio/mp3") == 0 ||
        strcasecmp(ct, "audio/mpeg") == 0 ||
        strcasecmp(ct, "binary/octet-stream") == 0 ||
        strcasecmp(ct, "application/octet-stream") == 0) {
        return ESP_CODEC_TYPE_MP3;
    }
    if (strcasecmp(ct, "audio/aac") == 0 ||
        strcasecmp(ct, "audio/x-aac") == 0 ||
        strcasecmp(ct, "audio/aacp") == 0 ||
        strcasecmp(ct, "audio/mp4") == 0) {
        return ESP_CODEC_TYPE_AAC;
    }
    if (strcasecmp(ct, "audio/wav") == 0) {
        return ESP_CODEC_TYPE_WAV;
    }
    if (strcasecmp(ct, "audio/opus") == 0) {
        return ESP_CODEC_TYPE_OPUS;
    }
    return ESP_CODEC_TYPE_UNKNOW;
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_HEADER) {
        return ESP_OK;
    }

    audio_element_handle_t el = (audio_element_handle_t)evt->user_data;
    icy_http_t *http = (icy_http_t *)audio_element_getdata(el);

    if (strcasecmp(evt->header_key, "icy-metaint") == 0) {
        http->icy_metaint = atoi(evt->header_value);
        http->bytes_until_meta = http->icy_metaint;
    } else if (strcasecmp(evt->header_key, "Content-Type") == 0) {
        audio_element_set_codec_fmt(el, _content_type_to_codec(evt->header_value));
    }
    return ESP_OK;
}

static int _dispatch(audio_element_handle_t self, icy_http_event_id_t id)
{
    icy_http_t *http = (icy_http_t *)audio_element_getdata(self);
    if (!http->event_cb) {
        return ESP_OK;
    }
    icy_http_event_msg_t msg = {
        .event_id    = id,
        .client      = http->client,
        .el          = self,
        .user_data   = http->user_data,
        .icy_metaint = http->icy_metaint,
    };
    return http->event_cb(&msg);
}

static esp_err_t _icy_open(audio_element_handle_t self)
{
    icy_http_t *http = (icy_http_t *)audio_element_getdata(self);
    if (http->is_open) {
        return ESP_OK;
    }

    char *uri = audio_element_get_uri(self);
    if (!uri) {
        ESP_LOGE(TAG, "URI not set");
        return ESP_FAIL;
    }

    /* Reset ICY state on each open */
    http->icy_metaint      = 0;
    http->bytes_until_meta = 0;
    http->meta_expected    = 0;
    http->meta_received    = 0;
    http->_errno           = 0;

    esp_http_client_config_t cfg = {
        .url               = uri,
        .event_handler     = _http_event_handler,
        .user_data         = self,
        .timeout_ms        = http->timeout_ms,
        .buffer_size       = http->buffer_len,
        .buffer_size_tx    = 1024,
        .cert_pem          = http->cert_pem,
        .crt_bundle_attach = http->crt_bundle_attach,
        .user_agent        = http->user_agent,
    };

    http->client = esp_http_client_init(&cfg);
    if (!http->client) {
        ESP_LOGE(TAG, "esp_http_client_init failed");
        return ESP_FAIL;
    }

    esp_http_client_set_header(http->client, "Icy-MetaData", "1");
    esp_http_client_set_header(http->client, "Accept", "*/*");
    esp_http_client_set_header(http->client, "Accept-Encoding", "identity;q=1,*;q=0");
    esp_http_client_set_header(http->client, "Connection", "keep-alive");

    if (_dispatch(self, ICY_HTTP_EVENT_PRE_REQUEST) != ESP_OK) {
        ESP_LOGE(TAG, "PRE_REQUEST callback failed");
        goto _open_fail;
    }

_redirect:
    if (esp_http_client_open(http->client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        goto _open_fail;
    }

    int64_t content_len = esp_http_client_fetch_headers(http->client);
    int status = esp_http_client_get_status_code(http->client);

    if (status == 301 || status == 302 || status == 307 || status == 308) {
        esp_http_client_set_redirection(http->client);
        goto _redirect;
    }
    if (status != 200 && status != 206) {
        ESP_LOGE(TAG, "HTTP status %d", status);
        goto _open_fail;
    }

    if (content_len > 0) {
        audio_element_set_total_bytes(self, content_len);
    }

    if (_dispatch(self, ICY_HTTP_EVENT_POST_REQUEST) != ESP_OK) {
        ESP_LOGE(TAG, "POST_REQUEST callback failed");
        goto _open_fail;
    }

    audio_element_report_codec_fmt(self);
    http->is_open = true;
    return ESP_OK;

_open_fail:
    if (http->client) {
        esp_http_client_close(http->client);
        esp_http_client_cleanup(http->client);
        http->client = NULL;
    }
    return ESP_FAIL;
}

static esp_err_t _icy_close(audio_element_handle_t self)
{
    icy_http_t *http = (icy_http_t *)audio_element_getdata(self);
    if (http->is_open) {
        http->is_open = false;
    }
    if (audio_element_get_state(self) != AEL_STATE_PAUSED) {
        audio_element_report_pos(self);
        audio_element_set_byte_pos(self, 0);
    }
    if (http->client) {
        esp_http_client_close(http->client);
        esp_http_client_cleanup(http->client);
        http->client = NULL;
    }
    return ESP_OK;
}

static esp_err_t _icy_destroy(audio_element_handle_t self)
{
    icy_http_t *http = (icy_http_t *)audio_element_getdata(self);
    audio_free(http);
    return ESP_OK;
}

/*
 * Read callback. Parses ICY metadata inline if icy_metaint > 0.
 * The Shoutcast frame layout is: <metaint audio bytes> <len*16 byte> <metadata>.
 * `len` is a single byte; metadata length = len * 16 (max 4080).
 */
static int _icy_read(audio_element_handle_t self, char *buffer, int len,
                     TickType_t ticks_to_wait, void *context)
{
    icy_http_t *http = (icy_http_t *)audio_element_getdata(self);
    int rlen = esp_http_client_read(http->client, buffer, len);

    /* Fast path: no ICY metadata in stream */
    if (http->icy_metaint <= 0) {
        goto _handle_rlen;
    }

    if (rlen > 0) {
        int out = 0;
        int i = 0;

        /* Fast path: whole chunk is audio and we don't cross meta boundary */
        if (http->meta_expected == 0 && http->bytes_until_meta > rlen) {
            http->bytes_until_meta -= rlen;
            audio_element_update_byte_pos(self, rlen);
            return rlen;
        }

        while (i < rlen) {
            if (http->bytes_until_meta < 0) {
                http->bytes_until_meta = 0;
            }

            /* AUDIO bytes before next meta */
            if (http->meta_expected == 0 && http->bytes_until_meta > 0) {
                int consume = MIN(http->bytes_until_meta, rlen - i);
                if (out != i) {
                    memmove(buffer + out, buffer + i, consume);
                }
                i   += consume;
                out += consume;
                http->bytes_until_meta -= consume;
                continue;
            }

            /* META LENGTH byte */
            if (http->meta_expected == 0 && http->bytes_until_meta == 0) {
                if (i >= rlen) break;
                uint8_t meta_len_byte = (uint8_t)buffer[i++];
                uint16_t meta_len = (uint16_t)meta_len_byte * 16;

                if (meta_len == 0) {
                    http->bytes_until_meta = http->icy_metaint;
                    continue;
                }
                if (meta_len > sizeof(http->meta_buf) - 1) {
                    ESP_LOGW(TAG, "ICY meta too large: %u, truncating", meta_len);
                    meta_len = sizeof(http->meta_buf) - 1;
                }
                http->meta_expected = meta_len;
                http->meta_received = 0;
                /* fallthrough */
            }

            /* META DATA bytes */
            {
                int remaining = rlen - i;
                int needed    = http->meta_expected - http->meta_received;
                int to_copy   = MIN(needed, remaining);

                if (to_copy > 0) {
                    memcpy(http->meta_buf + http->meta_received, buffer + i, to_copy);
                    http->meta_received += to_copy;
                    i += to_copy;
                }
                if (http->meta_received < http->meta_expected) {
                    /* Need more bytes from next read */
                    break;
                }
                http->meta_buf[http->meta_expected] = '\0';
                ESP_LOGD(TAG, "ICY meta: %s", http->meta_buf);
                if (http->meta_cb) {
                    http->meta_cb(http->meta_buf);
                }
                http->meta_expected = 0;
                http->meta_received = 0;
                http->bytes_until_meta = http->icy_metaint;
            }
        }

        if (out == 0) {
            /* Whole chunk consumed by metadata — not EOF, ask pipeline to retry */
            return 0;
        }
        audio_element_update_byte_pos(self, out);
        return out;
    }

_handle_rlen:
    if (rlen <= 0) {
        http->_errno = esp_http_client_get_errno(http->client);
        ESP_LOGW(TAG, "No data, errno=%d, rlen=%d", http->_errno, rlen);
        if (http->_errno != 0) {
            return http->_errno;
        }
        if (_dispatch(self, ICY_HTTP_EVENT_FINISH_TRACK) != ESP_OK) {
            return ESP_FAIL;
        }
        return ESP_OK;
    }
    audio_element_update_byte_pos(self, rlen);
    return rlen;
}

static audio_element_err_t _icy_process(audio_element_handle_t self, char *in_buf, int in_len)
{
    int r = audio_element_input(self, in_buf, in_len);
    if (audio_element_is_stopping(self)) {
        return AEL_IO_ABORT;
    }
    if (r > 0) {
        return audio_element_output(self, in_buf, r);
    }
    /* r == 0: chunk consumed by ICY metadata — ask pipeline to call us again */
    if (r == 0) {
        return ESP_OK;
    }
    return r;
}

audio_element_handle_t icy_http_stream_init(const icy_http_stream_cfg_t *cfg)
{
    if (!cfg) {
        ESP_LOGE(TAG, "cfg is NULL");
        return NULL;
    }

    icy_http_t *http = audio_calloc(1, sizeof(icy_http_t));
    if (!http) {
        ESP_LOGE(TAG, "no mem for icy_http_t");
        return NULL;
    }

    http->cert_pem          = cfg->cert_pem;
    http->crt_bundle_attach = cfg->crt_bundle_attach;
    http->user_agent        = cfg->user_agent ? cfg->user_agent : "Mozilla/5.0";
    http->timeout_ms        = cfg->timeout_ms > 0 ? cfg->timeout_ms : 30 * 1000;
    http->buffer_len        = cfg->buffer_len > 0 ? cfg->buffer_len : 2048;
    http->event_cb          = cfg->event_handle;
    http->user_data         = cfg->user_data;

    audio_element_cfg_t el_cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    el_cfg.open         = _icy_open;
    el_cfg.close        = _icy_close;
    el_cfg.destroy      = _icy_destroy;
    el_cfg.read         = _icy_read;
    el_cfg.process      = _icy_process;
    el_cfg.task_stack   = cfg->task_stack > 0 ? cfg->task_stack : 6 * 1024;
    el_cfg.task_prio    = cfg->task_prio > 0 ? cfg->task_prio : 4;
    el_cfg.task_core    = cfg->task_core;
    el_cfg.out_rb_size  = cfg->out_rb_size > 0 ? cfg->out_rb_size : 32 * 1024;
    el_cfg.buffer_len   = http->buffer_len;
    el_cfg.stack_in_ext = cfg->stack_in_ext;
    el_cfg.tag          = "icy_http";

    audio_element_handle_t el = audio_element_init(&el_cfg);
    if (!el) {
        ESP_LOGE(TAG, "audio_element_init failed");
        audio_free(http);
        return NULL;
    }
    audio_element_setdata(el, http);

    audio_element_info_t info = AUDIO_ELEMENT_INFO_DEFAULT();
    audio_element_setinfo(el, &info);
    return el;
}

void icy_http_stream_set_metadata_cb(audio_element_handle_t el,
                                     icy_http_metadata_cb_t cb)
{
    if (!el) return;
    icy_http_t *http = (icy_http_t *)audio_element_getdata(el);
    if (http) {
        http->meta_cb = cb;
    }
}
