#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "nvs.h"
#include "cJSON.h"

#include "espnow_link.h"
#include "media_control.h"
#include "app_state.h"
#include "playlist.h"
#include "sd_player.h"
#include "audio_engine.h"
#include "settings.h"
#include "wifi_manager.h"

static const char *TAG = "ESPNOW";

#define NVS_NS          "espnow"
#define NVS_KEY_PEER    "peer"

#define RX_QUEUE_DEPTH  8
#define RX_PAYLOAD_MAX  128     // pilot→radio frames are short plain commands

// media_command_execute_text() reaches settings_set_volume() → save_to_file(),
// the same depth the 8192-byte httpd task already survives on the WS path.
// Deliberately internal RAM, not MALLOC_CAP_SPIRAM: that save writes SPIFFS,
// and a task running off a PSRAM stack must not be scheduled while the flash
// cache is disabled.
#define TASK_STACK      8192

// docs/espnow_link.md: replies are capped and truncated, never fragmented.
// ESP_NOW_MAX_DATA_LEN_V2 is 1470 in IDF 5.5.4; 1400 leaves room for the
// sequence byte and stays clear of the limit.
#define REPLY_MAX       1400
#define NAME_MAX        32      // playlist name truncation in t:"list"
#define TEXT_MAX        48      // stn/ttl truncation in t:"state"
#define LIST_MAX        32

static QueueHandle_t s_rx_q;
static uint8_t       s_peer[ESPNOW_MAC_LEN];
static bool          s_paired;
static int64_t       s_pair_until_us;      // 0 = window closed
static int           s_last_seq = -1;      // -1 = nothing accepted yet
static bool          s_ready;

typedef struct {
    uint8_t src[ESPNOW_MAC_LEN];
    uint8_t seq;
    char    payload[RX_PAYLOAD_MAX];   // NUL-terminated
} rx_frame_t;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool window_open(void)
{
    return s_pair_until_us != 0 && esp_timer_get_time() < s_pair_until_us;
}

static int current_channel(void)
{
    uint8_t pri = 0;
    wifi_second_chan_t sec;
    if (esp_wifi_get_channel(&pri, &sec) != ESP_OK) return 0;
    return pri;
}

// ESP-NOW must be bound to a started interface. In AP fallback the STA
// interface does not exist, so the peer's ifidx has to follow the run mode.
static wifi_interface_t link_if(void)
{
    return (wifi_get_run_mode() == WIFI_RUN_MODE_AP) ? WIFI_IF_AP : WIFI_IF_STA;
}

static esp_err_t peer_ensure(const uint8_t mac[ESPNOW_MAC_LEN])
{
    if (esp_now_is_peer_exist(mac)) return ESP_OK;

    esp_now_peer_info_t p = { 0 };
    memcpy(p.peer_addr, mac, ESPNOW_MAC_LEN);
    // channel 0 = "whatever the interface is on". A hardcoded number here works
    // until the router next moves channel, then fails silently.
    p.channel = 0;
    p.ifidx   = link_if();
    p.encrypt = false;

    esp_err_t err = esp_now_add_peer(&p);
    if (err != ESP_OK) ESP_LOGW(TAG, "add_peer failed: %s", esp_err_to_name(err));
    return err;
}

/**
 * Truncates to at most dst_sz-1 bytes without splitting a UTF-8 sequence.
 * Polish station names are multi-byte; a split tail would reach the pilot as
 * invalid UTF-8 and break its JSON parse.
 */
static void copy_trunc(char *dst, size_t dst_sz, const char *src)
{
    if (!src) { dst[0] = 0; return; }
    size_t n = strlen(src);
    if (n > dst_sz - 1) {
        n = dst_sz - 1;
        while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;  // back off continuation bytes
    }
    memcpy(dst, src, n);
    dst[n] = 0;
}

/** Sends one reply frame. Takes ownership of `json` (a cJSON_Print* string). */
static void reply(const uint8_t mac[ESPNOW_MAC_LEN], uint8_t seq, char *json)
{
    if (!json) {
        ESP_LOGW(TAG, "reply build failed (out of heap)");
        return;
    }

    size_t n = strlen(json);
    if (n > REPLY_MAX) {
        ESP_LOGW(TAG, "reply %u B over the %d B cap — dropped", (unsigned)n, REPLY_MAX);
        free(json);
        return;
    }

    uint8_t *frame = malloc(n + 1);
    if (!frame) { free(json); return; }

    frame[0] = seq;                      // echo the request's sequence
    memcpy(frame + 1, json, n);
    free(json);

    if (peer_ensure(mac) == ESP_OK) {
        esp_err_t err = esp_now_send(mac, frame, n + 1);
        if (err != ESP_OK) ESP_LOGW(TAG, "send failed: %s", esp_err_to_name(err));
    }
    free(frame);
}

// ─────────────────────────────────────────────────────────────────────────────
// Stored peer (NVS)
// ─────────────────────────────────────────────────────────────────────────────

