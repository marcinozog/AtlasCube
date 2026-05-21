#include "sdkconfig.h"

#if CONFIG_MQTT_ENABLE

#include "mqtt_svc.h"
#include "mqtt_config.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_state.h"
#include "settings.h"
#include "radio_service.h"
#include "playlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t s_client = NULL;
static bool                     s_connected = false;
static mqtt_svc_widget_state_cb_t s_widget_cb = NULL;

// Cached values for state diffing — publish only on real change.
static radio_state_t s_last_radio_state = (radio_state_t)-1;
static int           s_last_volume      = -1;
static int           s_last_index       = -1;
static char          s_last_title[128]  = {0};
static char          s_last_station[64] = {0};

// ─────────────────────────────────────────────────────────────────────────────
// Topic helpers
// ─────────────────────────────────────────────────────────────────────────────
static void make_topic(char *out, size_t out_sz, const char *suffix)
{
    mqtt_config_t *c = mqtt_config_get();
    const char *base = c->base_topic[0] ? c->base_topic : "atlascube";
    snprintf(out, out_sz, "%s/%s", base, suffix);
}

static const char *radio_state_str(radio_state_t st)
{
    switch (st) {
        case RADIO_STATE_PLAYING:   return "playing";
        case RADIO_STATE_BUFFERING: return "buffering";
        case RADIO_STATE_STOPPED:   return "stopped";
        case RADIO_STATE_ERROR:     return "error";
        default:                    return "unknown";
    }
}

static void publish_str(const char *suffix, const char *payload, bool retain)
{
    if (!s_connected || !s_client) return;
    char topic[160];
    make_topic(topic, sizeof(topic), suffix);
    esp_mqtt_client_publish(s_client, topic, payload, 0, 0, retain ? 1 : 0);
}

static void publish_state_full(void)
{
    if (!s_connected) return;
    app_state_t *as = app_state_get();

    publish_str("state/playing", radio_state_str(as->radio_state), true);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", as->volume);
    publish_str("state/volume", buf, true);

    snprintf(buf, sizeof(buf), "%d", as->curr_index);
    publish_str("state/station_index", buf, true);

    publish_str("state/station", as->station_name, true);
    publish_str("state/title",   as->title,        true);

    s_last_radio_state = as->radio_state;
    s_last_volume     = as->volume;
    s_last_index      = as->curr_index;
    strncpy(s_last_title,   as->title,        sizeof(s_last_title)   - 1);
    strncpy(s_last_station, as->station_name, sizeof(s_last_station) - 1);
}

