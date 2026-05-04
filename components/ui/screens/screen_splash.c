#include "screen_splash.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "theme.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "SCR_SPLASH";

LV_IMAGE_DECLARE(img_atlas_logo);

static void splash_create(lv_obj_t *parent)
{
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, &img_atlas_logo);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    ESP_LOGI(TAG, "Created");
}

static void splash_destroy(void)
{
    ESP_LOGI(TAG, "Destroyed");
}

const ui_screen_t screen_splash = {
    .create      = splash_create,
    .destroy     = splash_destroy,
    .apply_theme = NULL,
    .on_event    = NULL,
    .on_input    = NULL,
    .name        = "splash",
};
