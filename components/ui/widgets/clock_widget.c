#include "clock_widget.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "ui_timefmt.h"
#include "ntp_service.h"
#include "esp_log.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "CLK_WIDGET";

static lv_obj_t   *s_label_time   = NULL;
static lv_timer_t *s_timer        = NULL;

static void update_display(void)
{
    if (!s_label_time) return;

    if (!ntp_service_is_synced()) {
        lv_label_set_text(s_label_time, "00:00");
        return;
    }

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    // Unlike the home screen's big digit-only fonts, this widget's fonts are
    // text-capable, so the AM/PM suffix can live in the same label.
    char time_buf[12], out[20];
    const char *suffix = ui_format_time(time_buf, sizeof(time_buf), &t);
    snprintf(out, sizeof(out), "%s%s%s", time_buf, suffix[0] ? " " : "", suffix);
    lv_label_set_text(s_label_time, out);
}

static void timer_cb(lv_timer_t *timer)
{
    (void)timer; 
    update_display(); 
}

void clock_widget_create(lv_obj_t *parent, int x, int y, const lv_font_t *font,
                         ui_label_align_t align)
{
    const ui_theme_colors_t *th = theme_get();

    s_label_time = ui_anchored_label(parent, x, y, align);
    lv_label_set_text(s_label_time, "--:--");
    lv_obj_set_style_text_font(s_label_time,
        font ? font : &lv_font_montserrat_18_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_time,
        lv_color_hex(th->text_primary), LV_PART_MAIN);
    ui_label_scrim(s_label_time);

    s_timer = lv_timer_create(timer_cb, 60 * 1000, NULL);
    update_display();

    ESP_LOGI(TAG, "Created (theme=%d)", theme_current());
}

void clock_widget_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_label_time = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

void clock_widget_tick(void)
{
    update_display();
}

void clock_widget_apply_theme(void)
{
    if (!s_label_time) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_text_color(s_label_time,
        lv_color_hex(th->text_primary), LV_PART_MAIN);
    ui_label_scrim(s_label_time);
}