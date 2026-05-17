#include "vol_overlay_widget.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include <stdio.h>

#define VOL_OVERLAY_TIMEOUT_MS 1500
#define VOL_OVERLAY_W_LARGE    200
#define VOL_OVERLAY_H_LARGE    100
#define VOL_OVERLAY_W_SMALL    80
#define VOL_OVERLAY_H_SMALL    36

static lv_obj_t   *s_overlay = NULL;
static lv_obj_t   *s_label   = NULL;
static lv_timer_t *s_timer   = NULL;
static bool        s_large   = false;

static void hide_cb(lv_timer_t *t)
{
    (void)t;
    vol_overlay_hide();
}

void vol_overlay_hide(void)
{
    if (s_overlay) {
        lv_obj_del(s_overlay);
        s_overlay = NULL;
        s_label   = NULL;
    }
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
}

void vol_overlay_show(lv_obj_t *parent, int vol, bool large)
{
    if (!parent) return;
    const ui_theme_colors_t *th = theme_get();

    // Parent changed (screen swap) or size variant changed — rebuild.
    if (s_overlay && (lv_obj_get_parent(s_overlay) != parent || s_large != large)) {
        vol_overlay_hide();
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", vol);

    if (!s_overlay) {
        s_large = large;
        s_overlay = lv_obj_create(parent);
        lv_obj_remove_style_all(s_overlay);
        lv_obj_set_style_bg_color(s_overlay, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_overlay, lv_color_hex(th->accent), LV_PART_MAIN);
        lv_obj_set_style_border_width(s_overlay, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(s_overlay, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(s_overlay, 12, LV_PART_MAIN);
        lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

        if (large) lv_obj_set_size(s_overlay, VOL_OVERLAY_W_LARGE, VOL_OVERLAY_H_LARGE);
        else       lv_obj_set_size(s_overlay, VOL_OVERLAY_W_SMALL, VOL_OVERLAY_H_SMALL);

        s_label = lv_label_create(s_overlay);
        lv_obj_set_style_text_font(s_label,
            large ? &lv_font_montserrat_72 : &lv_font_montserrat_12_pl,
            LV_PART_MAIN);
        lv_obj_set_style_text_color(s_label, lv_color_hex(th->accent), LV_PART_MAIN);
    }

    lv_label_set_text(s_label, buf);
    lv_obj_center(s_label);
    lv_obj_center(s_overlay);
    lv_obj_move_foreground(s_overlay);

    if (s_timer) {
        lv_timer_reset(s_timer);
    } else {
        s_timer = lv_timer_create(hide_cb, VOL_OVERLAY_TIMEOUT_MS, NULL);
        lv_timer_set_repeat_count(s_timer, 1);
    }
}
