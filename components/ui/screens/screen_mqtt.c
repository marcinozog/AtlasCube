#include "screen_mqtt.h"
#include "ui_screen.h"
#include "ui_manager.h"
#include "ui_events.h"
#include "theme.h"
#include "settings.h"
#include "mqtt_svc.h"
#include "mqtt_config.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "SCR_MQTT";

// Per-widget UI handles. We keep them in arrays indexed the same as
// mqtt_config_t::widgets, so the state callback (which receives a widget
// index) can update the right LVGL object.
static lv_obj_t  *s_root         = NULL;
static lv_obj_t  *s_status_label = NULL;
static lv_obj_t  *s_widget_main[MQTT_MAX_WIDGETS]  = {0};   // toggle: lv_switch; slider: lv_slider; label: value text
static lv_obj_t  *s_widget_title[MQTT_MAX_WIDGETS] = {0};
static lv_timer_t *s_poll_timer  = NULL;

// External state arriving from the esp-mqtt task — written under atomics,
// applied by an lv_timer on the LVGL task.
typedef struct {
    atomic_bool dirty;
    char        value[64];
} widget_pending_t;
static widget_pending_t s_pending[MQTT_MAX_WIDGETS];

// ─────────────────────────────────────────────────────────────────────────────
// MQTT → UI: called from esp-mqtt task; defer LVGL work to poll timer.
// ─────────────────────────────────────────────────────────────────────────────
static void on_mqtt_widget_state(int idx, const char *value)
{
    if (idx < 0 || idx >= MQTT_MAX_WIDGETS) return;
    size_t n = strlen(value);
    if (n >= sizeof(s_pending[idx].value)) n = sizeof(s_pending[idx].value) - 1;
    memcpy((void *)s_pending[idx].value, value, n);
    ((char *)s_pending[idx].value)[n] = '\0';
    atomic_store(&s_pending[idx].dirty, true);
}

static bool parse_bool_value(const char *v)
{
    if (!v || !*v) return false;
    if (!strcasecmp(v, "on")  || !strcasecmp(v, "true")  || v[0] == '1') return true;
    if (!strcasecmp(v, "off") || !strcasecmp(v, "false") || v[0] == '0') return false;
    return false;
}

static void apply_pending(int idx)
{
    mqtt_config_t *c = mqtt_config_get();
    mqtt_widget_t *w = &c->widgets[idx];
    lv_obj_t *obj = s_widget_main[idx];
    if (!obj) return;
    const char *v = (const char *)s_pending[idx].value;

    switch (w->type) {
        case MQTT_W_TOGGLE: {
            bool on = parse_bool_value(v);
            if (on) lv_obj_add_state(obj, LV_STATE_CHECKED);
            else    lv_obj_clear_state(obj, LV_STATE_CHECKED);
            break;
        }
        case MQTT_W_SLIDER: {
            int n = atoi(v);
            if (n < w->min) n = w->min;
            if (n > w->max) n = w->max;
            lv_slider_set_value(obj, n, LV_ANIM_OFF);
            break;
        }
        case MQTT_W_LABEL: {
            char buf[80];
            if (w->unit[0]) snprintf(buf, sizeof(buf), "%s %s", v, w->unit);
            else            snprintf(buf, sizeof(buf), "%s", v);
            lv_label_set_text(obj, buf);
            break;
        }
        default: break;
    }
}

static void refresh_status(void)
{
    if (!s_status_label) return;
    mqtt_config_t *c = mqtt_config_get();
    int active = 0;
    for (int i = 0; i < MQTT_MAX_WIDGETS; ++i)
        if (c->widgets[i].type != MQTT_W_NONE) active++;

    if (!c->enabled) {
        lv_label_set_text(s_status_label, "MQTT disabled");
    } else if (!mqtt_svc_is_connected()) {
        lv_label_set_text(s_status_label, "Connecting…");
    } else if (active == 0) {
        lv_label_set_text(s_status_label, "No widgets configured");
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "Connected · %d widget%s", active, active == 1 ? "" : "s");
        lv_label_set_text(s_status_label, buf);
    }
}

static void poll_timer_cb(lv_timer_t *t)
{
    (void)t;
    for (int i = 0; i < MQTT_MAX_WIDGETS; ++i) {
        if (atomic_exchange(&s_pending[i].dirty, false)) apply_pending(i);
    }
    refresh_status();
}

// ─────────────────────────────────────────────────────────────────────────────
// Widget input → MQTT
// ─────────────────────────────────────────────────────────────────────────────
static void on_toggle_changed(lv_event_t *e)
{
    int idx = (intptr_t)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target(e);
    mqtt_svc_publish_widget_bool(idx, lv_obj_has_state(sw, LV_STATE_CHECKED));
}

