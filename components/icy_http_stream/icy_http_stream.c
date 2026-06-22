/*
 * icy_http_stream — minimal HTTP audio reader for esp-adf with ICY metadata.
 *
 * http://  streams are read over a raw socket (esp_http_client's read path
 * does not interoperate with some ancient SHOUTcast DNAS servers, which accept
 * the connection but leave fetch_headers spinning). https:// streams go through
 * esp_http_client for TLS. Both paths share the same ICY metadata parsing.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "audio_element.h"
#include "audio_common.h"
#include "audio_mem.h"

#include "icy_http_stream.h"
#include "ts_demux.h"
#include "hls_playlist.h"

static const char *TAG = "ICY_HTTP";

#define ICY_META_BUF_SIZE 512
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define TS_PKT_SIZE       188
#define HLS_PLAYLIST_MAX  (256 * 1024)   /* hard cap on a fetched .m3u8 body */

typedef struct {
    esp_http_client_handle_t  client;
    int                       raw_fd;   /* >=0 → raw-socket mode (http://), else esp_http_client */
    bool                      is_open;
    int                       _errno;

    const char               *cert_pem;
    esp_err_t               (*crt_bundle_attach)(void *conf);
    const char               *user_agent;
    int                       timeout_ms;
    int                       buffer_len;

    icy_http_event_cb_t       event_cb;
    void                     *user_data;

    /* Codec inferred from the Content-Type response header (both transports).
     * ESP_CODEC_TYPE_UNKNOW until headers are parsed; surfaced to the engine in
     * the POST_REQUEST event so it can relink the decoder if the URL-hint guess
     * was wrong. */
    esp_codec_type_t          detected_codec;

    /* ICY (Shoutcast) parsing state */
    int                       icy_metaint;
    int                       bytes_until_meta;
    int                       meta_expected;
    int                       meta_received;
    char                      meta_buf[ICY_META_BUF_SIZE];
    icy_http_metadata_cb_t    meta_cb;

    /* HLS mode state (active when the URL is an .m3u8). In this mode the element
     * fetches a media playlist, streams each .ts segment over http->client and
     * demuxes MPEG-TS to ADTS so the downstream AAC decoder sees a normal stream.
     * ICY parsing is inactive (icy_metaint stays 0). */
    bool                      is_hls;
    ts_demux_t               *demux;
    uint8_t                  *ts_scratch;     /* recv buffer before demux (buffer_len) */
    char                     *hls_media_url;  /* resolved media playlist URL (live refresh) */
    char                    **hls_q;          /* pending segment URLs (owned) */
    int                       hls_q_count;
    int                       hls_q_pos;
    int                       hls_q_cap;
    char                    **hls_tmp;        /* scratch list filled by parse callback */
    int                       hls_tmp_count;
    int                       hls_tmp_cap;
    uint64_t                  hls_next_seq;   /* next media-sequence number to enqueue */
    uint32_t                  hls_target_dur; /* #EXT-X-TARGETDURATION, refresh pacing */
    bool                      hls_is_live;    /* no #EXT-X-ENDLIST → keep refreshing */
    bool                      hls_seg_open;   /* a segment connection is currently open */
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
        http->detected_codec = _content_type_to_codec(evt->header_value);
        audio_element_set_codec_fmt(el, http->detected_codec);
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
        .codec       = http->detected_codec,
    };
    return http->event_cb(&msg);
}

/* TCP connect with a bounded timeout (blocking connect can hang ~75s on a
 * dead host). Returns a connected socket fd or -1. */
