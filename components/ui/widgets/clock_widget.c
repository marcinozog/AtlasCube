#include "clock_widget.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "ntp_service.h"
#include "esp_log.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "CLK_WIDGET";

static lv_obj_t   *s_label_time   = NULL;
static lv_obj_t   *s_label_date   = NULL;
static lv_obj_t   *s_label_nosync = NULL;
static lv_timer_t *s_timer        = NULL;

static void update_display(void)
{
    if (!s_label_time) return;

    if (!ntp_service_is_synced()) {
        lv_label_set_text(s_label_time, "00:00");
        if (s_label_date)   lv_label_set_text(s_label_date, "");
        if (s_label_nosync) lv_label_set_text(s_label_nosync, "Syncing...");
        return;
    }

    if (s_label_nosync) lv_label_set_text(s_label_nosync, "");

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    char time_buf[12];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", t.tm_hour, t.tm_min);
    lv_label_set_text(s_label_time, time_buf);

    if (s_label_date) {
        char date_buf[32];
        static const char *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        snprintf(date_buf, sizeof(date_buf), "%s  %04d-%02d-%02d",
                 days[t.tm_wday], t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        lv_label_set_text(s_label_date, date_buf);
    }
}

static void timer_cb(lv_timer_t *timer)
{
    (void)timer; 
    update_display(); 
}

void clock_widget_create(lv_obj_t *parent, int x, int y, bool large)
{
    const ui_theme_colors_t *th = theme_get();

    s_label_time = lv_label_create(parent);
    lv_label_set_text(s_label_time, "--:--");
    lv_obj_set_style_text_font(s_label_time,
        large ? &lv_font_montserrat_96 : &lv_font_montserrat_18_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_time,
        lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_label_time, LV_ALIGN_TOP_MID, x, y);

    if (large) {
        s_label_date = lv_label_create(parent);
        lv_label_set_text(s_label_date, "");
        lv_obj_set_style_text_font(s_label_date, &lv_font_montserrat_18_pl, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_label_date,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
        lv_obj_align(s_label_date, LV_ALIGN_CENTER, 0, 50);

        s_label_nosync = lv_label_create(parent);
        lv_label_set_text(s_label_nosync, "");
        lv_obj_set_style_text_font(s_label_nosync, &lv_font_montserrat_12_pl, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_label_nosync,
            lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_align(s_label_nosync, LV_ALIGN_CENTER, 0, 55);
    } else {
        s_label_date   = NULL;
        s_label_nosync = NULL;
    }

    s_timer = lv_timer_create(timer_cb, 60 * 1000, NULL);
    update_display();

    ESP_LOGI(TAG, "Created (large=%d, theme=%d)", large, theme_current());
}

void clock_widget_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_label_time   = NULL;
    s_label_date   = NULL;
    s_label_nosync = NULL;
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

    if (s_label_date)
        lv_obj_set_style_text_color(s_label_date,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);

    if (s_label_nosync)
        lv_obj_set_style_text_color(s_label_nosync,
            lv_color_hex(th->text_muted), LV_PART_MAIN);
}