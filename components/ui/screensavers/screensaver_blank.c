#include "screensaver_blank.h"
#include "ui_screen.h"
#include "ui_profile.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "SS_BLANK";

static lv_obj_t *s_root = NULL;

static void blank_create(lv_obj_t *parent)
{
    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    ESP_LOGI(TAG, "Created");
}

static void blank_destroy(void)
{
    if (s_root) { lv_obj_delete(s_root); s_root = NULL; }
    ESP_LOGI(TAG, "Destroyed");
}

const ui_screen_t screensaver_blank = {
    .create      = blank_create,
    .destroy     = blank_destroy,
    .apply_theme = NULL,
    .on_event    = NULL,
    .on_input    = NULL,
    .name        = "ss_blank",
};
