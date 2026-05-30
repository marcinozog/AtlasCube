#include "screen_splash.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "ui_profile.h"
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

    // Auto-fit the logo to the panel: shrink (never enlarge) so the native
    // 220x240 art fits both dimensions. No-op on panels large enough; on the
    // 256x64 mono OLED it scales down to fit the height. Generic — no per-panel
    // branch, the formula is a no-op when the art already fits.
    int32_t sx = (int32_t)DISPLAY_WIDTH  * 256 / img_atlas_logo.header.w;
    int32_t sy = (int32_t)DISPLAY_HEIGHT * 256 / img_atlas_logo.header.h;
    int32_t scale = sx < sy ? sx : sy;            // 256 = 1:1
    if (scale < 256) {
        lv_image_set_pivot(img, img_atlas_logo.header.w / 2, img_atlas_logo.header.h / 2);
        lv_image_set_scale(img, scale);
    }
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
