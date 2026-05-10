#include "defines.h"
#include "now_playing_widget.h"
#include "app_state.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NPW";

static lv_obj_t *s_label_station = NULL;
static lv_obj_t *s_label_title   = NULL;

void now_playing_widget_create(lv_obj_t *parent, int x, int y)
{
    const ui_theme_colors_t *th = theme_get();

    s_label_station = lv_label_create(parent);
    lv_label_set_long_mode(s_label_station, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_label_station, DISPLAY_WIDTH - 20);
    lv_obj_set_style_text_font(s_label_station, &lv_font_montserrat_18_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_station,
        lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_align(s_label_station, LV_ALIGN_TOP_MID, x, y);

    s_label_title = lv_label_create(parent);
    lv_label_set_long_mode(s_label_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_label_title, DISPLAY_WIDTH - 20);
    lv_obj_set_style_text_font(s_label_title, &lv_font_montserrat_14_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_title,
        lv_color_hex(th->text_secondary), LV_PART_MAIN);
    lv_obj_align(s_label_title, LV_ALIGN_TOP_MID, x, y + 26);

    now_playing_widget_update();
    ESP_LOGI(TAG, "Created");
}

void now_playing_widget_destroy(void)
{
    s_label_station = NULL;
    s_label_title   = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

void now_playing_widget_update(void)
{
    if (!s_label_station) return;
    app_state_t *s = app_state_get();
    lv_label_set_text(s_label_station,
        s->station_name[0] ? s->station_name : "Atlas Radio");
    lv_label_set_text(s_label_title,
        s->title[0] ? s->title : "");
}

void now_playing_widget_apply_theme(void)
{
    if (!s_label_station) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_text_color(s_label_station,
        lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_title,
        lv_color_hex(th->text_secondary), LV_PART_MAIN);
}