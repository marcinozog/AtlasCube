#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "defines.h"        // HAS_ESPNOW_REMOTE — the whole file is behind it
#include "espnow_link.h"

#if defined(HAS_ESPNOW_REMOTE)

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "trace.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "nvs.h"
#include "cJSON.h"

#include "media_control.h"
#include "app_state.h"
#include "playlist.h"
#include "sd_player.h"
#include "sdcard.h"         // sdcard_is_mounted() for the card flag in t:"sd"
#include "audio_engine.h"
#include "settings.h"
#include "wifi_manager.h"
#include "diag.h"           // the snapshot behind t:"diag", shared with /api/diag

static const char *TAG = "ESPNOW";

#define NVS_NS          "espnow"
#define NVS_KEY_PEER    "peer"

#define RX_QUEUE_DEPTH  8
// Remote→radio frames are short plain commands. The longest is set_eq_10 with ten
// two-digit negative gains, ~49 B — so this is the ceiling to raise first if the
// settings the remote writes ever stop fitting on one line.
#define RX_PAYLOAD_MAX  128

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
// Not NAME_MAX: that one is POSIX's, out of limits.h, and redefining it here
// warns wherever the toolchain got there first.
#define ENTRY_NAME_MAX  32      // playlist / SD name truncation in t:"list", t:"sd"
#define TEXT_MAX        48      // stn/ttl truncation in t:"state"
#define LIST_MAX        32

static QueueHandle_t s_rx_q;
static uint8_t       s_peer[ESPNOW_MAC_LEN];
static bool          s_paired;
static int64_t       s_pair_until_us;      // 0 = window closed
static int           s_last_seq = -1;      // -1 = nothing accepted yet
static bool          s_ready;

// Link activity, read out through espnow_link_get_status(). A few dozen bytes of
// .bss and one esp_timer_get_time() per accepted frame — a radio with no remote
// pays nothing for these because no frame is ever accepted.
static int64_t  s_last_rx_us;              // 0 = nothing ever accepted
static int8_t   s_last_rssi;
static uint32_t s_rx_frames;
static uint32_t s_tx_ok;
static uint32_t s_tx_fail;
static char     s_last_cmd[ESPNOW_LAST_CMD_MAX];

