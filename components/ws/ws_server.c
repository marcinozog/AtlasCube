#include "ws_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "radio_service.h"
#include "media_control.h"
#include "sd_player.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "settings.h"
#include "app_state.h"
#include "bt.h"

static const char *TAG = "WS";

#define MAX_WS_CLIENTS 8

// Largest accepted incoming frame. The biggest legitimate command (play with a
// long podcast URL + title) is well under 1 KB; the length is client-supplied,
// so without a cap a rogue frame could malloc megabytes out of the heap.
#define WS_MAX_FRAME_LEN 4096


static void broadcast_task(void *arg);
static void send_full_state(void);
static void on_state_change(void);
static void ws_broadcast(const char *data, size_t len);

static int ws_clients[MAX_WS_CLIENTS] = {0};
static httpd_handle_t g_server = NULL;
static QueueHandle_t s_broadcast_queue = NULL;


/*
void ws_init(void)
*/
void ws_init(void) {
    s_broadcast_queue = xQueueCreate(4, sizeof(char *));
    xTaskCreate(broadcast_task, "ws_broadcast", 4096, NULL, 5, NULL);
    app_state_subscribe(on_state_change);
}

/*
*/
static void broadcast_task(void *arg) {
    char *msg;
    while (1) {
        if (xQueueReceive(s_broadcast_queue, &msg, portMAX_DELAY) == pdTRUE) {
            size_t len = strlen(msg);
            httpd_ws_frame_t pkt = {
                .type    = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)msg,
                .len     = len,
            };
            for (int i = 0; i < MAX_WS_CLIENTS; i++) {
                int fd = ws_clients[i];
                if (fd == 0) continue;
                if (httpd_ws_send_frame_async(g_server, fd, &pkt) != ESP_OK) {
                    ESP_LOGW(TAG, "Client %d gone, removing", fd);
                    ws_clients[i] = 0;
                }
            }
            free(msg);
        }
    }
}

/*
static void handle_plain_cmd(const char *cmd)
*/
static void handle_plain_cmd(const char *cmd)
{
    ESP_LOGI(TAG, "Plain CMD: %s", cmd);

    // Semantic transport (voice, remote): acts on whatever source is playing
    // right now (radio/SD/BT), resolved by media_source_current().
    if (strcmp(cmd, "stop") == 0) {
        media_control_execute(media_source_current(), MEDIA_ACTION_STOP);
    }
    else if (strcmp(cmd, "play") == 0) {
        media_control_execute(media_source_current(), MEDIA_ACTION_PLAY);
    }
    else if (strcmp(cmd, "toggle") == 0) {
        media_control_execute(media_source_current(), MEDIA_ACTION_PLAY_TOGGLE);
    }
    else if (strcmp(cmd, "next") == 0) {
        media_control_execute(media_source_current(), MEDIA_ACTION_NEXT);
    }
    else if (strcmp(cmd, "prev") == 0) {
        media_control_execute(media_source_current(), MEDIA_ACTION_PREVIOUS);
    }
    else if (strcmp(cmd, "volp") == 0) {
        int v = app_state_get()->volume + 5;
        if (v > 100) v = 100;
        settings_set_volume(v);
    }
    else if (strcmp(cmd, "volm") == 0) {
        int v = app_state_get()->volume - 5;
        if (v < 0) v = 0;
        settings_set_volume(v);
    }
    else if (strncmp(cmd, "vol=", 4) == 0) {
        int v = atoi(cmd + 4);
        if (v >= 0 && v <= 100)
            settings_set_volume(v);
    }
    else if (strncmp(cmd, "playstation=", 12) == 0) {
        int idx = atoi(cmd + 12);
        radio_play_index(idx);
    }
    else if (strncmp(cmd, "source=", 7) == 0) {
        const char *t = cmd + 7;
        if      (strcmp(t, "radio") == 0) media_source_switch(MEDIA_SOURCE_RADIO);
        else if (strcmp(t, "sd")    == 0) media_source_switch(MEDIA_SOURCE_SD);
        else if (strcmp(t, "bt")    == 0) media_source_switch(MEDIA_SOURCE_BT);
        else ESP_LOGW(TAG, "Unknown source: %s", t);
    }
    else {
        ESP_LOGW(TAG, "Unknown plain CMD: %s", cmd);
    }
}

