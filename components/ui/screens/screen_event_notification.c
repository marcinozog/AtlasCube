#include "screen_event_notification.h"
#include "ui_manager.h"
#include "theme.h"
#include "settings.h"
#include "sdcard.h"
#include "lv_bin_image.h"
#include "net_wallpaper.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "SCR_EVENT";

// "Pending" data — set by prepare() right before navigation,
// read in create().
static ui_event_info_t s_pending_info;
static ui_screen_id_t  s_return_to = SCREEN_RADIO;

static lv_obj_t *s_icon        = NULL;   // NULL when the event brought artwork,
                                         // or when the bell is switched off
static lv_obj_t *s_type_label  = NULL;
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_time_label  = NULL;
static lv_obj_t *s_hint_label  = NULL;

// The event's own artwork, resampled to its on-screen size. Owned here, freed
// in scr_destroy().
static lv_image_dsc_t *s_img_dsc = NULL;

void screen_event_notification_set_info(const ui_event_info_t *info)
{
    if (!info) return;
    s_pending_info = *info;
}

void screen_event_notification_set_return(ui_screen_id_t return_to)
{
    s_return_to = return_to;
}

// On-screen size of the artwork: shrink to fit the panel, never enlarge. A
// panel-sized photo therefore fills the screen and a small picture keeps its own
// pixels, sitting where the bell would have been. Resampling (rather than
// lv_image_set_scale) keeps LVGL off the transform path, which renders through a
// layer buffer and is slow for full-screen art.
static void fit_to_panel(int w, int h, int *out_w, int *out_h)
{
    const int pw = lv_display_get_horizontal_resolution(NULL);
    const int ph = lv_display_get_vertical_resolution(NULL);

    if (w <= 0 || h <= 0) { *out_w = *out_h = 1; return; }
    if (w <= pw && h <= ph) { *out_w = w; *out_h = h; return; }

    const int sx = pw * 1024 / w;
    const int sy = ph * 1024 / h;
    const int s  = sx < sy ? sx : sy;
    *out_w = w * s / 1024;
    *out_h = h * s / 1024;
    if (*out_w < 1) *out_w = 1;
    if (*out_h < 1) *out_h = 1;
}

// Resolve the event's `image` reference. Returns NULL for every failure — an
// empty reference, an absent card, a missing file, a slot nothing has been
// fetched into yet — and the caller falls back to the bell.
static lv_image_dsc_t *load_event_image(const char *ref)
{
    if (!ref || !ref[0]) return NULL;

    int w, h;

    // "net0".."net9": an internet wallpaper already decoded into PSRAM. Those
    // pixels belong to the background and are replaced under us by the next
    // fetch, so take a scaled copy rather than pointing at them.
    if (strncmp(ref, "net", 3) == 0 &&
        ref[3] >= '0' && ref[3] <= '9' && ref[4] == '\0') {
        const lv_image_dsc_t *slot = net_wallpaper_image(ref[3] - '0');
        if (!slot) {
            ESP_LOGW(TAG, "%s holds no image yet", ref);
            return NULL;
        }
        fit_to_panel(slot->header.w, slot->header.h, &w, &h);
        return lv_bin_image_scale_copy(slot, w, h);
    }

    // Otherwise an SD path, spelled relative to the card root like `sound`.
    if (sdcard_init() != ESP_OK || !sdcard_is_mounted()) {
        ESP_LOGW(TAG, "SD not available — falling back to the bell");
        return NULL;
    }
    char path[sizeof(s_pending_info.image) + sizeof(SD_MOUNT_POINT) + 1];
    snprintf(path, sizeof(path), "%s%s%s",
             SD_MOUNT_POINT, ref[0] == '/' ? "" : "/", ref);

    lv_image_dsc_t *img = lv_bin_image_load(path, 0, 0);   // 0,0 = any size
    if (!img) return NULL;

    fit_to_panel(img->header.w, img->header.h, &w, &h);
    return lv_bin_image_scale(img, w, h);   // consumes img; no-op when it fits
}

// Tap anywhere dismisses the notification. Labels aren't clickable and
// LV_EVENT_PRESSED doesn't bubble, so a transparent full-screen catcher on top
// is the simplest way to capture taps over the whole screen.
static void scr_touch_cb(lv_event_t *e)
{
    (void)e;
    ui_input_send(UI_INPUT_BTN_OK);
}

