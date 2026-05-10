#include "mode_indicator_widget.h"
#include "app_state.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"

#define BT_BRAND_COLOR 0x0082FC  // ten sam co w screen_bt.c

static const char *TAG = "MODE_IND";
static lv_obj_t *s_label = NULL;

void mode_indicator_create(lv_obj_t *parent, int x, int y)
{
    s_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_14_pl, LV_PART_MAIN);
    lv_obj_set_pos(s_label, x, y);
    mode_indicator_update();
    ESP_LOGI(TAG, "Created");
}

void mode_indicator_destroy(void)
{
    s_label = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

void mode_indicator_update(void)
{
    if (!s_label) return;
    const ui_theme_colors_t *th = theme_get();
    app_state_t *s = app_state_get();

    if (s->bt_enable) {
        lv_label_set_text(s_label, LV_SYMBOL_BLUETOOTH);
        lv_obj_set_style_text_color(s_label, lv_color_hex(th->bt_brand), LV_PART_MAIN);
    } else {
        lv_label_set_text(s_label, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_color(s_label, lv_color_hex(th->accent), LV_PART_MAIN);
    }
}

void mode_indicator_apply_theme(void)
{
    mode_indicator_update();
}