static int _raw_connect(const char *host, int port, int timeout_ms)
{
    char portstr[8];
    snprintf(portstr, sizeof(portstr), "%d", port);

    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) {
        ESP_LOGE(TAG, "getaddrinfo failed for %s", host);
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int cret = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (cret != 0 && errno == EINPROGRESS) {
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(fd, &wset);
        struct timeval tv = { .tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000 };
        if (select(fd + 1, NULL, &wset, NULL, &tv) <= 0) {
            ESP_LOGE(TAG, "connect timeout to %s:%d", host, port);
            close(fd);
            return -1;
        }
        int soerr = 0; socklen_t slen = sizeof(soerr);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &slen);
        if (soerr != 0) {
            ESP_LOGE(TAG, "connect failed to %s:%d (err=%d)", host, port, soerr);
            close(fd);
            return -1;
        }
    } else if (cret != 0) {
        close(fd);
        return -1;
    }

    fcntl(fd, F_SETFL, flags);   /* back to blocking; rely on SO_RCVTIMEO */
    struct timeval rtv = { .tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv));
    return fd;
}

/* Apply one response header line (key already lowercased-compared) to state. */
static void _raw_apply_header(audio_element_handle_t self, icy_http_t *http,
                              const char *key, const char *val)
{
    if (strcasecmp(key, "icy-metaint") == 0) {
        http->icy_metaint = atoi(val);
        http->bytes_until_meta = http->icy_metaint;
    } else if (strcasecmp(key, "content-type") == 0) {
        http->detected_codec = _content_type_to_codec(val);
        audio_element_set_codec_fmt(self, http->detected_codec);
    }
}

/*
 * Raw-socket HTTP open for http:// streams. esp_http_client's read path does
 * not interoperate with some ancient SHOUTcast DNAS servers (verified: a raw
 * GET gets ICY 200 OK while esp_http_client's fetch_headers spins forever),
 * so we talk HTTP ourselves — like yoradio. Handles "ICY 200 OK" and
 * "HTTP/1.x 200/206" status lines, content-type and icy-metaint, plus simple
 * http→http redirects. https:// still goes through esp_http_client (TLS).
 */
/* Buffer sizes for the heap scratch (kept off the 6 KB element task stack). */
#define RAW_HDR_SZ 1536
#define RAW_URL_SZ 512
#define RAW_REQ_SZ 640