static void peer_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;

    size_t sz = ESPNOW_MAC_LEN;
    if (nvs_get_blob(h, NVS_KEY_PEER, s_peer, &sz) == ESP_OK && sz == ESPNOW_MAC_LEN)
        s_paired = true;

    nvs_close(h);
}

static void peer_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed — pairing will not survive a reboot");
        return;
    }
    if (nvs_set_blob(h, NVS_KEY_PEER, s_peer, ESPNOW_MAC_LEN) == ESP_OK)
        nvs_commit(h);
    nvs_close(h);
}

// ─────────────────────────────────────────────────────────────────────────────
// Reply builders
// ─────────────────────────────────────────────────────────────────────────────

static char *build_state(void)
{
    app_state_t   *s   = app_state_get();
    media_source_t src = media_source_current();

    const char *src_str = "radio";
    const char *stn     = s->station_name;
    const char *ttl     = s->title;
    bool        playing = (s->radio_state == RADIO_STATE_PLAYING ||
                           s->radio_state == RADIO_STATE_BUFFERING);

    if (src == MEDIA_SOURCE_SD) {
        src_str = "sd";
        playing = sd_player_is_active() && !audio_engine_is_paused();
    } else if (src == MEDIA_SOURCE_BT) {
        src_str = "bt";
        stn     = s->bt_artist;
        ttl     = s->bt_title;
        playing = s->bt_playing;
    }

    char stn_buf[TEXT_MAX], ttl_buf[TEXT_MAX];
    copy_trunc(stn_buf, sizeof(stn_buf), stn);
    copy_trunc(ttl_buf, sizeof(ttl_buf), ttl);

    cJSON *r = cJSON_CreateObject();
    if (!r) return NULL;

    cJSON_AddStringToObject(r, "t",   "state");
    cJSON_AddStringToObject(r, "src", src_str);
    cJSON_AddNumberToObject(r, "st",  playing ? 1 : 0);
    cJSON_AddNumberToObject(r, "vol", s->volume);
    cJSON_AddNumberToObject(r, "idx", src == MEDIA_SOURCE_RADIO ? s->curr_index : -1);
    cJSON_AddStringToObject(r, "stn", stn_buf);
    cJSON_AddStringToObject(r, "ttl", ttl_buf);
    cJSON_AddNumberToObject(r, "ch",  current_channel());

    char *out = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return out;
}

static char *build_list(int off, int cnt)
{
    int total = playlist_get_count();

    if (off < 0)     off = 0;
    if (off > total) off = total;
    if (cnt <= 0 || cnt > LIST_MAX) cnt = LIST_MAX;
    if (off + cnt > total)          cnt = total - off;

    // Names are pre-truncated, so the first attempt fits in practice. The loop
    // only guards the pathological case where JSON escaping expands a name past
    // the frame cap; halving always terminates at cnt == 0.
    for (;;) {
        cJSON *r = cJSON_CreateObject();
        if (!r) return NULL;

        cJSON_AddStringToObject(r, "t",     "list");
        cJSON_AddNumberToObject(r, "off",   off);
        cJSON_AddNumberToObject(r, "total", total);
        cJSON *arr = cJSON_AddArrayToObject(r, "e");

        for (int i = 0; arr && i < cnt; i++) {
            const playlist_entry_t *e = playlist_get(off + i);
            if (!e) break;
            char nm[NAME_MAX];
            copy_trunc(nm, sizeof(nm), e->name);
            cJSON_AddItemToArray(arr, cJSON_CreateString(nm));
        }

        char *out = cJSON_PrintUnformatted(r);
        cJSON_Delete(r);

        if (!out) return NULL;
        if (strlen(out) <= REPLY_MAX || cnt == 0) return out;

        free(out);
        cnt /= 2;
    }
}

static char *build_pair_ack(void)
{
    uint8_t mac[ESPNOW_MAC_LEN] = { 0 };
    esp_wifi_get_mac(link_if(), mac);

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(mac));

    const char *name = settings_get()->device.hostname;

    cJSON *r = cJSON_CreateObject();
    if (!r) return NULL;

    cJSON_AddStringToObject(r, "t",    "pair");
    cJSON_AddNumberToObject(r, "ch",   current_channel());
    cJSON_AddStringToObject(r, "name", (name && name[0]) ? name : "AtlasCube");
    cJSON_AddStringToObject(r, "mac",  mac_str);

    char *out = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return out;
}

