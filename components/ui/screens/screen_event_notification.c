#include "screen_event_notification.h"
#include "ui_manager.h"
#include "theme.h"
#include "settings.h"
#include "sdcard.h"
#include "lv_bin_image.h"
#include "net_wallpaper.h"
#include "ui_background.h"
#include "events_service.h"   // event_text_pos_t values behind ui_event_info_t.text_pos
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

// Is there a picture behind the text — the event's own artwork, or the shared
// background this screen inherits? Mirrors ui_background_apply()'s order for a
// screen with no per-screen override: the internet slot first, then the global
// SD wallpaper. A gradient or solid theme colour doesn't count; text reads fine
// on those and a card over them would be noise.
static bool background_has_image(void)
{
    if (s_img_dsc) return true;
    if (net_wallpaper_image(ui_background_net_slot_for(SCREEN_EVENT_NOTIFICATION)))
        return true;

    const app_settings_t *st = settings_get();
    return st->display.wallpaper_on && st->display.wallpaper_path[0] != '\0';
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

    // The event's own artwork, if it has any and it loads. Built first so it is
    // the backdrop everything else draws over.
    s_img_dsc = load_event_image(s_pending_info.image);
    if (s_img_dsc) {
        lv_obj_t *img = lv_image_create(parent);
        lv_image_set_src(img, s_img_dsc);
        lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 0);
    }

    const int pos = s_pending_info.text_pos;

    // Bell and text live in one column so they move together and can never
    // overlap, whichever placement the event asks for. The column hugs its
    // content, so its scrim (below) is only as big as what it holds.
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, LV_PCT(88));
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(col, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_row(col, 4, LV_PART_MAIN);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    // Over a picture — the event's own or the shared wallpaper — plain text on
    // an arbitrary photo is a coin flip, so put it on a dark translucent card.
    // On a flat theme background there is nothing to fight with, so no card.
    if (background_has_image()) {
        lv_obj_set_style_bg_color(col, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(col, LV_OPA_60, LV_PART_MAIN);
        lv_obj_set_style_radius(col, 10, LV_PART_MAIN);
    }

    // Large BELL symbol — shown when the event brought no artwork, and switched
    // off entirely by the setting so a wallpaper can show through untouched.
    if (!s_img_dsc && settings_get()->display.event_bell) {
        s_icon = lv_label_create(col);
        lv_label_set_text(s_icon, LV_SYMBOL_BELL);
        lv_obj_set_style_text_font(s_icon, &lv_font_montserrat_96, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_icon, lv_color_hex(th->accent), LV_PART_MAIN);
    }

    if (pos != EV_TEXT_NONE) {
        // Type
        s_type_label = lv_label_create(col);
        lv_label_set_text(s_type_label,
            s_pending_info.type_label[0] ? s_pending_info.type_label : "EVENT");
        lv_obj_set_style_text_font(s_type_label, &lv_font_montserrat_14_pl, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_type_label, lv_color_hex(th->text_secondary), LV_PART_MAIN);

        // Title — width follows the column (and so the panel), not a fixed 300 px
        // that overflowed anything narrower.
        s_title_label = lv_label_create(col);
        lv_label_set_text(s_title_label, s_pending_info.title);
        lv_label_set_long_mode(s_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(s_title_label, LV_PCT(100));
        lv_obj_set_style_text_align(s_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_18_pl, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_title_label, lv_color_hex(th->text_primary), LV_PART_MAIN);

        // Time
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", s_pending_info.hour, s_pending_info.minute);
        s_time_label = lv_label_create(col);
        lv_label_set_text(s_time_label, buf);
        lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_14_pl, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_time_label, lv_color_hex(th->accent), LV_PART_MAIN);
    }

    // An empty column (no text, no bell) would still paint its card.
    if (lv_obj_get_child_count(col) == 0) {
        lv_obj_del(col);
    } else if (pos == EV_TEXT_TOP) {
        lv_obj_align(col, LV_ALIGN_TOP_MID, 0, 8);
    } else if (pos == EV_TEXT_BOTTOM) {
        lv_obj_align(col, LV_ALIGN_BOTTOM_MID, 0, -30);   // clear of the hint
    } else {
        lv_obj_align(col, LV_ALIGN_CENTER, 0, 0);
    }

    // Hint — dropped along with the text, so "no text" really means a clean
    // picture. Tapping or pressing still dismisses.
    if (pos != EV_TEXT_NONE) {
        s_hint_label = lv_label_create(parent);
        lv_label_set_text(s_hint_label, LV_SYMBOL_OK "  press encoder");
        lv_obj_set_style_text_font(s_hint_label, &lv_font_montserrat_12_pl, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_hint_label, lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_align(s_hint_label, LV_ALIGN_BOTTOM_MID, 0, -8);
    }

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
    // Every part is optional now — artwork replaces the bell, the setting can
    // remove it, and text_pos "none" removes the labels — so each is checked.
    const ui_theme_colors_t *th = theme_get();

    if (s_icon)
        lv_obj_set_style_text_color(s_icon,       lv_color_hex(th->accent),         LV_PART_MAIN);
    if (s_type_label)
        lv_obj_set_style_text_color(s_type_label, lv_color_hex(th->text_secondary), LV_PART_MAIN);
    if (s_title_label)
        lv_obj_set_style_text_color(s_title_label, lv_color_hex(th->text_primary),  LV_PART_MAIN);
    if (s_time_label)
        lv_obj_set_style_text_color(s_time_label, lv_color_hex(th->accent),         LV_PART_MAIN);
    if (s_hint_label)
        lv_obj_set_style_text_color(s_hint_label, lv_color_hex(th->text_muted),     LV_PART_MAIN);
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
