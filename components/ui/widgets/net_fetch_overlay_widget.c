#include "net_fetch_overlay_widget.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"

#define FAIL_HIDE_MS 3000

static lv_obj_t   *s_pill  = NULL;
static lv_obj_t   *s_label = NULL;
static lv_timer_t *s_timer = NULL;   // one-shot for the fail message; exists only while it lingers

static void hide(void)
{
    if (s_pill) {
        lv_obj_del(s_pill);
        s_pill  = NULL;
        s_label = NULL;
    }
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
}

static void hide_cb(lv_timer_t *t)
{
    (void)t;
    hide();
}

void net_fetch_overlay_show(void)
{
    const ui_theme_colors_t *th = theme_get();

    // A new fetch while the previous fail message still lingers — drop its timer.
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }

    if (!s_pill) {
        s_pill = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(s_pill);
        lv_obj_set_style_bg_color(s_pill, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        // Solid, not translucent: an alpha overlay would force a per-frame
        // reblend of everything under it (the VU lesson).
        lv_obj_set_style_bg_opa(s_pill, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_pill, lv_color_hex(th->accent), LV_PART_MAIN);
        lv_obj_set_style_border_width(s_pill, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(s_pill, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(s_pill, 16, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(s_pill, 10, LV_PART_MAIN);
        lv_obj_set_size(s_pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_clear_flag(s_pill, LV_OBJ_FLAG_SCROLLABLE);

        s_label = lv_label_create(s_pill);
        lv_obj_set_style_text_font(s_label, &lv_font_montserrat_18_pl, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_label, lv_color_hex(th->accent), LV_PART_MAIN);
        // Narrow panels (240 px): wrap instead of overflowing the screen.
        lv_obj_set_style_max_width(s_label, LV_HOR_RES - 56, LV_PART_MAIN);
        lv_obj_set_style_text_align(s_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    lv_label_set_text(s_label, "Updating wallpaper...");
    lv_obj_center(s_pill);
    lv_obj_move_foreground(s_pill);
}

void net_fetch_overlay_done(bool ok)
{
    if (!s_pill) return;
    if (ok) {
        hide();
        return;
    }
    lv_label_set_text(s_label, "Wallpaper update failed");
    lv_obj_center(s_pill);   // re-center for the longer text
    s_timer = lv_timer_create(hide_cb, FAIL_HIDE_MS, NULL);
    lv_timer_set_repeat_count(s_timer, 1);
}