static esp_err_t _raw_open(audio_element_handle_t self, icy_http_t *http, const char *uri)
{
    /* Big buffers on the heap — the http element task only has ~6 KB stack and
     * these (+ getaddrinfo/select frames) overflow it otherwise. */
    char *scratch = audio_malloc(RAW_HDR_SZ + RAW_URL_SZ + RAW_REQ_SZ + RAW_URL_SZ);
    if (!scratch) {
        ESP_LOGE(TAG, "raw: no mem for scratch");
        return ESP_FAIL;
    }
    char *hdr      = scratch;                          /* RAW_HDR_SZ */
    char *cur      = hdr + RAW_HDR_SZ;                 /* RAW_URL_SZ */
    char *req      = cur + RAW_URL_SZ;                 /* RAW_REQ_SZ */
    char *location = req + RAW_REQ_SZ;                 /* RAW_URL_SZ */

    esp_err_t ret = ESP_FAIL;
    int fd = -1;

    strncpy(cur, uri, RAW_URL_SZ - 1);
    cur[RAW_URL_SZ - 1] = '\0';

    for (int redirect = 0; redirect < 5; redirect++) {
        if (strncmp(cur, "http://", 7) != 0) {
            ESP_LOGE(TAG, "raw: unsupported scheme: %s", cur);
            goto cleanup;
        }

        char host[160] = {0};
        char path[300] = "/";
        int  port = 80;

        const char *p = cur + 7;
        const char *slash = strchr(p, '/');
        const char *colon = strchr(p, ':');
        if (colon && (!slash || colon < slash)) {
            int hlen = colon - p;
            if (hlen >= (int)sizeof(host)) hlen = sizeof(host) - 1;
            memcpy(host, p, hlen);
            port = atoi(colon + 1);
        } else {
            int hlen = slash ? (slash - p) : (int)strlen(p);
            if (hlen >= (int)sizeof(host)) hlen = sizeof(host) - 1;
            memcpy(host, p, hlen);
        }
        if (slash) {
            strncpy(path, slash, sizeof(path) - 1);
        }

        ESP_LOGD(TAG, "raw open: host=%s port=%d path=%s", host, port, path);

        fd = _raw_connect(host, port, http->timeout_ms);
        if (fd < 0) goto cleanup;

        int reqlen = snprintf(req, RAW_REQ_SZ,
            "GET %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "User-Agent: %s\r\n"
            "Icy-MetaData: 1\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n\r\n",
            path, host, port, http->user_agent);
        if (send(fd, req, reqlen, 0) != reqlen) {
            ESP_LOGE(TAG, "raw: send failed");
            goto cleanup;
        }

        /* Read the header block one byte at a time so no body bytes are
         * consumed (header is ~400 B, a one-time cost at stream start). */
        int  hlen = 0;
        bool done = false;
        while (hlen < RAW_HDR_SZ - 1) {
            char c;
            int r = recv(fd, &c, 1, 0);
            if (r <= 0) {
                ESP_LOGE(TAG, "raw: header recv failed (r=%d errno=%d)", r, errno);
                goto cleanup;
            }
            hdr[hlen++] = c;
            if (hlen >= 4 && hdr[hlen-4]=='\r' && hdr[hlen-3]=='\n'
                          && hdr[hlen-2]=='\r' && hdr[hlen-1]=='\n') {
                done = true;
                break;
            }
        }
        hdr[hlen] = '\0';
        if (!done) {
            ESP_LOGE(TAG, "raw: header too large / not terminated");
            goto cleanup;
        }

        /* Status line: "ICY 200 OK" or "HTTP/1.x 200 OK" — status after 1st space. */
        const char *sp = strchr(hdr, ' ');
        int status = sp ? atoi(sp + 1) : 0;
        ESP_LOGD(TAG, "raw: status=%d", status);

        /* Parse headers; capture Location for redirects. */
        location[0] = '\0';
        char *save = NULL;
        char *line = strtok_r(hdr, "\n", &save);   /* first line = status */
        line = strtok_r(NULL, "\n", &save);
        while (line) {
            char *colon2 = strchr(line, ':');
            if (colon2) {
                *colon2 = '\0';
                char *key = line;
                char *val = colon2 + 1;
                while (*val == ' ' || *val == '\t') val++;
                /* strip trailing \r */
                int vl = strlen(val);
                while (vl > 0 && (val[vl-1] == '\r' || val[vl-1] == '\n')) val[--vl] = '\0';
                if (strcasecmp(key, "location") == 0) {
                    strncpy(location, val, RAW_URL_SZ - 1);
                    location[RAW_URL_SZ - 1] = '\0';
                } else {
                    _raw_apply_header(self, http, key, val);
                }
            }
            line = strtok_r(NULL, "\n", &save);
        }

        if (status == 301 || status == 302 || status == 307 || status == 308) {
            close(fd);
            fd = -1;
            if (!location[0]) { ESP_LOGE(TAG, "raw: redirect without Location"); goto cleanup; }
            ESP_LOGI(TAG, "raw: redirect -> %s", location);
            strncpy(cur, location, RAW_URL_SZ - 1);
            cur[RAW_URL_SZ - 1] = '\0';
            continue;   /* re-loop; if https, the scheme check fails → caller handles */
        }
        if (status != 200 && status != 206) {
            ESP_LOGE(TAG, "raw: HTTP status %d", status);
            goto cleanup;
        }

        http->raw_fd = fd;
        fd = -1;            /* owned by http now — don't close in cleanup */
        ret = ESP_OK;
        goto cleanup;
    }

    ESP_LOGE(TAG, "raw: too many redirects");

cleanup:
    if (fd >= 0) close(fd);
    audio_free(scratch);
    return ret;
}

