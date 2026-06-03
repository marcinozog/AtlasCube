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

    char time_buf[12];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", t.tm_hour, t.tm_min);
    lv_label_set_text(s_label_time, time_buf);
}

static void timer_cb(lv_timer_t *timer)
{
    (void)timer; 
    update_display(); 
}

void clock_widget_create(lv_obj_t *parent, int x, int y, const lv_font_t *font)
{
    const ui_theme_colors_t *th = theme_get();

    s_label_time = lv_label_create(parent);
    lv_label_set_text(s_label_time, "--:--");
    lv_obj_set_style_text_font(s_label_time,
        font ? font : &lv_font_montserrat_18_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_time,
        lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_set_pos(s_label_time, x, y);

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
}