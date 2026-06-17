#include "screen_splash.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "ui_profile.h"
#include "theme.h"
#include "settings.h"
#include "wifi_manager.h"
#include "sdcard.h"
#include "lv_bin_image.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_app_desc.h"
#include "esp_log.h"

static const char *TAG = "SCR_SPLASH";

LV_IMAGE_DECLARE(img_atlas_logo);

// Optional custom logo loaded from SD (settings.display.logo_path). NULL = use
// the built-in img_atlas_logo. Owned here, freed in splash_destroy().
static lv_image_dsc_t *s_logo_dsc;

// Try to load the user's splash logo from SD. Lazily mounts the card; on a
// build without SD hardware sdcard_init() is a no-op error and we fall back to
// the built-in logo. Any size is accepted — splash_create() auto-fits it.
static const lv_image_dsc_t *resolve_logo(void)
{
    const char *path = settings_get()->display.logo_path;
    if (!path[0]) return &img_atlas_logo;
    if (sdcard_init() != ESP_OK || !sdcard_is_mounted()) {
        ESP_LOGW(TAG, "SD not available — using built-in logo");
        return &img_atlas_logo;
    }
    s_logo_dsc = lv_bin_image_load(path, 0, 0);   // 0,0 = any size
    return s_logo_dsc ? s_logo_dsc : &img_atlas_logo;
}

// Diagnostic boot-info overlay: firmware version + STA IP, drawn over the logo.
// Purely informational, toggled by settings.display.show_boot_info. The IP isn't
// known when the splash is built (WiFi comes up after the UI in app_main), so a
// timer refreshes the line until the STA lease lands. STA-only — hidden in AP.
static lv_obj_t  *s_info_label;
static lv_timer_t *s_info_timer;

static void info_refresh(lv_timer_t *t)
{
    (void)t;
    if (!s_info_label) return;

    // Version is always shown; the IP line is added once WiFi is in STA (in AP we
    // head to the WiFi-setup screen anyway). app_main holds the splash for a few
    // seconds after STA comes up, so the lease has resolved by the time it shows.
    if (wifi_get_run_mode() == WIFI_RUN_MODE_STA) {
        char ip[16];
        wifi_get_ip(ip, sizeof(ip));
        lv_label_set_text_fmt(s_info_label, "%s\nIP: %s",
                              esp_app_get_description()->version, ip);
    } else {
        lv_label_set_text_fmt(s_info_label, "%s",
                              esp_app_get_description()->version);
    }
}

static void splash_create(lv_obj_t *parent)
{
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    const lv_image_dsc_t *logo = resolve_logo();
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, logo);

    // Auto-fit the logo to the panel: shrink (never enlarge) so the art fits
    // both dimensions. No-op on panels large enough; on the 256x64 mono OLED it
    // scales down to fit the height. Generic — no per-panel branch, the formula
    // is a no-op when the art already fits. Works for any custom logo size too.
    int32_t sx = (int32_t)DISPLAY_WIDTH  * 256 / logo->header.w;
    int32_t sy = (int32_t)DISPLAY_HEIGHT * 256 / logo->header.h;
    int32_t scale = sx < sy ? sx : sy;            // 256 = 1:1
    if (scale < 256) {
        lv_image_set_pivot(img, logo->header.w / 2, logo->header.h / 2);
        lv_image_set_scale(img, scale);
    }
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    // Boot-info overlay — version + IP, bottom-centred over the logo. Painted
    // immediately (version always known, IP refined by the timer once the STA
    // lease lands) so it never depends on the timer firing. White text on a
    // translucent dark pill keeps it readable over the logo art and on any
    // theme/panel. Overlap with the logo is accepted (diagnostic, disableable).
    if (settings_get()->display.show_boot_info) {
        s_info_label = lv_label_create(parent);
        lv_obj_set_style_text_font(s_info_label, &lv_font_montserrat_14_pl, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_info_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_align(s_info_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_info_label, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_info_label, LV_OPA_70, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(s_info_label, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(s_info_label, 3, LV_PART_MAIN);
        lv_obj_set_style_radius(s_info_label, 4, LV_PART_MAIN);
        lv_obj_align(s_info_label, LV_ALIGN_BOTTOM_MID, 0, -36);

        info_refresh(NULL);                                  // paint immediately
        s_info_timer = lv_timer_create(info_refresh, 500, NULL);
    }

    ESP_LOGI(TAG, "Created");
}

static void splash_destroy(void)
{
    if (s_info_timer) {
        lv_timer_del(s_info_timer);
        s_info_timer = NULL;
    }
    s_info_label = NULL;   // child freed by lv_obj_clean() on navigation
    if (s_logo_dsc) { lv_bin_image_free(s_logo_dsc); s_logo_dsc = NULL; }
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