/* ----------------------------------------------------------------------------
 * HLS (.m3u8) support
 *
 * Single-variant / single audio rendition live + VOD HLS. The master playlist
 * (if any) is resolved one level down to its preferred audio media playlist;
 * the media playlist's .ts segments are streamed in order and demuxed to ADTS.
 * For live streams the media playlist is re-fetched every TARGETDURATION and
 * new segments (by media sequence number) are appended. AES, byte-range and
 * I-frame playlists are not handled.
 * ------------------------------------------------------------------------- */

/* True if the URL path (ignoring any query string) ends in ".m3u8". */
static bool _uri_is_hls(const char *uri)
{
    const char *q = strchr(uri, '?');
    int len = q ? (int)(q - uri) : (int)strlen(uri);
    const char *ext = ".m3u8";
    int el = 5;
    return len >= el && strncasecmp(uri + len - el, ext, el) == 0;
}

/* hls_playlist media-URI callback: collect segment URLs (already absolute) into
 * the temp list. hls_playlist frees its copy after we return, so we strdup. */
static int _hls_seg_cb(char *uri, void *ctx)
{
    icy_http_t *http = (icy_http_t *)ctx;
    if (http->hls_tmp_count >= http->hls_tmp_cap) {
        int ncap = http->hls_tmp_cap ? http->hls_tmp_cap * 2 : 16;
        char **n = realloc(http->hls_tmp, ncap * sizeof(char *));
        if (!n) {
            return 0;
        }
        http->hls_tmp = n;
        http->hls_tmp_cap = ncap;
    }
    char *dup = strdup(uri);
    if (dup) {
        http->hls_tmp[http->hls_tmp_count++] = dup;
    }
    return 0;
}

/* Append a segment URL to the play queue, taking ownership. */
static void _hls_enqueue(icy_http_t *http, char *url)
{
    if (http->hls_q_count >= http->hls_q_cap) {
        int ncap = http->hls_q_cap ? http->hls_q_cap * 2 : 16;
        char **n = realloc(http->hls_q, ncap * sizeof(char *));
        if (!n) {
            free(url);
            return;
        }
        http->hls_q = n;
        http->hls_q_cap = ncap;
    }
    http->hls_q[http->hls_q_count++] = url;
}

/* Pop the next pending segment URL (caller takes ownership), or NULL if empty. */
static char *_hls_dequeue(icy_http_t *http)
{
    if (http->hls_q_pos >= http->hls_q_count) {
        return NULL;
    }
    char *u = http->hls_q[http->hls_q_pos];
    http->hls_q[http->hls_q_pos++] = NULL;
    if (http->hls_q_pos >= http->hls_q_count) {
        http->hls_q_pos = http->hls_q_count = 0;   /* drained → reset */
    }
    return u;
}

/* Merge the freshly-parsed temp list into the play queue, keeping only segments
 * with a media sequence number not seen before (dedup across live refreshes). */
static void _hls_merge(icy_http_t *http, uint64_t base)
{
    for (int i = 0; i < http->hls_tmp_count; i++) {
        uint64_t seq = base + i;
        if (seq >= http->hls_next_seq) {
            _hls_enqueue(http, http->hls_tmp[i]);   /* transfer ownership */
            http->hls_next_seq = seq + 1;
        } else {
            free(http->hls_tmp[i]);
        }
        http->hls_tmp[i] = NULL;
    }
    http->hls_tmp_count = 0;
}

/* One persistent esp_http_client reused (keep-alive) for every HLS request —
 * playlist fetches and .ts segment reads alike. Opening a fresh TLS connection
 * per segment exhausts LwIP sockets and trips CDN connect timeouts; reusing the
 * connection (same host) avoids the per-segment handshake. Handles redirects and
 * checks for a 200/206 status. */
