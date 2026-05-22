#include "screensaver_mqtt.h"
#include "ui_screen.h"
#include "ui_profile.h"
#include "ui_events.h"
#include "theme.h"
#include "mqtt_svc.h"
#include "mqtt_config.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "SS_MQTT";

static lv_obj_t *s_root         = NULL;
static lv_obj_t *s_value_lbl    = NULL;
static lv_obj_t *s_status_label = NULL;

// Latest payload from the broker, written by the mqtt task. apply_async on the
// LVGL task reads the snapshot and pushes it into the label.
static char              s_pending[64];
static SemaphoreHandle_t s_pending_mtx = NULL;

static void refresh_status(void)
{
    if (!s_status_label) return;
    mqtt_config_t *c = mqtt_config_get();

    if (!c->enabled) {
        lv_label_set_text(s_status_label, "MQTT disabled");
    } else if (!mqtt_svc_is_connected()) {
        lv_label_set_text(s_status_label, "Connecting…");
    } else if (!c->screensaver.topic_state[0]) {
        lv_label_set_text(s_status_label, "");
    } else {
        lv_label_set_text(s_status_label, "Connected");
    }
}

static void apply_async(void *arg)
{
    (void)arg;
    if (!s_value_lbl) return;

    mqtt_config_t *c = mqtt_config_get();
    char value[80];
    xSemaphoreTake(s_pending_mtx, portMAX_DELAY);
    if (c->screensaver.unit[0]) snprintf(value, sizeof(value), "%s %s", s_pending, c->screensaver.unit);
    else                        snprintf(value, sizeof(value), "%s",    s_pending);
    xSemaphoreGive(s_pending_mtx);

    lv_label_set_text(s_value_lbl, value);
    refresh_status();
}

// Called from the esp-mqtt task. Take a snapshot and hop to the LVGL task.
static void on_ss_state(const char *value)
{
    if (!s_pending_mtx) return;
    xSemaphoreTake(s_pending_mtx, portMAX_DELAY);
    size_t n = strlen(value);
    if (n >= sizeof(s_pending)) n = sizeof(s_pending) - 1;
    memcpy(s_pending, value, n);
    s_pending[n] = '\0';
    xSemaphoreGive(s_pending_mtx);

    lv_async_call(apply_async, NULL);
}

static const lv_font_t *title_font(void)
{
    return (DISPLAY_WIDTH >= 240) ? &lv_font_montserrat_18_pl : &lv_font_montserrat_14_pl;
}

static const lv_font_t *value_font(void)
{
    // Built-in lv_font_montserrat_48 — full ASCII. Custom _72/_80/_96 in this
    // project are digits-only (generated for the clock screensaver) and can't
    // render arbitrary MQTT payloads like "ON"/"OFF".
    return &lv_font_montserrat_48;
}

static void mqtt_ss_create(lv_obj_t *parent)
{
    const int W = DISPLAY_WIDTH;
    const int H = DISPLAY_HEIGHT;
    const ui_theme_colors_t *th = theme_get();
    mqtt_config_t *c = mqtt_config_get();

    s_pending_mtx = xSemaphoreCreateMutex();
    s_pending[0] = '\0';

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, W, H);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa  (s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (c->screensaver.topic_state[0]) {
        lv_obj_t *title = lv_label_create(s_root);
        lv_label_set_text(title, c->screensaver.title[0] ? c->screensaver.title : "—");
        lv_obj_set_style_text_font(title, title_font(), LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(th->text_muted), LV_PART_MAIN);

        s_value_lbl = lv_label_create(s_root);
        lv_label_set_text(s_value_lbl, "—");
        lv_obj_set_style_text_font(s_value_lbl, value_font(), LV_PART_MAIN);
        lv_obj_set_style_text_color(s_value_lbl, lv_color_hex(th->text_primary), LV_PART_MAIN);
    } else {
        lv_obj_t *empty = lv_label_create(s_root);
        lv_label_set_text(empty, "MQTT — no screensaver topic");
        lv_obj_set_style_text_font(empty, title_font(), LV_PART_MAIN);
        lv_obj_set_style_text_color(empty, lv_color_hex(th->text_muted), LV_PART_MAIN);
    }

    s_status_label = lv_label_create(s_root);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_12_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(th->text_muted), LV_PART_MAIN);

    mqtt_svc_set_ss_state_cb(on_ss_state);
    refresh_status();

    ESP_LOGI(TAG, "Created (%dx%d, topic='%s')", W, H, c->screensaver.topic_state);
}

static void mqtt_ss_destroy(void)
{
    mqtt_svc_set_ss_state_cb(NULL);

    if (s_root) { lv_obj_delete(s_root); s_root = NULL; }
    s_value_lbl = NULL;
    s_status_label = NULL;

    if (s_pending_mtx) { vSemaphoreDelete(s_pending_mtx); s_pending_mtx = NULL; }
    ESP_LOGI(TAG, "Destroyed");
}

static void mqtt_ss_on_event(const ui_event_t *ev)
{
    if (ev->type == UI_EVT_STATE_CHANGED) refresh_status();
}

const ui_screen_t screensaver_mqtt = {
    .create      = mqtt_ss_create,
    .destroy     = mqtt_ss_destroy,
    .apply_theme = NULL,
    .on_event    = mqtt_ss_on_event,
    .on_input    = NULL,
    .name        = "ss_mqtt",
};
