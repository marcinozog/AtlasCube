#include "screensaver_dashboard.h"
#include "ui_screen.h"
#include "ui_profile.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "settings.h"
#include "buzzer.h"
#include "fonts/ui_fonts.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "SS_DASHBOARD";

// Buzzer pattern played when a notification fires. (freq_hz, duration_ms)
// pairs, freq=0 means silence. Edit to taste.
static const uint16_t DASHBOARD_ALERT_PATTERN[] = {
    988, 120,   0, 60,
    988, 120,   0, 60,
   1319, 220,
};
#define DASHBOARD_ALERT_PAIRS (sizeof(DASHBOARD_ALERT_PATTERN) / sizeof(uint16_t) / 2)

// ---------------------------------------------------------------------------
// Widget config
// ---------------------------------------------------------------------------

typedef enum {
    WIDGET_LABEL,
    WIDGET_BIG_NUMBER,
} widget_type_t;

typedef struct {
    widget_type_t  type;
    char           title[32];
    char           url[256];
    char           json_path[64];   // e.g. "rates[0].mid"; empty = use root
    uint32_t       poll_interval_ms;
    char           suffix[16];      // appended to the value (may be empty)
} dashboard_widget_t;

#define WIDGET_COUNT 1
static dashboard_widget_t s_widgets[WIDGET_COUNT];

// ---------------------------------------------------------------------------
// Notification — snapshot of settings + runtime armed/primed flags
// ---------------------------------------------------------------------------

typedef struct {
    bool   enabled;
    int    value_type;       // dashboard_value_type_t (0=number, 1=string)
    bool   num_low_en;
    double num_low;
    bool   num_high_en;
    double num_high;
    bool   str_eq_en;
    char   str_eq[32];
    bool   str_ne_en;
    char   str_ne[32];
} notify_config_t;

typedef struct {
    bool primed;             // false → first fetch only sets state, no fire
    bool num_low_armed;
    bool num_high_armed;
    bool str_eq_armed;
    bool str_ne_armed;
} notify_state_t;

static notify_config_t s_notify_cfg;
static notify_state_t  s_notify_state;

static void populate_widgets_from_settings(void)
{
    const app_settings_t *s = settings_get();
    dashboard_widget_t   *w = &s_widgets[0];

    w->type = WIDGET_BIG_NUMBER;
    w->title[0] = '\0';     strncpy(w->title,     s->dashboard.title,     sizeof(w->title)     - 1);
    w->url[0]   = '\0';     strncpy(w->url,       s->dashboard.url,       sizeof(w->url)       - 1);
    w->json_path[0] = '\0'; strncpy(w->json_path, s->dashboard.json_path, sizeof(w->json_path) - 1);
    w->suffix[0]    = '\0'; strncpy(w->suffix,    s->dashboard.suffix,    sizeof(w->suffix)    - 1);
    w->poll_interval_ms = (s->dashboard.poll_interval_ms >= 5000)
                          ? (uint32_t)s->dashboard.poll_interval_ms
                          : 60000U;

    // Snapshot notification config (settings are read-only after this point —
    // changes via web UI take effect after the screensaver is re-activated).
    s_notify_cfg.enabled     = s->dashboard.notify_enabled;
    s_notify_cfg.value_type  = s->dashboard.value_type;
    s_notify_cfg.num_low_en  = s->dashboard.notify_num_low_en;
    s_notify_cfg.num_low     = s->dashboard.notify_num_low;
    s_notify_cfg.num_high_en = s->dashboard.notify_num_high_en;
    s_notify_cfg.num_high    = s->dashboard.notify_num_high;
    s_notify_cfg.str_eq_en   = s->dashboard.notify_str_eq_en;
    s_notify_cfg.str_eq[0]   = '\0'; strncpy(s_notify_cfg.str_eq, s->dashboard.notify_str_eq, sizeof(s_notify_cfg.str_eq) - 1);
    s_notify_cfg.str_ne_en   = s->dashboard.notify_str_ne_en;
    s_notify_cfg.str_ne[0]   = '\0'; strncpy(s_notify_cfg.str_ne, s->dashboard.notify_str_ne, sizeof(s_notify_cfg.str_ne) - 1);

    // Reset runtime state — all armed=true means "ready to fire", primed=false
    // means "first fetch will just initialize, no melody on cold start".
    s_notify_state.primed         = false;
    s_notify_state.num_low_armed  = true;
    s_notify_state.num_high_armed = true;
    s_notify_state.str_eq_armed   = true;
    s_notify_state.str_ne_armed   = true;
}

// ---------------------------------------------------------------------------
// Per-widget shared state (fetcher writes, lvgl_task reads)
// ---------------------------------------------------------------------------

#define VALUE_BUF_LEN 32