static void on_state_change(void)
{
    if (!s_connected) return;
    app_state_t *as = app_state_get();

    if (as->radio_state != s_last_radio_state) {
        s_last_radio_state = as->radio_state;
        publish_str("state/playing", radio_state_str(as->radio_state), true);
    }
    if (as->volume != s_last_volume) {
        s_last_volume = as->volume;
        char buf[16]; snprintf(buf, sizeof(buf), "%d", as->volume);
        publish_str("state/volume", buf, true);
    }
    if (as->curr_index != s_last_index) {
        s_last_index = as->curr_index;
        char buf[16]; snprintf(buf, sizeof(buf), "%d", as->curr_index);
        publish_str("state/station_index", buf, true);
    }
    if (strncmp(as->title, s_last_title, sizeof(s_last_title)) != 0) {
        strncpy(s_last_title, as->title, sizeof(s_last_title) - 1);
        publish_str("state/title", as->title, true);
    }
    if (strncmp(as->station_name, s_last_station, sizeof(s_last_station)) != 0) {
        strncpy(s_last_station, as->station_name, sizeof(s_last_station) - 1);
        publish_str("state/station", as->station_name, true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Command dispatcher (radio control)
// ─────────────────────────────────────────────────────────────────────────────
static void handle_cmd(const char *suffix, const char *payload)
{
    ESP_LOGI(TAG, "cmd %s = '%s'", suffix, payload);

    if (strcmp(suffix, "play") == 0) {
        int n = playlist_get_count();
        int idx = app_state_get()->curr_index;
        if (idx < 0 || idx >= n) idx = 0;
        radio_play_index(idx);
    } else if (strcmp(suffix, "stop") == 0) {
        radio_stop();
    } else if (strcmp(suffix, "next") == 0) {
        int n = playlist_get_count();
        if (n > 0) {
            int idx = (app_state_get()->curr_index + 1) % n;
            radio_play_index(idx);
        }
    } else if (strcmp(suffix, "prev") == 0) {
        int n = playlist_get_count();
        if (n > 0) {
            int idx = app_state_get()->curr_index - 1;
            if (idx < 0) idx = n - 1;
            radio_play_index(idx);
        }
    } else if (strcmp(suffix, "volume") == 0) {
        int v = atoi(payload);
        if (v < 0)   v = 0;
        if (v > 100) v = 100;
        settings_set_volume(v);
    } else if (strcmp(suffix, "station") == 0) {
        int idx = atoi(payload);
        if (idx >= 0 && idx < playlist_get_count()) {
            radio_play_index(idx);
        } else {
            ESP_LOGW(TAG, "station index %d out of range", idx);
        }
    } else {
        ESP_LOGW(TAG, "unknown cmd suffix: %s", suffix);
    }
}

static const char *cmd_suffix(const char *topic, int topic_len)
{
    char prefix[128];
    make_topic(prefix, sizeof(prefix), "cmd/");
    size_t plen = strlen(prefix);
    if ((int)plen >= topic_len) return NULL;
    if (strncmp(topic, prefix, plen) != 0) return NULL;
    return topic + plen;
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON path extraction — minimal "key":value finder, no nesting.
// Writes extracted value (or copy of input) into `out` (null-terminated).
// Returns true on success.
// ─────────────────────────────────────────────────────────────────────────────
static bool extract_json_value(const char *payload, int payload_len,
                               const char *key,
                               char *out, size_t out_sz)
{
    if (out_sz == 0) return false;
    out[0] = '\0';

    // Build "key" search needle
    char needle[64];
    int  needle_len = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (needle_len <= 0 || needle_len >= (int)sizeof(needle)) return false;

    // Cap copy for strstr
    char buf[512];
    int n = payload_len < (int)sizeof(buf) - 1 ? payload_len : (int)sizeof(buf) - 1;
    memcpy(buf, payload, n);
    buf[n] = '\0';

    const char *k = strstr(buf, needle);
    if (!k) return false;
    k += needle_len;
    while (*k == ' ' || *k == ':' || *k == '\t') k++;

    if (*k == '"') {
        k++;
        size_t i = 0;
        while (*k && *k != '"' && i < out_sz - 1) out[i++] = *k++;
        out[i] = '\0';
        return true;
    }
    // Bare number / bool / null
    size_t i = 0;
    while (*k && *k != ',' && *k != '}' && *k != ' ' && *k != '\n' && *k != '\r' && i < out_sz - 1)
        out[i++] = *k++;
    out[i] = '\0';
    return i > 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-widget state dispatch
// ─────────────────────────────────────────────────────────────────────────────
static void dispatch_widget_state(const char *topic, int topic_len,
                                  const char *payload, int payload_len)
{
    mqtt_config_t *c = mqtt_config_get();
    for (int i = 0; i < MQTT_MAX_WIDGETS; ++i) {
        mqtt_widget_t *w = &c->widgets[i];
        if (w->type == MQTT_W_NONE)      continue;
        if (w->topic_state[0] == '\0')   continue;
        size_t tlen = strlen(w->topic_state);
        if (tlen != (size_t)topic_len)   continue;
        if (strncmp(topic, w->topic_state, tlen) != 0) continue;

        char value[64];
        if (w->json_path[0]) {
            if (!extract_json_value(payload, payload_len, w->json_path,
                                    value, sizeof(value))) {
                continue;   // key not present in this message
            }
        } else {
            int n = payload_len < (int)sizeof(value) - 1
                  ? payload_len : (int)sizeof(value) - 1;
            memcpy(value, payload, n);
            value[n] = '\0';
        }
        if (s_widget_cb) s_widget_cb(i, value);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MQTT event handler
// ─────────────────────────────────────────────────────────────────────────────
static void subscribe_widget_states(void)
{
    if (!s_client) return;
    mqtt_config_t *c = mqtt_config_get();
    for (int i = 0; i < MQTT_MAX_WIDGETS; ++i) {
        mqtt_widget_t *w = &c->widgets[i];
        if (w->type == MQTT_W_NONE)    continue;
        if (w->topic_state[0] == '\0') continue;
        esp_mqtt_client_subscribe(s_client, w->topic_state, 0);
    }
}

static void on_mqtt_event(void *handler_args, esp_event_base_t base,
                          int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;
    (void)handler_args; (void)base;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "connected");
            s_connected = true;

            publish_str("status", "online", true);

            char topic[160];
            make_topic(topic, sizeof(topic), "cmd/+");
            esp_mqtt_client_subscribe(s_client, topic, 0);

            subscribe_widget_states();

            s_last_radio_state = (radio_state_t)-1;
            s_last_volume = s_last_index = -1;
            s_last_title[0] = s_last_station[0] = '\0';
            publish_state_full();
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "disconnected");
            s_connected = false;
            break;
        case MQTT_EVENT_DATA: {
            char *topic = (char *)malloc(ev->topic_len + 1);
            char *data  = (char *)malloc(ev->data_len  + 1);
            if (!topic || !data) { free(topic); free(data); break; }
            memcpy(topic, ev->topic, ev->topic_len); topic[ev->topic_len] = '\0';
            memcpy(data,  ev->data,  ev->data_len);  data [ev->data_len]  = '\0';

            const char *suffix = cmd_suffix(topic, ev->topic_len);
            if (suffix) {
                handle_cmd(suffix, data);
            } else {
                dispatch_widget_state(topic, ev->topic_len, data, ev->data_len);
            }
            free(topic); free(data);
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "error event");
            break;
        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────
static void stop_client(void)
{
    if (!s_client) return;
    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client = NULL;
    s_connected = false;
}

static void start_client(void)
{
    mqtt_config_t *c = mqtt_config_get();

    if (!c->enabled) {
        ESP_LOGI(TAG, "disabled in config");
        return;
    }
    if (c->host[0] == '\0') {
        ESP_LOGW(TAG, "host empty — not starting");
        return;
    }

    int port = c->port > 0 ? c->port : 1883;
    char uri[96];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", c->host, port);

    char lwt_topic[160];
    make_topic(lwt_topic, sizeof(lwt_topic), "status");

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,
        .credentials.client_id = c->client_id[0] ? c->client_id : NULL,
        .credentials.username  = c->username[0]  ? c->username  : NULL,
        .credentials.authentication.password = c->password[0] ? c->password : NULL,
        .session.last_will = {
            .topic   = lwt_topic,
            .msg     = "offline",
            .msg_len = 7,
            .qos     = 0,
            .retain  = 1,
        },
        .session.keepalive = 30,
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "client_init failed");
        return;
    }
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, on_mqtt_event, NULL);
    esp_mqtt_client_start(s_client);
    ESP_LOGI(TAG, "started — %s", uri);
}

void mqtt_svc_init(void)
{
    mqtt_config_load();
    app_state_subscribe(on_state_change);
    start_client();
}

void mqtt_svc_reconfigure(void)
{
    stop_client();
    start_client();
}

bool mqtt_svc_is_connected(void) { return s_connected; }

// ─────────────────────────────────────────────────────────────────────────────
// Widget publish
// ─────────────────────────────────────────────────────────────────────────────
static void publish_widget(int idx, const char *payload)
{
    if (!s_connected || idx < 0 || idx >= MQTT_MAX_WIDGETS) return;
    mqtt_widget_t *w = &mqtt_config_get()->widgets[idx];
    if (w->topic_cmd[0] == '\0') return;
    esp_mqtt_client_publish(s_client, w->topic_cmd, payload, 0, 0, 0);
}

void mqtt_svc_publish_widget_bool(int idx, bool on)
{
    publish_widget(idx, on ? "ON" : "OFF");
}

void mqtt_svc_publish_widget_int(int idx, int value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    publish_widget(idx, buf);
}

void mqtt_svc_set_widget_state_cb(mqtt_svc_widget_state_cb_t cb)
{
    s_widget_cb = cb;
}

#endif /* CONFIG_MQTT_ENABLE */