// Slider publishes only on release (RELEASED), not during drag — one packet
// per gesture, not 50.
static void on_slider_released(lv_event_t *e)
{
    int idx = (intptr_t)lv_event_get_user_data(e);
    lv_obj_t *sl = lv_event_get_target(e);
    mqtt_svc_publish_widget_int(idx, lv_slider_get_value(sl));
}

// ─────────────────────────────────────────────────────────────────────────────
// Build widgets — vertical stack, fixed slot heights
// ─────────────────────────────────────────────────────────────────────────────
static lv_obj_t *build_toggle(lv_obj_t *parent, int idx, const mqtt_widget_t *w)
{
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_set_size(sw, 60, 32);
    lv_obj_add_event_cb(sw, on_toggle_changed, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)idx);
    (void)w;
    return sw;
}

static lv_obj_t *build_slider(lv_obj_t *parent, int idx, const mqtt_widget_t *w)
{
    lv_obj_t *sl = lv_slider_create(parent);
    lv_obj_set_width(sl, 130);
    lv_slider_set_range(sl, w->min, w->max);
    lv_slider_set_value(sl, w->min, LV_ANIM_OFF);
    lv_obj_add_event_cb(sl, on_slider_released, LV_EVENT_RELEASED, (void *)(intptr_t)idx);
    return sl;
}

static lv_obj_t *build_label(lv_obj_t *parent, int idx, const mqtt_widget_t *w)
{
    (void)idx;
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, w->unit[0] ? w->unit : "—");
    const ui_theme_colors_t *th = theme_get();
    lv_obj_set_style_text_color(lbl, lv_color_hex(th->accent), LV_PART_MAIN);
    return lbl;
}

static void mqtt_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    mqtt_config_t *c = mqtt_config_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa  (parent, LV_OPA_COVER, LV_PART_MAIN);

    // Header title
    lv_obj_t *header = lv_label_create(parent);
    lv_label_set_text(header, "MQTT");
    lv_obj_set_style_text_color(header, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 6);

    // Container for widget rows — flex column. Auto height so it scrolls if
    // overflowing (LVGL default), but with 6 max compact rows it should fit.
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(80));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, 6, LV_PART_MAIN);

    for (int i = 0; i < MQTT_MAX_WIDGETS; ++i) {
        mqtt_widget_t *w = &c->widgets[i];
        if (w->type == MQTT_W_NONE) {
            s_widget_main[i] = NULL;
            s_widget_title[i] = NULL;
            continue;
        }

        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(row, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        lv_obj_set_style_bg_opa  (row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 6, LV_PART_MAIN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *title = lv_label_create(row);
        lv_label_set_text(title, w->title[0] ? w->title : "—");
        lv_obj_set_style_text_color(title, lv_color_hex(th->text_primary), LV_PART_MAIN);
        s_widget_title[i] = title;

        lv_obj_t *main = NULL;
        switch (w->type) {
            case MQTT_W_TOGGLE: main = build_toggle(row, i, w); break;
            case MQTT_W_SLIDER: main = build_slider(row, i, w); break;
            case MQTT_W_LABEL:  main = build_label (row, i, w); break;
            default: break;
        }
        s_widget_main[i] = main;

        // Force pending re-apply on screen rebuild (so a known retained value
        // is reflected on the new objects).
        if (atomic_load(&s_pending[i].dirty)) {
            // leave dirty; poll will pick it up
        } else if (s_pending[i].value[0]) {
            atomic_store(&s_pending[i].dirty, true);
        }
    }

    s_status_label = lv_label_create(parent);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    refresh_status();
    mqtt_svc_set_widget_state_cb(on_mqtt_widget_state);
    s_poll_timer = lv_timer_create(poll_timer_cb, 250, NULL);

    ESP_LOGI(TAG, "Created");
}

static void mqtt_destroy(void)
{
    if (s_poll_timer) { lv_timer_del(s_poll_timer); s_poll_timer = NULL; }
    mqtt_svc_set_widget_state_cb(NULL);
    for (int i = 0; i < MQTT_MAX_WIDGETS; ++i) {
        s_widget_main[i] = NULL;
        s_widget_title[i] = NULL;
    }
    s_root = s_status_label = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

static void mqtt_on_event(const ui_event_t *ev)
{
    if (ev->type == UI_EVT_STATE_CHANGED) refresh_status();
}

static void mqtt_on_input(ui_input_t input)
{
    switch (input) {
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
    if (s_status_label) lv_obj_set_style_text_color(s_status_label, lv_color_hex(th->text_muted), LV_PART_MAIN);
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