typedef struct {
    char              text[VALUE_BUF_LEN];
    atomic_uint_fast32_t version;   // bumped on every successful update
    SemaphoreHandle_t mutex;
} widget_state_t;

static widget_state_t s_state[WIDGET_COUNT];
static uint32_t       s_last_seen_version[WIDGET_COUNT];

// ---------------------------------------------------------------------------
// LVGL handles
// ---------------------------------------------------------------------------

static lv_obj_t   *s_root            = NULL;
static lv_obj_t   *s_value_lbl[WIDGET_COUNT] = {0};
static lv_timer_t *s_refresh_timer   = NULL;

// ---------------------------------------------------------------------------
// Fetcher task
// ---------------------------------------------------------------------------

static TaskHandle_t       s_fetch_task     = NULL;
static volatile bool      s_fetch_stop     = false;
static volatile bool      s_settings_dirty = false;

void screensaver_dashboard_settings_changed(void)
{
    s_settings_dirty = true;
}

#define HTTP_BUF_LEN 4096

static void set_widget_text(int i, const char *text)
{
    widget_state_t *st = &s_state[i];
    xSemaphoreTake(st->mutex, portMAX_DELAY);
    strncpy(st->text, text, VALUE_BUF_LEN - 1);
    st->text[VALUE_BUF_LEN - 1] = '\0';
    xSemaphoreGive(st->mutex);
    atomic_fetch_add(&st->version, 1);
}

// Resolve a JSON path like "rates[0].mid" against a parsed cJSON tree.
// Supported: dot-separated keys and bracketed numeric indices.
static cJSON *json_resolve_path(cJSON *root, const char *path)
{
    if (!root || !path || !*path) return root;

    cJSON      *cur = root;
    const char *p   = path;
    char        key[64];

    while (*p && cur) {
        if (*p == '.') { p++; continue; }

        if (*p == '[') {
            p++;
            int idx = (int)strtol(p, (char **)&p, 10);
            if (*p == ']') p++;
            if (!cJSON_IsArray(cur)) return NULL;
            cur = cJSON_GetArrayItem(cur, idx);
            continue;
        }

        size_t n = 0;
        while (*p && *p != '.' && *p != '[' && n < sizeof(key) - 1) {
            key[n++] = *p++;
        }
        key[n] = '\0';
        cur = cJSON_GetObjectItemCaseSensitive(cur, key);
    }
    return cur;
}

static void format_value(const dashboard_widget_t *w, cJSON *node, char *out, size_t cap)
{
    if (!node) { snprintf(out, cap, "—"); return; }

    const char *suffix = w->suffix;

    if (cJSON_IsNumber(node)) {
        snprintf(out, cap, "%.4g%s", node->valuedouble, suffix);
    } else if (cJSON_IsString(node) && node->valuestring) {
        snprintf(out, cap, "%s%s", node->valuestring, suffix);
    } else if (cJSON_IsBool(node)) {
        snprintf(out, cap, "%s%s", cJSON_IsTrue(node) ? "true" : "false", suffix);
    } else {
        snprintf(out, cap, "?");
    }
}