static char *build_pong(void)
{
    cJSON *r = cJSON_CreateObject();
    if (!r) return NULL;

    cJSON_AddStringToObject(r, "t",   "pong");
    cJSON_AddNumberToObject(r, "ch",  current_channel());
    cJSON_AddStringToObject(r, "ver", esp_app_get_description()->version);

    char *out = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Frame handling (worker task context)
// ─────────────────────────────────────────────────────────────────────────────

static void handle_frame(const rx_frame_t *f)
{
    const char *cmd = f->payload;

    // ── pair: the only frame accepted from an unknown MAC ────────────────────
    if (strcmp(cmd, "pair") == 0) {
        if (!window_open()) {
            ESP_LOGW(TAG, "pair from " MACSTR " ignored — window closed",
                     MAC2STR(f->src));
            return;
        }
        // Replacing a pilot: drop the old peer so repeated re-pairings cannot
        // fill the 20-slot peer list with dead MACs.
        if (s_paired && memcmp(s_peer, f->src, ESPNOW_MAC_LEN) != 0)
            esp_now_del_peer(s_peer);

        memcpy(s_peer, f->src, ESPNOW_MAC_LEN);
        s_paired    = true;
        s_last_seq  = -1;              // new pilot, accept whatever it sends next
        peer_save();
        peer_ensure(s_peer);
        ESP_LOGI(TAG, "paired with " MACSTR, MAC2STR(s_peer));
        reply(f->src, f->seq, build_pair_ack());
        return;
    }

    if (!s_paired || memcmp(f->src, s_peer, ESPNOW_MAC_LEN) != 0) {
        ESP_LOGW(TAG, "frame from unpaired " MACSTR " dropped", MAC2STR(f->src));
        return;
    }

    // ── queries: idempotent, so they are answered even when the sequence
    //    repeats. Deduplicating these would strand a pilot whose reply was
    //    lost — it would retry and get nothing back.
    if (strcmp(cmd, "ping") == 0) {
        reply(f->src, f->seq, build_pong());
        return;
    }
    if (strcmp(cmd, "get_state") == 0) {
        reply(f->src, f->seq, build_state());
        return;
    }
    if (strncmp(cmd, "get_list=", 9) == 0) {
        const char *args  = cmd + 9;
        const char *comma = strchr(args, ',');
        int off = atoi(args);
        int cnt = comma ? atoi(comma + 1) : LIST_MAX;
        reply(f->src, f->seq, build_list(off, cnt));
        return;
    }

    // ── mutating commands: a repeated sequence means the pilot never saw our
    //    MAC ACK and retried something we already did. next/prev/volp are not
    //    idempotent, so the repeat is dropped.
    if ((int)f->seq == s_last_seq) {
        ESP_LOGD(TAG, "duplicate seq %u dropped: %s", f->seq, cmd);
        return;
    }
    s_last_seq = f->seq;

    media_command_execute_text(cmd);
}

static void rx_task(void *arg)
{
    rx_frame_t f;
    for (;;) {
        if (xQueueReceive(s_rx_q, &f, portMAX_DELAY) == pdTRUE)
            handle_frame(&f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RX callback — runs in the WiFi task
// ─────────────────────────────────────────────────────────────────────────────

static void rx_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    // Copy and queue, nothing else. Calling into media_control or SPIFFS from
    // here would run them on the WiFi task's stack.
    if (!info || !data) return;
    if (len < 2 || len > 1 + (int)(RX_PAYLOAD_MAX - 1)) return;

    rx_frame_t f;
    memcpy(f.src, info->src_addr, ESPNOW_MAC_LEN);
    f.seq = data[0];
    memcpy(f.payload, data + 1, len - 1);
    f.payload[len - 1] = 0;

    // Full queue → drop. The pilot retries on a missing ACK anyway, and
    // blocking here would stall the WiFi task.
    xQueueSend(s_rx_q, &f, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

esp_err_t espnow_link_init(void)
{
    if (s_ready) return ESP_OK;

    s_rx_q = xQueueCreate(RX_QUEUE_DEPTH, sizeof(rx_frame_t));
    if (!s_rx_q) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
        vQueueDelete(s_rx_q);
        s_rx_q = NULL;
        return err;
    }
    esp_now_register_recv_cb(rx_cb);

    peer_load();
    if (s_paired) peer_ensure(s_peer);

    if (xTaskCreate(rx_task, "espnow_rx", TASK_STACK, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        esp_now_deinit();
        vQueueDelete(s_rx_q);
        s_rx_q = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_ready = true;

    if (s_paired)
        ESP_LOGI(TAG, "link up on ch %d, paired with " MACSTR,
                 current_channel(), MAC2STR(s_peer));
    else
        ESP_LOGI(TAG, "link up on ch %d, no pilot paired — open the pairing "
                      "window to attach one", current_channel());

    return ESP_OK;
}

void espnow_link_pair_window_open(uint32_t seconds)
{
    s_pair_until_us = esp_timer_get_time() + (int64_t)seconds * 1000000;
    ESP_LOGI(TAG, "pairing window open for %u s on ch %d",
             (unsigned)seconds, current_channel());
}

uint32_t espnow_link_pair_window_left(void)
{
    if (!window_open()) return 0;
    return (uint32_t)((s_pair_until_us - esp_timer_get_time()) / 1000000) + 1;
}

bool espnow_link_is_paired(void)
{
    return s_paired;
}

bool espnow_link_get_peer(uint8_t out[ESPNOW_MAC_LEN])
{
    if (!s_paired) return false;
    memcpy(out, s_peer, ESPNOW_MAC_LEN);
    return true;
}