/*
static esp_err_t ws_handler(httpd_req_t *req)
*/
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WS handshake done");

        int fd = httpd_req_to_sockfd(req);

        // check if exists
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (ws_clients[i] == fd) {
                ESP_LOGI("WS", "Client already registered");
                send_full_state();   // option
                return ESP_OK;
            }
        }

        // new client
        bool added = false;

        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (ws_clients[i] == 0) {
                ws_clients[i] = fd;
                added = true;
                break;
            }
        }

        if (!added) {
            ESP_LOGW("WS", "No free slot for client %d", fd);
        }

        // send initial state
        send_full_state();

        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT
    };

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);   // get length
    if (ret != ESP_OK) return ret;

    if (ws_pkt.len > WS_MAX_FRAME_LEN) {
        // Returning an error makes httpd close the socket; ws_on_close frees
        // the client slot.
        ESP_LOGW(TAG, "WS frame too large (%u bytes) — dropping client",
                 (unsigned)ws_pkt.len);
        return ESP_FAIL;
    }

    ws_pkt.payload = malloc(ws_pkt.len + 1);
    if (!ws_pkt.payload) return ESP_ERR_NO_MEM;

    ws_pkt.payload[ws_pkt.len] = 0;

    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);   // read data
        if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WS recv payload failed: %s", esp_err_to_name(ret));
        free(ws_pkt.payload);
        return ret;
    }

    char *payload = (char*)ws_pkt.payload;

    ESP_LOGI(TAG, "RX: %s", payload);

    // --- Plain text commands (e.g. from the Android app) ---
    if (payload[0] != '{') {
        handle_plain_cmd(payload);
        free(ws_pkt.payload);
        return ESP_OK;
    }


    // --- JSON commands ---
    cJSON *json = cJSON_Parse((char*)ws_pkt.payload);
    if (!json) {
        ESP_LOGE(TAG, "Invalid JSON");
        free(ws_pkt.payload);
        return ESP_FAIL;
    }

    cJSON *cmd = cJSON_GetObjectItem(json, "cmd");

    if (cmd && cJSON_IsString(cmd)) {

        if (strcmp(cmd->valuestring, "play") == 0) {
            cJSON *url = cJSON_GetObjectItem(json, "url");
            if (url && cJSON_IsString(url)) {
                // finite=true → podcast episode (EOF is a clean end, no retry);
                // title → shown on screen (podcasts carry no ICY metadata);
                // offset_bytes → resume mid-file via a Range request.
                cJSON *finite = cJSON_GetObjectItem(json, "finite");
                cJSON *title  = cJSON_GetObjectItem(json, "title");
                cJSON *offset = cJSON_GetObjectItem(json, "offset_bytes");
                uint32_t off = (offset && cJSON_IsNumber(offset) && offset->valuedouble > 0)
                             ? (uint32_t)offset->valuedouble : 0;
                radio_play_url(url->valuestring,
                               cJSON_IsTrue(finite),
                               (title && cJSON_IsString(title)) ? title->valuestring : NULL,
                               off);
            }
            cJSON *curr_index = cJSON_GetObjectItem(json, "curr_index");
            if (curr_index && cJSON_IsNumber(curr_index)) {
                settings_set_curr_index(curr_index->valueint);
            }
        }
        else if (strcmp(cmd->valuestring, "stop") == 0) {
            radio_stop();
        }

        // VOLUME
        else if (strcmp(cmd->valuestring, "set_volume") == 0) {
            cJSON *v = cJSON_GetObjectItem(json, "value");
            if (v && cJSON_IsNumber(v)) {
                settings_set_volume(v->valueint);
            }
        }

        // EQUALIZER
        else if (strcmp(cmd->valuestring, "set_eq_10") == 0) {
            cJSON *bands = cJSON_GetObjectItem(json, "bands");

            if (cJSON_IsArray(bands) && cJSON_GetArraySize(bands) == 10) {
                int vals[10];

                for (int i = 0; i < 10; i++) {
                    cJSON *item = cJSON_GetArrayItem(bands, i);
                    vals[i] = cJSON_IsNumber(item) ? item->valueint : 0;
                }

                settings_set_eq_10(vals);
            }
        }

        // BLUETOOTH
        else if (strcmp(cmd->valuestring, "bt_enable") == 0) {
            cJSON *v = cJSON_GetObjectItem(json, "value");
            if (v && cJSON_IsBool(v)) {
                settings_set_bt_enable(cJSON_IsTrue(v));
            }
        }
        else if (strcmp(cmd->valuestring, "bt_volume") == 0) {
            cJSON *v = cJSON_GetObjectItem(json, "value");
            if (v && cJSON_IsNumber(v)) {
                settings_set_bt_volume(v->valueint);
            }
        }
        else if (strcmp(cmd->valuestring, "bt_cmd") == 0) {
            cJSON *v = cJSON_GetObjectItem(json, "value");
            if (v && cJSON_IsString(v)) {
                bt_send_raw(v->valuestring);
            }
        }
        else if (strcmp(cmd->valuestring, "bt_sync_vol") == 0) {
            cJSON *v = cJSON_GetObjectItem(json, "value");
            if (v && cJSON_IsBool(v)) {
                settings_set_bt_vol_sync(cJSON_IsTrue(v));
            }
        }
        // Semantic transport — module-agnostic, resolved via the BT descriptor.
        else if (strcmp(cmd->valuestring, "bt_play") == 0) {
            // Explicit "play BT" = make BT the active source. The phone may
            // already be playing (no rising edge for on_bt_play_event), so switch
            // the source here: it muxes to BT and stops the SD player; bt_play()
            // then ensures the module is playing. Volatile — no save_to_file()
            // on the httpd task (its stack can't take the full-settings JSON build).
            settings_set_bt_enable_volatile(true);
            bt_play();
        }
        else if (strcmp(cmd->valuestring, "bt_pause") == 0) {
            bt_pause();
        }
        else if (strcmp(cmd->valuestring, "bt_next") == 0) {
            bt_next();
        }
        else if (strcmp(cmd->valuestring, "bt_prev") == 0) {
            bt_prev();
        }
        else if (strcmp(cmd->valuestring, "bt_reboot") == 0) {
            bt_reboot();
        }

        // PLAYLIST
        else if (strcmp(cmd->valuestring, "play_index") == 0) {
            cJSON *idx = cJSON_GetObjectItem(json, "index");
            if (idx && cJSON_IsNumber(idx)) {
                // settings_set_curr_index is called inside radio_play_index
                radio_play_index(idx->valueint);
            }
        }

        // Test hook: play a single local audio file from the SD card.
        else if (strcmp(cmd->valuestring, "play_file") == 0) {
            cJSON *path = cJSON_GetObjectItem(json, "path");
            if (path && cJSON_IsString(path)) {
                radio_play_file(path->valuestring);
            }
        }

        // SD-card music player (folder browsing).
        else if (strcmp(cmd->valuestring, "sd_play") == 0) {
            cJSON *dir = cJSON_GetObjectItem(json, "dir");
            sd_player_play_folder((dir && cJSON_IsString(dir)) ? dir->valuestring : NULL);
        }
        else if (strcmp(cmd->valuestring, "sd_play_path") == 0) {
            cJSON *p = cJSON_GetObjectItem(json, "path");
            if (p && cJSON_IsString(p)) sd_player_play_path(p->valuestring);
        }
        else if (strcmp(cmd->valuestring, "sd_next") == 0) { sd_player_next(); }
        else if (strcmp(cmd->valuestring, "sd_prev") == 0) { sd_player_prev(); }
        else if (strcmp(cmd->valuestring, "sd_stop") == 0) { sd_player_stop(); }
        else if (strcmp(cmd->valuestring, "sd_pause") == 0)   { sd_player_toggle_pause(); }
        else if (strcmp(cmd->valuestring, "sd_shuffle") == 0) { sd_player_toggle_shuffle(); }
        else if (strcmp(cmd->valuestring, "sd_repeat") == 0)  { sd_player_cycle_repeat(); }
        else if (strcmp(cmd->valuestring, "sd_list") == 0) {
            cJSON *dir = cJSON_GetObjectItem(json, "dir");
            int count = sd_player_scan((dir && cJSON_IsString(dir)) ? dir->valuestring : NULL);

            // Parent within the music tree (empty at the root).
            const char *cur  = sd_player_dir();
            const char *root = sd_player_root();
            char parent[192] = "";
            if (strcmp(cur, root) != 0) {
                strncpy(parent, cur, sizeof(parent) - 1);
                parent[sizeof(parent) - 1] = 0;
                char *s = strrchr(parent, '/');
                if (s && s != parent) *s = 0;
                if (strlen(parent) < strlen(root)) strcpy(parent, root);
            }

            cJSON *resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "type", "sd_list");
            cJSON_AddStringToObject(resp, "dir", cur);
            cJSON_AddStringToObject(resp, "parent", parent);
            cJSON *folders = cJSON_AddArrayToObject(resp, "folders");
            for (int i = 0; i < sd_player_folder_count(); i++) {
                const char *f = sd_player_folder(i);
                if (f) cJSON_AddItemToArray(folders, cJSON_CreateString(f));
            }
            cJSON *arr = cJSON_AddArrayToObject(resp, "tracks");
            for (int i = 0; i < count; i++) {
                const char *t = sd_player_track(i);
                if (t) cJSON_AddItemToArray(arr, cJSON_CreateString(t));
            }
            char *out = cJSON_PrintUnformatted(resp);
            if (out) { ws_broadcast(out, strlen(out)); free(out); }
            cJSON_Delete(resp);
        }

        // SCREENS
        else if (strcmp(cmd->valuestring, "set_screen") == 0) {
            cJSON *v = cJSON_GetObjectItem(json, "value");
            if (v && cJSON_IsString(v)) {
                const char *scr = v->valuestring;
                if      (strcmp(scr, "radio") == 0) settings_set_screen(SCREEN_RADIO);
                // "clock" kept for older clients — the clock screen is now the Home hub.
                else if (strcmp(scr, "home")  == 0) settings_set_screen(SCREEN_HOME);
                else if (strcmp(scr, "clock") == 0) settings_set_screen(SCREEN_HOME);
                else if (strcmp(scr, "bt")    == 0) settings_set_screen(SCREEN_BT);
                else ESP_LOGW(TAG, "set_screen: unknown screen '%s'", scr);
            }
        }
    }

    cJSON_Delete(json);

    free(ws_pkt.payload);

    return ESP_OK;
}