static esp_err_t http_get(const char *url, char *body, int cap, int *out_len)
{
    esp_http_client_config_t cfg = {
        .url                = url,
        .timeout_ms         = 8000,
        .crt_bundle_attach  = esp_crt_bundle_attach,
        .user_agent         = "AtlasCube/1.0",
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (!cli) return ESP_FAIL;

    esp_err_t err = esp_http_client_open(cli, 0);
    if (err != ESP_OK) { esp_http_client_cleanup(cli); return err; }

    int content_len = esp_http_client_fetch_headers(cli);
    (void)content_len;

    int status = esp_http_client_get_status_code(cli);
    if (status / 100 != 2) {
        ESP_LOGW(TAG, "HTTP %d for %s", status, url);
        esp_http_client_close(cli);
        esp_http_client_cleanup(cli);
        return ESP_FAIL;
    }

    int total = 0;
    while (total < cap - 1) {
        int r = esp_http_client_read(cli, body + total, cap - 1 - total);
        if (r <= 0) break;
        total += r;
    }
    body[total] = '\0';

    esp_http_client_close(cli);
    esp_http_client_cleanup(cli);

    *out_len = total;
    return ESP_OK;
}

// Threshold check + buzzer trigger. Called only after a successful parse,
// with the *raw* extracted node (before suffix formatting). Updates the
// armed/primed flags in s_notify_state.
static void check_notification(cJSON *node)
{
    if (!s_notify_cfg.enabled || !node) return;

    // First call after activation: just initialize armed flags so we don't
    // fire on the cold-start value, then mark primed and bail out.
    if (!s_notify_state.primed) {
        if (s_notify_cfg.value_type == DASHBOARD_VALUE_NUMBER && cJSON_IsNumber(node)) {
            double v = node->valuedouble;
            s_notify_state.num_low_armed  = (v >= s_notify_cfg.num_low);   // disarmed if already below
            s_notify_state.num_high_armed = (v <= s_notify_cfg.num_high);  // disarmed if already above
        } else if (s_notify_cfg.value_type == DASHBOARD_VALUE_STRING && cJSON_IsString(node) && node->valuestring) {
            const char *s = node->valuestring;
            s_notify_state.str_eq_armed = (strcmp(s, s_notify_cfg.str_eq) != 0);
            s_notify_state.str_ne_armed = (strcmp(s, s_notify_cfg.str_ne) == 0);
        }
        s_notify_state.primed = true;
        return;
    }

    bool fire = false;

    if (s_notify_cfg.value_type == DASHBOARD_VALUE_NUMBER && cJSON_IsNumber(node)) {
        double v = node->valuedouble;

        if (s_notify_cfg.num_low_en) {
            if (s_notify_state.num_low_armed && v < s_notify_cfg.num_low) {
                ESP_LOGI(TAG, "notify: value %.4g < low %.4g", v, s_notify_cfg.num_low);
                fire = true;
                s_notify_state.num_low_armed = false;
            } else if (!s_notify_state.num_low_armed && v >= s_notify_cfg.num_low) {
                s_notify_state.num_low_armed = true;
            }
        }
        if (s_notify_cfg.num_high_en) {
            if (s_notify_state.num_high_armed && v > s_notify_cfg.num_high) {
                ESP_LOGI(TAG, "notify: value %.4g > high %.4g", v, s_notify_cfg.num_high);
                fire = true;
                s_notify_state.num_high_armed = false;
            } else if (!s_notify_state.num_high_armed && v <= s_notify_cfg.num_high) {
                s_notify_state.num_high_armed = true;
            }
        }
    } else if (s_notify_cfg.value_type == DASHBOARD_VALUE_STRING && cJSON_IsString(node) && node->valuestring) {
        const char *s = node->valuestring;

        if (s_notify_cfg.str_eq_en) {
            bool match = (strcmp(s, s_notify_cfg.str_eq) == 0);
            if (s_notify_state.str_eq_armed && match) {
                ESP_LOGI(TAG, "notify: value == \"%s\"", s_notify_cfg.str_eq);
                fire = true;
                s_notify_state.str_eq_armed = false;
            } else if (!s_notify_state.str_eq_armed && !match) {
                s_notify_state.str_eq_armed = true;   // rearm when value moves away
            }
        }
        if (s_notify_cfg.str_ne_en) {
            bool differ = (strcmp(s, s_notify_cfg.str_ne) != 0);
            if (s_notify_state.str_ne_armed && differ) {
                ESP_LOGI(TAG, "notify: value != \"%s\" (got \"%s\")", s_notify_cfg.str_ne, s);
                fire = true;
                s_notify_state.str_ne_armed = false;
            } else if (!s_notify_state.str_ne_armed && !differ) {
                s_notify_state.str_ne_armed = true;
            }
        }
    }

    if (fire) buzzer_beep_pattern(DASHBOARD_ALERT_PATTERN, DASHBOARD_ALERT_PAIRS);
}

static void fetch_widget(int i)
{
    const dashboard_widget_t *w = &s_widgets[i];

    char *body = malloc(HTTP_BUF_LEN);
    if (!body) {
        ESP_LOGE(TAG, "OOM for %s", w->title);
        set_widget_text(i, "OOM");
        return;
    }

    int len = 0;
    esp_err_t err = http_get(w->url, body, HTTP_BUF_LEN, &len);
    if (err != ESP_OK || len <= 0) {
        ESP_LOGW(TAG, "%s: fetch failed (%s)", w->title, esp_err_to_name(err));
        set_widget_text(i, "net err");
        free(body);
        return;
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        ESP_LOGW(TAG, "%s: JSON parse failed", w->title);
        set_widget_text(i, "parse err");
        return;
    }

    cJSON *node = json_resolve_path(root, w->json_path);
    char value[VALUE_BUF_LEN];
    format_value(w, node, value, sizeof(value));

    ESP_LOGI(TAG, "%s = %s", w->title, value);
    set_widget_text(i, value);

    // Single-widget MVP: notification logic operates on widget 0.
    if (i == 0) check_notification(node);

    cJSON_Delete(root);
}

static void fetcher_task(void *arg)
{
    (void)arg;
    uint32_t next_due_ms[WIDGET_COUNT] = {0};   // 0 = fetch immediately

    while (!s_fetch_stop) {
        // Pick up settings changes saved via web UI while we were running.
        // Re-snapshot config, reset notify state (so new thresholds aren't
        // pre-disarmed against the cached value), and force an immediate
        // refetch with the new URL / poll interval.
        if (s_settings_dirty) {
            s_settings_dirty = false;
            populate_widgets_from_settings();
            memset(next_due_ms, 0, sizeof(next_due_ms));
            ESP_LOGI(TAG, "settings reloaded mid-flight");
        }

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        uint32_t sleep_ms = 1000;

        for (size_t i = 0; i < WIDGET_COUNT; i++) {
            if (s_fetch_stop) break;
            if ((int32_t)(now_ms - next_due_ms[i]) >= 0) {
                fetch_widget((int)i);
                next_due_ms[i] = now_ms + s_widgets[i].poll_interval_ms;
            }
            uint32_t until = next_due_ms[i] - now_ms;
            if (until < sleep_ms) sleep_ms = until;
        }

        // Wake periodically so we can observe s_fetch_stop without lag.
        if (sleep_ms > 500) sleep_ms = 500;
        vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    }

    s_fetch_task = NULL;
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// LVGL refresh — pull from shared state to labels
// ---------------------------------------------------------------------------

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    for (size_t i = 0; i < WIDGET_COUNT; i++) {
        uint32_t v = atomic_load(&s_state[i].version);
        if (v == s_last_seen_version[i]) continue;
        s_last_seen_version[i] = v;

        char buf[VALUE_BUF_LEN];
        xSemaphoreTake(s_state[i].mutex, portMAX_DELAY);
        memcpy(buf, s_state[i].text, VALUE_BUF_LEN);
        xSemaphoreGive(s_state[i].mutex);

        if (s_value_lbl[i]) lv_label_set_text(s_value_lbl[i], buf);
    }
}

// ---------------------------------------------------------------------------
// LVGL build
// ---------------------------------------------------------------------------

static const lv_font_t *big_font_for_display(void)
{
    return (DISPLAY_WIDTH >= 400) ? &lv_font_montserrat_72 : &lv_font_montserrat_48;
}

static void build_widget(lv_obj_t *parent, int i)
{
    const dashboard_widget_t *w = &s_widgets[i];

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(card, 4, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, w->title);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0x808080), LV_PART_MAIN);

    lv_obj_t *value = lv_label_create(card);
    lv_label_set_text(value, "—");
    const lv_font_t *font = (w->type == WIDGET_BIG_NUMBER)
                            ? big_font_for_display()
                            : &lv_font_montserrat_18_pl;
    lv_obj_set_style_text_font(value, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(value, lv_color_white(), LV_PART_MAIN);

    s_value_lbl[i] = value;
}

