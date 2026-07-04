#include "calendar_widget.h"

#include "ui_profile.h"   // DISPLAY_WIDTH (per UI_PROFILE_*)
#include "events_service.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "CAL_WGT";
static lv_obj_t   *s_label = NULL;
static lv_timer_t *s_timer = NULL;

// Periodic refresh (30 s) — same rationale as event_indicator: no external
// clock-tick event is emitted, so the widget polls the events service itself.
#define CAL_WGT_REFRESH_MS (30 * 1000)

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    calendar_widget_update();
}

void calendar_widget_create(lv_obj_t *parent, int16_t x, int16_t y,
                            int16_t w, const lv_font_t *font)
{
    s_label = lv_label_create(parent);
    lv_label_set_long_mode(s_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_label, w > 0 ? w : DISPLAY_WIDTH);
    lv_obj_set_style_text_font(s_label,
        font ? font : &lv_font_montserrat_14_pl, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_label, x, y);

    calendar_widget_update();

    if (!s_timer) {
        s_timer = lv_timer_create(timer_cb, CAL_WGT_REFRESH_MS, NULL);
    }

    ESP_LOGI(TAG, "Created");
}

void calendar_widget_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_label = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

void calendar_widget_update(void)
{
    if (!s_label) return;

    event_t e;
    if (!events_calendar_current(&e)) {
        lv_obj_add_flag(s_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const ui_theme_colors_t *th = theme_get();
    lv_obj_clear_flag(s_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(s_label, lv_color_hex(th->text_primary), LV_PART_MAIN);

    char buf[96];
    snprintf(buf, sizeof(buf), "%02d:%02d  %s",
             e.hour, e.minute, e.title[0] ? e.title : "(no title)");
    lv_label_set_text(s_label, buf);
}

void calendar_widget_apply_theme(void)
{
    calendar_widget_update();
}
