#include "screen_mqtt.h"
#include "ui_screen.h"
#include "ui_manager.h"
#include "ui_events.h"
#include "theme.h"
#include "settings.h"
#include "mqtt_svc.h"
#include "lvgl.h"
#include "esp_log.h"
#include <string.h>
#include <stdatomic.h>

static const char *TAG = "SCR_MQTT";

static lv_obj_t  *s_root          = NULL;
static lv_obj_t  *s_title_label   = NULL;
static lv_obj_t  *s_switch        = NULL;
static lv_obj_t  *s_status_label  = NULL;
static lv_timer_t *s_poll_timer   = NULL;

// Set from the esp-mqtt task by mqtt_svc when the state topic delivers a new
// value. The LVGL timer polls these on the UI task and applies changes safely.
static atomic_bool s_external_state = ATOMIC_VAR_INIT(false);
static atomic_bool s_external_dirty = ATOMIC_VAR_INIT(false);

static void on_mqtt_toggle_state(bool on)
{
    atomic_store(&s_external_state, on);
    atomic_store(&s_external_dirty, true);
}

static void apply_switch_state(bool on)
{
    if (!s_switch) return;
    if (on) lv_obj_add_state(s_switch, LV_STATE_CHECKED);
    else    lv_obj_clear_state(s_switch, LV_STATE_CHECKED);
}

static void refresh_status(void)
{
    if (!s_status_label) return;
    app_settings_t *as = settings_get();
    if (!as->mqtt.enabled) {
        lv_label_set_text(s_status_label, "MQTT disabled");
    } else if (!mqtt_svc_is_connected()) {
        lv_label_set_text(s_status_label, "Connecting…");
    } else if (as->mqtt.toggle_topic_cmd[0] == '\0') {
        lv_label_set_text(s_status_label, "No topic configured");
    } else {
        lv_label_set_text(s_status_label, "Connected");
    }
}

static void on_switch_value_changed(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    mqtt_svc_publish_toggle(on);
}

static void poll_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (atomic_exchange(&s_external_dirty, false)) {
        apply_switch_state(atomic_load(&s_external_state));
    }
    refresh_status();
}

static void mqtt_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    app_settings_t *as = settings_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa  (parent, LV_OPA_COVER, LV_PART_MAIN);

    s_title_label = lv_label_create(parent);
    lv_label_set_text(s_title_label,
        as->mqtt.toggle_label[0] ? as->mqtt.toggle_label : "Switch");
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 24);

    s_switch = lv_switch_create(parent);
    lv_obj_set_size(s_switch, 100, 50);
    lv_obj_align(s_switch, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(s_switch, on_switch_value_changed, LV_EVENT_VALUE_CHANGED, NULL);

    s_status_label = lv_label_create(parent);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -24);

    apply_switch_state(atomic_load(&s_external_state));
    refresh_status();

    mqtt_svc_set_toggle_state_cb(on_mqtt_toggle_state);
    s_poll_timer = lv_timer_create(poll_timer_cb, 250, NULL);

    ESP_LOGI(TAG, "Created");
}

static void mqtt_destroy(void)
{
    if (s_poll_timer) { lv_timer_del(s_poll_timer); s_poll_timer = NULL; }
    mqtt_svc_set_toggle_state_cb(NULL);
    s_root = s_title_label = s_switch = s_status_label = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

static void mqtt_on_event(const ui_event_t *ev)
{
    if (ev->type == UI_EVT_STATE_CHANGED) refresh_status();
}

static void mqtt_on_input(ui_input_t input)
{
    switch (input) {
        case UI_INPUT_ENCODER_PRESS:
        case UI_INPUT_BTN_OK: {
            if (!s_switch) break;
            bool on = !lv_obj_has_state(s_switch, LV_STATE_CHECKED);
            apply_switch_state(on);
            mqtt_svc_publish_toggle(on);
            break;
        }
        case UI_INPUT_SWIPE_LEFT:
            settings_set_screen(SCREEN_CLOCK);
            break;
        default:
            break;
    }
}

static void mqtt_apply_theme(void)
{
    if (!s_root) return;
    const ui_theme_colors_t *th = theme_get();
    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    if (s_title_label)  lv_obj_set_style_text_color(s_title_label,  lv_color_hex(th->text_primary), LV_PART_MAIN);
    if (s_status_label) lv_obj_set_style_text_color(s_status_label, lv_color_hex(th->text_muted),   LV_PART_MAIN);
    lv_obj_invalidate(s_root);
}

const ui_screen_t screen_mqtt = {
    .create      = mqtt_create,
    .destroy     = mqtt_destroy,
    .apply_theme = mqtt_apply_theme,
    .on_event    = mqtt_on_event,
    .on_input    = mqtt_on_input,
    .name        = "mqtt",
};