/*
void ws_register(httpd_handle_t server)
*/
void ws_register(httpd_handle_t server)
{
    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true
    };

    httpd_register_uri_handler(server, &ws_uri);

    ESP_LOGI(TAG, "WebSocket registered");
}


/*
void ws_set_server(httpd_handle_t server)
*/
void ws_set_server(httpd_handle_t server)
{
    g_server = server;
}


/*
static void ws_broadcast(const char *data, size_t len)
*/
static void ws_broadcast(const char *data, size_t len) {
    if (!s_broadcast_queue) return;
    char *copy = malloc(len + 1);
    if (!copy) return;
    memcpy(copy, data, len + 1);
    if (xQueueSend(s_broadcast_queue, &copy, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Broadcast queue full, dropping");
        free(copy);
    }
}


/*
static void send_full_state(void)
*/
static void send_full_state(void)
{
    app_state_t *s = app_state_get();

    const char *state_str = "unknown";

    switch (s->radio_state) {
        case RADIO_STATE_STOPPED:   state_str = "stopped"; break;
        case RADIO_STATE_PLAYING:   state_str = "playing"; break;
        case RADIO_STATE_BUFFERING: state_str = "buffering"; break;
        case RADIO_STATE_ERROR:     state_str = "error"; break;
        case RADIO_STATE_FINISHED:  state_str = "finished"; break;
    }

    cJSON *json = cJSON_CreateObject();

    cJSON_AddStringToObject(json, "type", "state");
    cJSON_AddBoolToObject(json, "bt_enable", s->bt_enable);
    cJSON_AddNumberToObject(json, "bt_state", s->bt_state);
    cJSON_AddNumberToObject(json, "bt_volume", s->bt_volume);
    cJSON_AddBoolToObject(json, "bt_vol_sync", s->bt_vol_sync);
    cJSON_AddStringToObject(json, "bt_title",  s->bt_title);
    cJSON_AddStringToObject(json, "bt_artist", s->bt_artist);
    cJSON_AddNumberToObject(json, "bt_duration_ms", s->bt_duration_ms);
    cJSON_AddNumberToObject(json, "bt_position_s",  s->bt_position_s);
    cJSON_AddStringToObject(json, "bt_codec", s->bt_codec);
    cJSON_AddNumberToObject(json, "bt_sample_rate", s->bt_sample_rate);
    cJSON_AddNumberToObject(json, "bt_bits", s->bt_bits);
    cJSON_AddStringToObject(json, "radio", state_str);
    cJSON_AddNumberToObject(json, "volume", s->volume);
    cJSON_AddStringToObject(json, "url", s->url);
    cJSON_AddStringToObject(json, "station_name", s->station_name[0] ? s->station_name : "");
    cJSON_AddStringToObject(json, "title", s->title[0] ? s->title : "");
    cJSON_AddNumberToObject(json, "curr_index", s->curr_index);
    cJSON_AddNumberToObject(json, "sr", s->sample_rate);
    cJSON_AddNumberToObject(json, "bits", s->bits);
    cJSON_AddNumberToObject(json, "ch", s->channels);
    cJSON_AddNumberToObject(json, "br", s->bitrate);
    cJSON_AddNumberToObject(json, "fmt", s->codec_fmt);
    cJSON_AddBoolToObject(json, "sd_active", s->sd_active);
    cJSON_AddNumberToObject(json, "sd_index", s->sd_index);
    cJSON_AddNumberToObject(json, "sd_count", s->sd_count);
    cJSON_AddStringToObject(json, "sd_track", s->sd_track[0] ? s->sd_track : "");
    cJSON_AddStringToObject(json, "sd_dir", s->sd_dir[0] ? s->sd_dir : "");
    cJSON_AddBoolToObject(json, "sd_paused", s->sd_paused);
    cJSON_AddBoolToObject(json, "sd_shuffle", s->sd_shuffle);
    cJSON_AddNumberToObject(json, "sd_repeat", s->sd_repeat);

    // WiFi RSSI (dBm) — 0 when STA not connected / AP mode
    wifi_ap_record_t ap;
    cJSON_AddNumberToObject(json, "rssi",
        esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0);

    cJSON *eq = cJSON_CreateIntArray(s->eq, 10);
    cJSON_AddItemToObject(json, "eq", eq);

    // NULL on heap exhaustion — skip the broadcast rather than strlen(NULL).
    char *str = cJSON_PrintUnformatted(json);
    if (str) {
        ws_broadcast(str, strlen(str));
        free(str);
    }
    cJSON_Delete(json);
}


/*
static void on_state_change(void)
*/
static void on_state_change(void)
{
    send_full_state();
}


/*
void ws_on_close(httpd_handle_t hd, int fd)
   Called by esp_http_server when any socket closes (not only WS).
   After overriding close_fn we are responsible for actually closing the fd.
*/
void ws_on_close(httpd_handle_t hd, int fd)
{
    (void)hd;
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_clients[i] == fd) {
            ws_clients[i] = 0;
            ESP_LOGI(TAG, "Client %d closed, slot freed", fd);
            break;
        }
    }
    close(fd);
}


/*
void ws_send_bt_log(const char *line)
*/
void ws_send_bt_log(const char *line)
{
    cJSON *json = cJSON_CreateObject();

    cJSON_AddStringToObject(json, "type", "bt_log");
    cJSON_AddStringToObject(json, "data", line);

    // NULL on heap exhaustion — skip the broadcast rather than strlen(NULL).
    char *str = cJSON_PrintUnformatted(json);
    if (str) {
        ws_broadcast(str, strlen(str));
        free(str);
    }
    cJSON_Delete(json);
}