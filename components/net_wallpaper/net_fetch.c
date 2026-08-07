#include "net_fetch.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdarg.h>
#include <stdio.h>

static const char *TAG = "NET_FETCH";

#define HTTP_TIMEOUT_MS  15000

static const char *s_status = "idle";   // "idle"/"busy"/"ok" or points at s_err
static char        s_err[96];

void net_fetch_set_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_err, sizeof(s_err), fmt, ap);
    va_end(ap);
    s_status = s_err;
    ESP_LOGW(TAG, "%s", s_err);
}

void net_fetch_set_status(const char *literal)
{
    s_status = literal;
}

const char *net_fetch_status(void)
{
    return s_status;
}

uint8_t *net_fetch_download(const char *url, size_t cap, int *out_len)
{
    esp_http_client_config_t cfg = {
        .url               = url,
        .timeout_ms        = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_agent        = "AtlasCube/1.0",
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (!cli) { net_fetch_set_err("http client init failed"); return NULL; }

    int status = 0;
    for (int hop = 0; hop < 4; hop++) {
        esp_err_t err = esp_http_client_open(cli, 0);
        if (err != ESP_OK) {
            net_fetch_set_err("connect failed (%s)", esp_err_to_name(err));
            esp_http_client_cleanup(cli);
            return NULL;
        }
        esp_http_client_fetch_headers(cli);
        status = esp_http_client_get_status_code(cli);
        if (status / 100 != 3) break;
        esp_http_client_set_redirection(cli);   // Location header → client URL
        esp_http_client_close(cli);
    }
    if (status / 100 != 2) {
        net_fetch_set_err("HTTP %d", status);
        esp_http_client_close(cli);
        esp_http_client_cleanup(cli);
        return NULL;
    }

    uint8_t *buf = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
    if (!buf) {
        net_fetch_set_err("no PSRAM for %u B download", (unsigned)cap);
        esp_http_client_close(cli);
        esp_http_client_cleanup(cli);
        return NULL;
    }

    int total = 0;
    while ((size_t)total < cap) {
        int r = esp_http_client_read(cli, (char *)buf + total, cap - total);
        if (r < 0) { net_fetch_set_err("read failed at %d B", total); goto fail; }
        if (r == 0) break;
        total += r;
    }
    if ((size_t)total >= cap) {
        net_fetch_set_err("file larger than %u KB cap", (unsigned)(cap / 1024));
        goto fail;
    }

    esp_http_client_close(cli);
    esp_http_client_cleanup(cli);
    *out_len = total;
    return buf;

fail:
    heap_caps_free(buf);
    esp_http_client_close(cli);
    esp_http_client_cleanup(cli);
    return NULL;
}