static esp_err_t _hls_http_open(icy_http_t *http, const char *url)
{
    if (!http->client) {
        esp_http_client_config_t cfg = {
            .url               = url,
            .timeout_ms        = http->timeout_ms,
            .cert_pem          = http->cert_pem,
            .crt_bundle_attach = http->crt_bundle_attach,
            .user_agent        = http->user_agent,
            .buffer_size       = http->buffer_len,
            .keep_alive_enable = true,
        };
        http->client = esp_http_client_init(&cfg);
        if (!http->client) {
            return ESP_FAIL;
        }
    } else {
        esp_http_client_set_url(http->client, url);
    }

    int status = 0, tries = 0;
    for (;;) {
        if (esp_http_client_open(http->client, 0) != ESP_OK) {
            return ESP_FAIL;
        }
        esp_http_client_fetch_headers(http->client);
        status = esp_http_client_get_status_code(http->client);
        if ((status == 301 || status == 302 || status == 307 || status == 308) && tries++ < 5) {
            esp_http_client_set_redirection(http->client);
            esp_http_client_close(http->client);
            continue;
        }
        break;
    }
    if (status != 200 && status != 206) {
        ESP_LOGE(TAG, "HLS HTTP %d (%s)", status, url);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* Close the current request but keep the client object so the next request can
 * reuse the keep-alive connection. Use after a clean read or to reset on error. */
static void _hls_conn_close(icy_http_t *http)
{
    if (http->client) {
        esp_http_client_close(http->client);
    }
}

/* Fully tear down the persistent client (stream end). */
static void _hls_conn_destroy(icy_http_t *http)
{
    if (http->client) {
        esp_http_client_close(http->client);
        esp_http_client_cleanup(http->client);
        http->client = NULL;
    }
}

/* Fetch an entire (small) text resource into a PSRAM buffer. Caller frees.
 * Reuses the persistent keep-alive client; does not tear it down. */
static esp_err_t _hls_fetch(icy_http_t *http, const char *url, char **out, int *out_len)
{
    if (_hls_http_open(http, url) != ESP_OK) {
        return ESP_FAIL;
    }

    char *buf = NULL;
    int   cap = 4096, len = 0;
    buf = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        buf = malloc(cap);
    }
    if (!buf) {
        return ESP_FAIL;
    }
    for (;;) {
        if (len + 1024 + 1 > cap) {
            if (cap >= HLS_PLAYLIST_MAX) {
                ESP_LOGE(TAG, "playlist too large");
                free(buf);
                return ESP_FAIL;
            }
            int ncap = cap * 2;
            char *nb = heap_caps_realloc(buf, ncap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!nb) {
                nb = realloc(buf, ncap);
            }
            if (!nb) {
                free(buf);
                return ESP_FAIL;
            }
            buf = nb;
            cap = ncap;
        }
        int r = esp_http_client_read(http->client, buf + len, cap - len - 1);
        if (r < 0) {
            _hls_conn_close(http);   /* reset on error so the next open reconnects */
            free(buf);
            return ESP_FAIL;
        }
        if (r == 0) {
            break;
        }
        len += r;
    }
    buf[len] = '\0';
    *out = buf;
    *out_len = len;
    /* End the request; with keep-alive this keeps the socket for the next open. */
    _hls_conn_close(http);
    return ESP_OK;
}

/* Fetch + parse a playlist. If it is a master, follow its preferred audio media
 * playlist (one level). On the media playlist, update sequence/duration/live
 * metadata (when is_initial) and merge new segments into the queue. */
static esp_err_t _hls_load_playlist(icy_http_t *http, const char *start_url, bool is_initial)
{
    char *url = strdup(start_url);
    if (!url) {
        return ESP_FAIL;
    }

    for (int depth = 0; depth < 3; depth++) {
        char *body = NULL;
        int   blen = 0;
        if (_hls_fetch(http, url, &body, &blen) != ESP_OK) {
            free(url);
            return ESP_FAIL;
        }

        hls_playlist_cfg_t cfg = {
            .prefer_bitrate = 0,           /* 0 → first listed variant */
            .cb             = _hls_seg_cb,
            .ctx            = http,
            .uri            = url,
        };
        hls_handle_t h = hls_playlist_open(&cfg);
        if (!h) {
            free(body);
            free(url);
            return ESP_FAIL;
        }
        http->hls_tmp_count = 0;
        hls_playlist_parse_data(h, (uint8_t *)body, blen, true);
        free(body);

        if (hls_playlist_is_master(h)) {
            char *media = hls_playlist_get_prefer_url(h, HLS_STREAM_TYPE_AUDIO);
            hls_playlist_close(h);
            if (!media) {
                ESP_LOGE(TAG, "master playlist has no usable audio variant");
                free(url);
                return ESP_FAIL;
            }
            free(url);
            url = media;   /* join_url-allocated absolute URL; loop to fetch it */
            continue;
        }

        uint64_t base = hls_playlist_get_sequence_no(h);
        uint32_t td   = hls_playlist_get_target_duration(h);
        bool     live = !hls_playlist_is_media_end(h);
        hls_playlist_close(h);

        if (is_initial) {
            http->hls_next_seq = base;     /* start from the first listed segment */
            free(http->hls_media_url);
            http->hls_media_url = strdup(url);
            http->hls_target_dur = td ? td : 6;
        }
        http->hls_is_live = live;

        int before = http->hls_q_count - http->hls_q_pos;
        _hls_merge(http, base);
        int added = (http->hls_q_count - http->hls_q_pos) - before;
        ESP_LOGI(TAG, "HLS %s: seq=%llu target=%us live=%d +%d seg",
                 is_initial ? "media" : "refresh",
                 (unsigned long long)base, (unsigned)http->hls_target_dur,
                 live, added);

        free(url);
        return ESP_OK;
    }

    free(url);
    ESP_LOGE(TAG, "too many nested playlists");
    return ESP_FAIL;
}

/* Sleep up to `seconds`, returning false early if the element is stopping. */
static bool _hls_wait(audio_element_handle_t self, int seconds)
{
    int waited = 0;
    int total = seconds * 1000;
    while (waited < total) {
        if (audio_element_is_stopping(self)) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        waited += 100;
    }
    return true;
}

/* Next segment URL to play (caller owns), or NULL on end-of-stream / stop.
 * For live streams, blocks across playlist refreshes until a new segment
 * appears or the element is stopping. */
static char *_hls_next_segment(audio_element_handle_t self, icy_http_t *http)
{
    char *u = _hls_dequeue(http);
    if (u) {
        return u;
    }
    /* Queue drained. For a live stream, re-fetch the media playlist to pick up
     * newly published segments. Refresh immediately (the buffer is already
     * running low), and only back off — at most target_duration between polls —
     * when the publisher has nothing new yet. */
    int fails = 0;
    while (http->hls_is_live) {
        if (audio_element_is_stopping(self)) {
            return NULL;
        }
        if (_hls_load_playlist(http, http->hls_media_url, false) != ESP_OK) {
            ESP_LOGW(TAG, "live playlist refresh failed (%d)", fails + 1);
            if (++fails >= 5) {
                return NULL;   /* give up → triggers the engine's retry path */
            }
            if (!_hls_wait(self, 2)) {
                return NULL;
            }
            continue;
        }
        fails = 0;
        u = _hls_dequeue(http);
        if (u) {
            return u;
        }
        /* Caught up to the live edge — the publisher emits ~one segment per
         * target_duration, so poll at that cadence (not faster) to avoid
         * churning TLS connections / sockets while nothing new is available.
         * The large out_rb (~tens of seconds) covers the wait. */
        int poll = http->hls_target_dur > 0 ? (int)http->hls_target_dur : 6;
        if (!_hls_wait(self, poll)) {
            return NULL;
        }
    }
    return NULL;   /* VOD: list exhausted */
}

/* Read callback for HLS mode: serve demuxed ADTS bytes. */
static int _hls_read(audio_element_handle_t self, icy_http_t *http, char *buffer, int len)
{
    /* Leave headroom so a packet completed from a previous call's carry-over
     * (< 188 B) cannot push the demux output past `len`. Also never exceed the
     * ts_scratch allocation (buffer_len). */
    int want = len - 256;
    if (want > http->buffer_len) {
        want = http->buffer_len;
    }
    if (want < TS_PKT_SIZE) {
        want = TS_PKT_SIZE;
    }

    for (;;) {
        if (audio_element_is_stopping(self)) {
            return ESP_OK;
        }
        if (!http->hls_seg_open) {
            char *seg = _hls_next_segment(self, http);
            if (!seg) {
                if (_dispatch(self, ICY_HTTP_EVENT_FINISH_TRACK) != ESP_OK) {
                    return ESP_FAIL;
                }
                return ESP_OK;   /* 0 → end of stream */
            }
            esp_err_t r = _hls_http_open(http, seg);
            free(seg);
            if (r != ESP_OK) {
                _hls_conn_close(http);   /* reset; reconnect on next segment */
                continue;                /* skip a bad segment */
            }
            ts_demux_reset_segment(http->demux);
            http->hls_seg_open = true;
        }

        int n = esp_http_client_read(http->client, (char *)http->ts_scratch, want);
        if (n <= 0) {
            /* Segment finished (n==0) or error (n<0). End the request; keep-alive
             * preserves the socket for the next segment's open. */
            _hls_conn_close(http);
            http->hls_seg_open = false;
            continue;
        }
        int outn = ts_demux_process(http->demux, http->ts_scratch, n,
                                    (uint8_t *)buffer, len);
        if (outn > 0) {
            audio_element_update_byte_pos(self, outn);
            return outn;
        }
        /* outn == 0: only PAT/PMT so far — keep reading. */
    }
}

static void _hls_cleanup(icy_http_t *http)
{
    _hls_conn_destroy(http);
    http->hls_seg_open = false;
    for (int i = http->hls_q_pos; i < http->hls_q_count; i++) {
        free(http->hls_q[i]);
    }
    free(http->hls_q);
    http->hls_q = NULL;
    http->hls_q_count = http->hls_q_pos = http->hls_q_cap = 0;
    for (int i = 0; i < http->hls_tmp_count; i++) {
        free(http->hls_tmp[i]);
    }
    free(http->hls_tmp);
    http->hls_tmp = NULL;
    http->hls_tmp_count = http->hls_tmp_cap = 0;
    free(http->hls_media_url);
    http->hls_media_url = NULL;
    http->hls_next_seq = 0;
    http->hls_target_dur = 0;
    http->hls_is_live = false;
}

static esp_err_t _hls_open(audio_element_handle_t self, icy_http_t *http, const char *uri)
{
    if (_dispatch(self, ICY_HTTP_EVENT_PRE_REQUEST) != ESP_OK) {
        ESP_LOGE(TAG, "PRE_REQUEST callback failed");
        return ESP_FAIL;
    }
    if (!http->demux) {
        http->demux = ts_demux_create();
    }
    if (!http->ts_scratch) {
        http->ts_scratch = audio_malloc(http->buffer_len);
    }
    if (!http->demux || !http->ts_scratch) {
        ESP_LOGE(TAG, "hls: out of memory");
        return ESP_FAIL;
    }
    if (_hls_load_playlist(http, uri, true) != ESP_OK) {
        return ESP_FAIL;
    }

    http->detected_codec = ESP_CODEC_TYPE_AAC;
    audio_element_set_codec_fmt(self, http->detected_codec);

    if (_dispatch(self, ICY_HTTP_EVENT_POST_REQUEST) != ESP_OK) {
        ESP_LOGE(TAG, "POST_REQUEST callback failed");
        return ESP_FAIL;
    }
    audio_element_report_codec_fmt(self);
    http->is_open = true;
    return ESP_OK;
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
    http->raw_fd           = -1;
    http->detected_codec   = ESP_CODEC_TYPE_UNKNOW;

    /* HLS (.m3u8) gets its own playlist + TS-demux path. */
    http->is_hls = _uri_is_hls(uri);
    if (http->is_hls) {
        return _hls_open(self, http, uri);
    }

    /* http:// → raw socket (esp_http_client can't talk to old SHOUTcast DNAS);
     * https:// → esp_http_client for TLS. */
    if (strncmp(uri, "http://", 7) == 0) {
        if (_dispatch(self, ICY_HTTP_EVENT_PRE_REQUEST) != ESP_OK) {
            ESP_LOGE(TAG, "PRE_REQUEST callback failed");
            return ESP_FAIL;
        }
        if (_raw_open(self, http, uri) != ESP_OK) {
            return ESP_FAIL;
        }
        if (_dispatch(self, ICY_HTTP_EVENT_POST_REQUEST) != ESP_OK) {
            ESP_LOGE(TAG, "POST_REQUEST callback failed");
            close(http->raw_fd);
            http->raw_fd = -1;
            return ESP_FAIL;
        }
        audio_element_report_codec_fmt(self);
        http->is_open = true;
        return ESP_OK;
    }

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

    // Minimal request headers — same set the raw http:// path sends, kept
    // lean for compatibility with picky streaming servers.
    esp_http_client_set_header(http->client, "Icy-MetaData", "1");
    esp_http_client_set_header(http->client, "Accept", "*/*");
    esp_http_client_set_header(http->client, "Connection", "close");

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

    ESP_LOGD(TAG, "open: status=%d content_len=%lld metaint=%d",
             status, (long long)content_len, http->icy_metaint);

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
    if (http->is_hls) {
        _hls_cleanup(http);   /* also closes http->client */
    }
    if (http->raw_fd >= 0) {
        close(http->raw_fd);
        http->raw_fd = -1;
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
    _hls_cleanup(http);
    if (http->demux) {
        ts_demux_destroy(http->demux);
        http->demux = NULL;
    }
    if (http->ts_scratch) {
        audio_free(http->ts_scratch);
        http->ts_scratch = NULL;
    }
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

    if (http->is_hls) {
        return _hls_read(self, http, buffer, len);
    }

    /* Loop so a read that turns out to be entirely ICY metadata does not
     * surface as a 0-length return — esp-adf treats read()==0 as EOF and
     * would tear the stream down. Raw recv() returns small chunks, so a
     * chunk can land exactly on a metadata boundary with the metadata
     * spilling into the next read; in that case we just read more. */
    for (;;) {
        int rlen;
        if (http->raw_fd >= 0) {
            rlen = recv(http->raw_fd, buffer, len, 0);
        } else {
            rlen = esp_http_client_read(http->client, buffer, len);
        }

        if (rlen <= 0) {
            http->_errno = (http->raw_fd >= 0) ? errno
                                               : esp_http_client_get_errno(http->client);
            ESP_LOGW(TAG, "No data, errno=%d, rlen=%d", http->_errno, rlen);
            if (http->_errno != 0) {
                return http->_errno;
            }
            if (_dispatch(self, ICY_HTTP_EVENT_FINISH_TRACK) != ESP_OK) {
                return ESP_FAIL;
            }
            return ESP_OK;
        }

        /* No ICY metadata in stream → return audio as-is */
        if (http->icy_metaint <= 0) {
            audio_element_update_byte_pos(self, rlen);
            return rlen;
        }

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

        if (out > 0) {
            audio_element_update_byte_pos(self, out);
            return out;
        }
        /* out == 0: chunk was entirely ICY metadata — read more (don't
         * return 0, which esp-adf would interpret as end-of-stream). */
    }
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

    http->raw_fd            = -1;
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