static void dashboard_create(lv_obj_t *parent)
{
    const int W = DISPLAY_WIDTH;
    const int H = DISPLAY_HEIGHT;

    populate_widgets_from_settings();
    s_settings_dirty = false;   // create() already pulled fresh snapshot

    for (size_t i = 0; i < WIDGET_COUNT; i++) {
        s_state[i].mutex = xSemaphoreCreateMutex();
        s_state[i].text[0] = '\0';
        atomic_store(&s_state[i].version, 0);
        s_last_seen_version[i] = 0;
    }

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, W, H);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (size_t i = 0; i < WIDGET_COUNT; i++) {
        build_widget(s_root, (int)i);
    }

    s_refresh_timer = lv_timer_create(refresh_cb, 500, NULL);

    s_fetch_stop = false;
    xTaskCreatePinnedToCore(fetcher_task, "ss_dash_fetch", 6144, NULL, 5, &s_fetch_task, tskNO_AFFINITY);

    ESP_LOGI(TAG, "Created (%dx%d, widgets=%u)", W, H, (unsigned)WIDGET_COUNT);
}

static void dashboard_destroy(void)
{
    s_fetch_stop = true;
    // Wait for the task to self-delete; bounded by HTTP timeout.
    int waited_ms = 0;
    while (s_fetch_task && waited_ms < 10000) {
        vTaskDelay(pdMS_TO_TICKS(50));
        waited_ms += 50;
    }
    if (s_fetch_task) {
        ESP_LOGW(TAG, "fetcher did not exit in time, forcing delete");
        vTaskDelete(s_fetch_task);
        s_fetch_task = NULL;
    }

    if (s_refresh_timer) { lv_timer_delete(s_refresh_timer); s_refresh_timer = NULL; }
    if (s_root)          { lv_obj_delete(s_root);            s_root          = NULL; }

    for (size_t i = 0; i < WIDGET_COUNT; i++) {
        s_value_lbl[i] = NULL;
        if (s_state[i].mutex) { vSemaphoreDelete(s_state[i].mutex); s_state[i].mutex = NULL; }
    }
    ESP_LOGI(TAG, "Destroyed");
}

const ui_screen_t screensaver_dashboard = {
    .create      = dashboard_create,
    .destroy     = dashboard_destroy,
    .apply_theme = NULL,
    .on_event    = NULL,
    .on_input    = NULL,
    .name        = "ss_dashboard",
};