static void scr_create(lv_obj_t *parent)
{
    const ui_theme_colors_t *th = theme_get();

    // No background of our own: ui_manager re-applies the shared one (wallpaper,
    // gradient or solid) right after create(), so painting here would only be
    // overwritten — and the artwork below is meant to sit on that background.

    // The event's own artwork, if it has any and it loads. Built before the
    // labels so they draw on top of it.
    s_img_dsc = load_event_image(s_pending_info.image);
    if (s_img_dsc) {
        lv_obj_t *img = lv_image_create(parent);
        lv_image_set_src(img, s_img_dsc);
        lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 0);
    } else if (settings_get()->display.event_bell) {
        // Large BELL symbol — the fallback, and switched off entirely by the
        // setting so a wallpaper can show through untouched.
        s_icon = lv_label_create(parent);
        lv_label_set_text(s_icon, LV_SYMBOL_BELL);
        lv_obj_set_style_text_font(s_icon, &lv_font_montserrat_96, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_icon, lv_color_hex(th->accent), LV_PART_MAIN);
        lv_obj_align(s_icon, LV_ALIGN_TOP_MID, 0, 10);
    }

    // Type
    s_type_label = lv_label_create(parent);
    lv_label_set_text(s_type_label,
        s_pending_info.type_label[0] ? s_pending_info.type_label : "EVENT");
    lv_obj_set_style_text_font(s_type_label, &lv_font_montserrat_14_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_type_label, lv_color_hex(th->text_secondary), LV_PART_MAIN);
    lv_obj_align(s_type_label, LV_ALIGN_TOP_MID, 0, 118);

    // Title
    s_title_label = lv_label_create(parent);
    lv_label_set_text(s_title_label, s_pending_info.title);
    lv_label_set_long_mode(s_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_title_label, 300);
    lv_obj_set_style_text_align(s_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_18_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 140);

    // Time
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", s_pending_info.hour, s_pending_info.minute);
    s_time_label = lv_label_create(parent);
    lv_label_set_text(s_time_label, buf);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_14_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_align(s_time_label, LV_ALIGN_TOP_MID, 0, 172);

    // Hint
    s_hint_label = lv_label_create(parent);
    lv_label_set_text(s_hint_label, LV_SYMBOL_OK "  press encoder");
    lv_obj_set_style_text_font(s_hint_label, &lv_font_montserrat_12_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint_label, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_align(s_hint_label, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_obj_t *catcher = lv_obj_create(parent);
    lv_obj_remove_style_all(catcher);
    lv_obj_set_size(catcher, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(catcher, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(catcher, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(catcher, scr_touch_cb, LV_EVENT_PRESSED, NULL);

    ESP_LOGI(TAG, "Shown: [%s] %s (return_to=%d)",
             s_pending_info.id, s_pending_info.title, s_return_to);
}

static void scr_destroy(void)
{
    s_icon = s_type_label = s_title_label = s_time_label = s_hint_label = NULL;
    // The lv_image referencing this is already gone (lv_obj_clean); dropping the
    // descriptor here also drops LVGL's cache entry for it.
    lv_bin_image_free(s_img_dsc);
    s_img_dsc = NULL;
}

static void scr_apply_theme(void)
{
    // s_icon is optional now (artwork or a switched-off bell), so the labels —
    // always built — decide whether the screen is up.
    if (!s_title_label) return;
    const ui_theme_colors_t *th = theme_get();

    if (s_icon)
        lv_obj_set_style_text_color(s_icon,       lv_color_hex(th->accent),         LV_PART_MAIN);
    lv_obj_set_style_text_color(s_type_label,     lv_color_hex(th->text_secondary), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_title_label,    lv_color_hex(th->text_primary),   LV_PART_MAIN);
    lv_obj_set_style_text_color(s_time_label,     lv_color_hex(th->accent),         LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint_label,     lv_color_hex(th->text_muted),     LV_PART_MAIN);
}

static void scr_on_input(ui_input_t input)
{
    if (input == UI_INPUT_ENCODER_PRESS || input == UI_INPUT_BTN_OK) {
        // Dismiss → return to whatever screen was showing before the event.
        ui_navigate(s_return_to);
    }
}

const ui_screen_t screen_event_notification = {
    .create      = scr_create,
    .destroy     = scr_destroy,
    .apply_theme = scr_apply_theme,
    .on_event    = NULL,
    .on_input    = scr_on_input,
    .name        = "event_notification",
};
