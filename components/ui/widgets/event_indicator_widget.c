#include "event_indicator_widget.h"

#include "events_service.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "EV_IND";
static lv_obj_t   *s_label = NULL;
static lv_timer_t *s_timer = NULL;

// Periodic refresh (30 s) — catches event add/fire without relying on
// an external UI_EVT_CLOCK_TICK (which isn't emitted).
#define EV_IND_REFRESH_MS (30 * 1000)

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    event_indicator_update();
}

void event_indicator_create(lv_obj_t *parent, lv_align_t align, int x, int y)
{
    s_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_14_pl, LV_PART_MAIN);
    lv_obj_align(s_label, align, x, y);
    event_indicator_update();

    if (!s_timer) {
        s_timer = lv_timer_create(timer_cb, EV_IND_REFRESH_MS, NULL);
    }

    ESP_LOGI(TAG, "Created");
}

void event_indicator_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_label = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

void event_indicator_update(void)
{
    if (!s_label) return;

    int n = events_pending_today_count();

    if (n <= 0) {
        lv_obj_add_flag(s_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const ui_theme_colors_t *th = theme_get();
    lv_obj_clear_flag(s_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(s_label, lv_color_hex(th->accent), LV_PART_MAIN);

    if (n == 1) {
        lv_label_set_text(s_label, LV_SYMBOL_BELL);
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), LV_SYMBOL_BELL " %d", n);
        lv_label_set_text(s_label, buf);
    }
}

void event_indicator_apply_theme(void)
{
    event_indicator_update();
}