typedef struct {
    uint8_t src[ESPNOW_MAC_LEN];
    uint8_t seq;
    int8_t  rssi;                      // as the WiFi driver saw this frame, dBm
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
 * Polish station names are multi-byte; a split tail would reach the remote as
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
        // The folder stands in for the album. ttl already carries the track's
        // ID3 title, and station_name belongs to the radio — it keeps whatever
        // stream played last and goes stale the moment the source changes,
        // which on the remote reads as "still playing that station".
        //
        // Basename only: sd_dir is a full path, and every character of the
        // "/sdcard/music/" in front would come out of the 48 this field is
        // truncated to.
        const char *slash = strrchr(s->sd_dir, '/');
        stn     = (slash && slash[1]) ? slash + 1 : s->sd_dir;
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
            char nm[ENTRY_NAME_MAX];
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

// ─────────────────────────────────────────────────────────────────────────────
// SD browsing
//
// The remote navigates by index and never sees a path, exactly as it plays
// stations by index and never sees a URL. What makes that work is that
// sd_player already keeps a cursor — the last folder it scanned — so "entry 3"
// has a meaning here without the remote having to say where from.
//
// Folders and tracks share one numbering, folders first. An index the remote read
// off a row therefore means the same thing in get_sd, sd_open and sd_play_index,
// and neither end has to agree about anything beyond `nf`.

// Mirrors sd_player's own SD_DIR_MAX: a path it cannot store is a path there is
// no point building.
#define SD_PATH_MAX     192

static bool sd_at_root(void)
{
    return strcmp(sd_player_dir(), sd_player_root()) == 0;
}

// The current folder's own name, not its path — paths do not cross this link,
// and a remote's screen has room for one word of it anyway. Empty at the root.
static const char *sd_dir_name(void)
{
    const char *dir = sd_player_dir();
    if (!dir[0] || sd_at_root()) {
        return "";
    }

    const char *slash = strrchr(dir, '/');
    return slash ? slash + 1 : dir;
}

// Reads the listing sd_player already holds rather than rescanning per page: a
// page is one request, and rescanning the card for each would put a directory
// walk between the remote's finger and every row it scrolls past. The scan
// happens when the folder changes — and here, once, for a card nobody has
// browsed yet on this boot.
static char *build_sd(int off, int cnt)
{
    if (!sd_player_dir()[0]) {
        sd_player_scan(NULL);   // also mounts the card, lazily
    }

    // Asked of the card rather than inferred from the listing. It gets its own
    // field because "no card" and "empty folder" both list nothing and want
    // different words on the remote's screen — and because a card pulled after a
    // scan stops answering here while the cursor it left behind does not.
    const bool card    = sdcard_is_mounted();
    const int  folders = card ? sd_player_folder_count() : 0;
    const int  tracks  = card ? sd_player_count() : 0;
    const int  total   = folders + tracks;

    if (off < 0)     off = 0;
    if (off > total) off = total;
    if (cnt <= 0 || cnt > LIST_MAX) cnt = LIST_MAX;
    if (off + cnt > total)          cnt = total - off;

    // Same halving guard as build_list: file names are longer and less
    // predictable than station names, so the frame cap is likelier to bite here.
    for (;;) {
        cJSON *r = cJSON_CreateObject();
        if (!r) return NULL;

        cJSON_AddStringToObject(r, "t",     "sd");
        cJSON_AddNumberToObject(r, "card",  card ? 1 : 0);
        cJSON_AddStringToObject(r, "dir",   sd_dir_name());
        cJSON_AddNumberToObject(r, "up",    (card && !sd_at_root()) ? 1 : 0);
        cJSON_AddNumberToObject(r, "off",   off);
        cJSON_AddNumberToObject(r, "total", total);
        cJSON_AddNumberToObject(r, "nf",    folders);
        cJSON *arr = cJSON_AddArrayToObject(r, "e");

        for (int i = 0; arr && i < cnt; i++) {
            const int   idx  = off + i;
            const char *name = idx < folders ? sd_player_folder(idx)
                                             : sd_player_track(idx - folders);
            if (!name) break;

            char nm[ENTRY_NAME_MAX];
            copy_trunc(nm, sizeof(nm), name);
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

// The path is built before the scan that invalidates the name it was built from:
// sd_player_folder() points into the listing buffers, and scanning rewrites them.
static void sd_open(int n)
{
    if (n < 0 || n >= sd_player_folder_count()) {
        ESP_LOGW(TAG, "sd_open=%d is not a folder", n);
        return;
    }

    char path[SD_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", sd_player_dir(), sd_player_folder(n));
    sd_player_scan(path);
}

static void sd_up(void)
{
    if (sd_at_root()) {
        return;   // the browse root is the top; there is nothing above it
    }

    char path[SD_PATH_MAX];
    snprintf(path, sizeof(path), "%s", sd_player_dir());

    char *slash = strrchr(path, '/');
    if (!slash) {
        return;
    }
    *slash = 0;

    // The cursor is shared with the web UI, which is free to point it anywhere
    // on the card — so climbing from wherever it was left can land outside the
    // browse root. Going home is the only sensible reading of "up" from there.
    if (strncmp(path, sd_player_root(), strlen(sd_player_root())) != 0) {
        sd_player_scan(NULL);
        return;
    }

    sd_player_scan(path);
}

static void sd_play_index(int n)
{
    const int folders = sd_player_folder_count();
    if (n < folders || n >= folders + sd_player_count()) {
        ESP_LOGW(TAG, "sd_play_index=%d is not a track", n);
        return;
    }

    char path[SD_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s",
             sd_player_dir(), sd_player_track(n - folders));
    sd_player_play_path(path);
}

// ─────────────────────────────────────────────────────────────────────────────

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

/*
 * `rssi` is the strength of the ping frame being answered, as the radio's WiFi
 * driver saw it — not the radio's own signal. A remote cannot measure how well its
 * transmissions arrive, so this is the only number it can turn into a signal-bar
 * indicator. It describes remote→radio; the reverse direction is not symmetric,
 * but at these power levels it is the useful approximation.
 */
static char *build_pong(int8_t rssi)
{
    cJSON *r = cJSON_CreateObject();
    if (!r) return NULL;

    cJSON_AddStringToObject(r, "t",    "pong");
    cJSON_AddNumberToObject(r, "ch",   current_channel());
    cJSON_AddStringToObject(r, "ver",  esp_app_get_description()->version);
    cJSON_AddNumberToObject(r, "rssi", rssi);

    char *out = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return out;
}

/*
 * The settings the remote can edit, as a fixed reply rather than a queryable path.
 *
 * A path-addressed read would need the whole settings tree as cJSON to walk, and
 * that tree only exists inside the /api/settings GET handler, built by hand. Not
 * extracting it is a deliberate choice while the remote edits three keys — see the
 * note above the setters in handle_frame() for the line that changes it.
 */
static char *build_cfg(void)
{
    const app_settings_t *s = settings_get();

    cJSON *r = cJSON_CreateObject();
    if (!r) return NULL;

    cJSON_AddStringToObject(r, "t",    "cfg");
    cJSON_AddNumberToObject(r, "br",   s->display.brightness);
    cJSON_AddBoolToObject  (r, "eqen", s->audio.eq_enabled);
    cJSON_AddItemToObject  (r, "eq",   cJSON_CreateIntArray(s->audio.eq, 10));
    cJSON_AddNumberToObject(r, "ch",   current_channel());

    char *out = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return out;
}

/*
 * The subset of diag_collect() a remote has a screen for: firmware identity, the
 * update flag, and the handful of numbers worth watching from across the room.
 *
 * Not the whole snapshot. /api/diag also carries the IDF version, the panel
 * geometry, the flash size, the www staleness and the MQTT group — data for a bug
 * report filed from a browser, none of which reads usefully on a 240 px panel.
 * The full thing is also ~800 B of mostly variable-length strings, which is a bad
 * bet against a 1400 B cap that truncates rather than fragments.
 */
static char *build_diag(void)
{
    diag_info_t d;
    diag_collect(&d);

    // Own baseline, exactly like the /api/diag handler keeps its own: per-core
    // load is a delta since this state's previous call, so a remote sharing the
    // browser's reference point would consume it and both would read near zero.
    static diag_cpu_state_t s_cpu;
    int cpu0 = 0, cpu1 = 0;
    diag_cpu_usage(&s_cpu, &cpu0, &cpu1);

    cJSON *r = cJSON_CreateObject();
    if (!r) return NULL;

    cJSON_AddStringToObject(r, "t",    "diag");
    cJSON_AddStringToObject(r, "ver",  d.fw_version);
    cJSON_AddBoolToObject  (r, "upd",  d.update_available);
    cJSON_AddStringToObject(r, "new",  d.update_latest);
    cJSON_AddNumberToObject(r, "up",   d.uptime_s);

    // Tenths of a degree, the unit diag_info_t already uses to stay float-free.
    // Omitted rather than sent as a sentinel when the sensor never installed: the
    // remote renders a missing key as "—", and no negative value is safely out of
    // range for a die temperature.
    if (d.temp_valid) {
        cJSON_AddNumberToObject(r, "t10", d.temp_c10);
    }

    cJSON_AddNumberToObject(r, "heap", (double)d.int_free);
    cJSON_AddNumberToObject(r, "hmin", (double)d.int_min_free);
    cJSON_AddStringToObject(r, "ssid", d.ssid);
    cJSON_AddNumberToObject(r, "rssi", d.rssi);
    cJSON_AddNumberToObject(r, "sd",   d.sd_mounted ? 1 : 0);

    // Megabytes: the byte counts are uint64 and would spend a hundred characters
    // of the frame on digits the remote rounds away before it draws them.
    cJSON_AddNumberToObject(r, "sdfree", (double)(d.sd_free / (1024 * 1024)));
    cJSON_AddNumberToObject(r, "cpu0", cpu0);
    cJSON_AddNumberToObject(r, "cpu1", cpu1);
    cJSON_AddNumberToObject(r, "ch",   current_channel());

    char *out = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Frame handling (worker task context)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Records that a frame from the remote was accepted. Called for everything that
 * gets past the paired-MAC check — including duplicates the dedup then drops,
 * because "the remote is talking to us" is true either way and that is what
 * last_seen answers.
 */
static void mark_seen(const rx_frame_t *f)
{
    s_last_rx_us = esp_timer_get_time();
    s_last_rssi  = f->rssi;
    s_rx_frames++;
    copy_trunc(s_last_cmd, sizeof(s_last_cmd), f->payload);
}

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
        // Replacing a remote: drop the old peer so repeated re-pairings cannot
        // fill the 20-slot peer list with dead MACs.
        if (s_paired && memcmp(s_peer, f->src, ESPNOW_MAC_LEN) != 0)
            esp_now_del_peer(s_peer);

        memcpy(s_peer, f->src, ESPNOW_MAC_LEN);
        s_paired    = true;
        s_last_seq  = -1;              // new remote, accept whatever it sends next
        // Activity belongs to a remote, not to the radio: carrying the old one's
        // frame counts past a re-pair would read as history that never happened.
        // mark_seen() below then counts this pair frame as the first.
        s_rx_frames = 0;
        s_tx_ok     = 0;
        s_tx_fail   = 0;
        peer_save();
        peer_ensure(s_peer);
        ESP_LOGI(TAG, "paired with " MACSTR, MAC2STR(s_peer));
        mark_seen(f);
        reply(f->src, f->seq, build_pair_ack());
        return;
    }

    if (!s_paired || memcmp(f->src, s_peer, ESPNOW_MAC_LEN) != 0) {
        ESP_LOGW(TAG, "frame from unpaired " MACSTR " dropped", MAC2STR(f->src));
        return;
    }

    mark_seen(f);

    // ── queries: idempotent, so they are answered even when the sequence
    //    repeats. Deduplicating these would strand a remote whose reply was
    //    lost — it would retry and get nothing back.
    if (strcmp(cmd, "ping") == 0) {
        reply(f->src, f->seq, build_pong(f->rssi));
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
    if (strncmp(cmd, "get_sd=", 7) == 0) {
        const char *args  = cmd + 7;
        const char *comma = strchr(args, ',');
        int off = atoi(args);
        int cnt = comma ? atoi(comma + 1) : LIST_MAX;
        reply(f->src, f->seq, build_sd(off, cnt));
        return;
    }
    if (strcmp(cmd, "get_cfg") == 0) {
        reply(f->src, f->seq, build_cfg());
        return;
    }
    if (strcmp(cmd, "get_diag") == 0) {
        reply(f->src, f->seq, build_diag());
        return;
    }

    // ── mutating commands: a repeated sequence means the remote never saw our
    //    MAC ACK and retried something we already did. next/prev/volp are not
    //    idempotent, so the repeat is dropped.
    if ((int)f->seq == s_last_seq) {
        TRACE(TRACE_ESPNOW, TAG, "duplicate seq %u dropped: %s", f->seq, cmd);
        return;
    }
    s_last_seq = f->seq;

    // Card navigation is deliberately on this side of the dedup, and deliberately
    // silent. Moving the cursor is not idempotent — a retried sd_open would
    // descend twice — so it has to be a command the dedup can drop, which means
    // it cannot carry the new listing back. The remote reads that with get_sd,
    // one exchange later.
    if (strncmp(cmd, "sd_open=", 8) == 0) {
        sd_open(atoi(cmd + 8));
        return;
    }
    if (strcmp(cmd, "sd_up") == 0) {
        sd_up();
        return;
    }
    if (strncmp(cmd, "sd_play_index=", 14) == 0) {
        sd_play_index(atoi(cmd + 14));
        return;
    }

    /*
     * Settings. These call the setters directly instead of going through the JSON
     * patch that POST /api/settings applies, and that is a decision with a limit
     * written into it.
     *
     * That handler is 400 lines, and its sections trail side effects: flip,
     * invert and bgr post UI_EVT_BG_CHANGED, a background change calls
     * net_wallpaper_dismiss(), the NTP section reconfigures the time service and
     * re-applies the dim schedule. Sharing it would mean lifting all of that into
     * a component both transports can reach — a refactor of code that every radio
     * runs, for a device most of them do not have.
     *
     * The three keys below are the ones with no such tail: bare setters, nothing
     * to keep in step, nothing to drift. The moment the remote wants one that does
     * have a tail — display.flip, display.theme, anything under wallpaper or ntp
     * — this block is the wrong place for it and that handler's body has to come
     * out into a shared settings_apply_patch() first. Do not grow it past that.
     *
     * Absolute setters, so a duplicate would be harmless; they sit below the dedup
     * anyway, because that is where anything that writes belongs.
     */
    if (strncmp(cmd, "set_brightness=", 15) == 0) {
        int pct = atoi(cmd + 15);
        // Ignored rather than clamped, the same choice vol=N makes in
        // ws_protocol.md: an explicit setpoint the device silently rewrites is
        // worse than one it refuses.
        if (pct >= 0 && pct <= 100) {
            settings_set_brightness(pct);
        } else {
            ESP_LOGW(TAG, "set_brightness=%d out of range — ignored", pct);
        }
        return;
    }
    if (strncmp(cmd, "set_eq_enabled=", 15) == 0) {
        settings_set_eq_enabled(atoi(cmd + 15) != 0);
        return;
    }
    if (strncmp(cmd, "set_eq_10=", 10) == 0) {
        // Named after the WS command that does the same thing, because it is the
        // same thing in a different encoding. Ten bands or none: settings_set_eq_10()
        // takes the whole array, so a short list would silently zero the rest.
        int         bands[10];
        int         n = 0;
        const char *p = cmd + 10;

        while (n < 10 && *p) {
            bands[n++] = atoi(p);
            const char *comma = strchr(p, ',');
            if (!comma) break;
            p = comma + 1;
        }

        if (n == 10) {
            settings_set_eq_10(bands);
        } else {
            ESP_LOGW(TAG, "set_eq_10: %d bands, need 10 — ignored", n);
        }
        return;
    }

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

/*
 * Signature note: this used to take the destination MAC. Since IDF 5.x it takes
 * esp_now_send_info_t (= wifi_tx_info_t), and esp_now.h says the separate
 * `status` argument will eventually go in favour of tx_info->tx_status. Only the
 * pass/fail matters here, so `status` stays until it actually disappears.
 *
 * There is one peer, so the address is not needed either — hence tx_info unused.
 */
static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;

    // Unicast ESP-NOW is ACKed at the MAC layer, so this is the only place the
    // radio learns that a reply did not land. Counters only — the remote owns the
    // retry.
    if (status == ESP_NOW_SEND_SUCCESS) s_tx_ok++;
    else                                s_tx_fail++;
}

static void rx_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    // Copy and queue, nothing else. Calling into media_control or SPIFFS from
    // here would run them on the WiFi task's stack.
    if (!info || !data) return;
    if (len < 2 || len > 1 + (int)(RX_PAYLOAD_MAX - 1)) return;

    // Cheap gate before the queue: this callback sees every ESP-NOW frame in the
    // air, a neighbour's radio and its broadcasts included. Without it each one
    // costs a queue slot and a worker wake-up to be rejected in handle_frame(),
    // which still holds the authoritative checks — this only spares the trip.
    // Anything from an unknown MAC gets through solely while the pairing window
    // is open, because that is where broadcast `pair` arrives.
    bool from_peer = s_paired && memcmp(info->src_addr, s_peer, ESPNOW_MAC_LEN) == 0;
    if (!from_peer && !window_open()) return;

    rx_frame_t f;
    memcpy(f.src, info->src_addr, ESPNOW_MAC_LEN);
    f.seq  = data[0];
    f.rssi = info->rx_ctrl ? (int8_t)info->rx_ctrl->rssi : 0;
    memcpy(f.payload, data + 1, len - 1);
    f.payload[len - 1] = 0;

    // Full queue → drop. The remote retries on a missing ACK anyway, and
    // blocking here would stall the WiFi task.
    xQueueSend(s_rx_q, &f, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Everything the link costs: the RX queue, esp_now_init(), the callbacks and the
 * worker task — roughly 10 KB of internal DRAM once the 8 KB stack is counted,
 * plus a callback that then sees every ESP-NOW frame in the air.
 *
 * Split out of espnow_link_init() so a radio with no remote never pays it. Two
 * things call it: boot, when NVS already holds a peer, and the pairing window.
 */
static esp_err_t link_start(void)
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
    esp_now_register_send_cb(send_cb);

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
        ESP_LOGI(TAG, "link up on ch %d for pairing", current_channel());

    return ESP_OK;
}

esp_err_t espnow_link_init(void)
{
    if (s_ready) return ESP_OK;

    // The NVS read is the whole cost on a radio that has no remote: no queue, no
    // task, no esp_now_init(), so not even an RX callback to run on the WiFi task.
    // Pairing is always someone pressing a button, and that path starts the link.
    peer_load();
    if (!s_paired) {
        ESP_LOGI(TAG, "no remote paired — link stays down until the pairing "
                      "window opens");
        return ESP_OK;
    }

    return link_start();
}

void espnow_link_rebind(void)
{
    if (!s_ready || !s_paired) return;

    // The peer carries the interface it was added on. A late AP→STA transition
    // (the recovery supervisor reaching the router minutes after boot) leaves it
    // pinned to WIFI_IF_AP, which no longer exists — the remote then goes silent
    // with no error anywhere. Re-add it against the current interface.
    esp_now_del_peer(s_peer);
    peer_ensure(s_peer);
    ESP_LOGI(TAG, "peer rebound to %s on ch %d",
             link_if() == WIFI_IF_AP ? "AP" : "STA", current_channel());
}

void espnow_link_pair_window_open(uint32_t seconds)
{
    // The link may still be down — espnow_link_init() leaves it that way when no
    // remote is stored. This is the deliberate act that brings it up. Failing to
    // start is not fatal here: the window opens anyway and simply catches nothing,
    // which the caller reports as "no remote answered" like any other timeout.
    if (link_start() != ESP_OK)
        ESP_LOGE(TAG, "pairing window opened but the link failed to start");

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

void espnow_link_get_status(espnow_link_status_t *out)
{
    if (!out) return;

    memset(out, 0, sizeof(*out));
    out->supported = true;
    out->started   = s_ready;
    out->paired    = s_paired;
    if (s_paired) memcpy(out->mac, s_peer, ESPNOW_MAC_LEN);
    out->window_s  = espnow_link_pair_window_left();
    out->rssi      = s_last_rssi;
    out->rx_frames = s_rx_frames;
    out->tx_ok     = s_tx_ok;
    out->tx_fail   = s_tx_fail;
    memcpy(out->last_cmd, s_last_cmd, sizeof(out->last_cmd));

    out->last_seen_s = s_last_rx_us
        ? (uint32_t)((esp_timer_get_time() - s_last_rx_us) / 1000000)
        : ESPNOW_NEVER;
}

#else  // !HAS_ESPNOW_REMOTE

// Built without the remote link. Stubs rather than #ifdefs at every call site:
// app_main, network_services and the web handlers stay unchanged, and the web UI
// learns the truth from status.supported.

esp_err_t espnow_link_init(void)                     { return ESP_ERR_NOT_SUPPORTED; }
void      espnow_link_rebind(void)                   { }
void      espnow_link_pair_window_open(uint32_t s)   { (void)s; }
uint32_t  espnow_link_pair_window_left(void)         { return 0; }
bool      espnow_link_is_paired(void)                { return false; }

bool espnow_link_get_peer(uint8_t out[ESPNOW_MAC_LEN])
{
    (void)out;
    return false;
}

void espnow_link_get_status(espnow_link_status_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->last_seen_s = ESPNOW_NEVER;   // supported stays false
}

#endif // HAS_ESPNOW_REMOTE
