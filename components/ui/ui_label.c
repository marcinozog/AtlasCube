#include "ui_label.h"
#include "theme.h"
#include "ui_profile.h"   // UI_PROFILE_MONO_* selection (via defines.h)
#include <string.h>

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t align;
} anchor_t;

// Re-run on every size change (label auto-sizes when its text changes): shift
// the top-left so the chosen anchor point lands back on (x, y). Position-only
// change, so it does not re-trigger a size event.
static void on_size_changed(lv_event_t *e)
{
    lv_obj_t   *label = lv_event_get_target(e);
    anchor_t   *a     = lv_event_get_user_data(e);
    lv_coord_t  w     = lv_obj_get_width(label);

    int x = a->x;
    if      (a->align == UI_ALIGN_CENTER) x -= w / 2;
    else if (a->align == UI_ALIGN_RIGHT)  x -= w;

    lv_obj_set_pos(label, x, a->y);
}

static void on_delete(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}

lv_obj_t *ui_anchored_label(lv_obj_t *parent, int x, int y, ui_label_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);

    anchor_t *a = lv_malloc(sizeof(*a));
    a->x     = x;
    a->y     = y;
    a->align = align;

    lv_obj_add_event_cb(label, on_size_changed, LV_EVENT_SIZE_CHANGED, a);
    lv_obj_add_event_cb(label, on_delete,       LV_EVENT_DELETE,       a);

    lv_obj_set_pos(label, x, y);  // initial; refined on first text/size change
    return label;
}

void ui_label_scrim(lv_obj_t *obj, int opa_pct)
{
#if defined(UI_PROFILE_MONO_128X64) || defined(UI_PROFILE_MONO_256X64)
    (void)obj;
    (void)opa_pct;   // 1-bit panels have no wallpaper; a plate would just be a box
#else
    if (!obj || opa_pct <= 0) return;
    if (opa_pct > 100) opa_pct = 100;

    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_get()->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, (opa_pct * 255) / 100, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(obj, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(obj, 1, LV_PART_MAIN);
#endif
}

void ui_label_set_text(lv_obj_t *lbl, const char *txt)
{
    if (!lbl) return;
    txt = txt ? txt : "";

    // Skip an unchanged string: lv_label_set_text() restarts the scroll
    // animation, so a label refreshed on every state change would otherwise
    // stay stuck at the first frame of its scroll.
    if (strcmp(lv_label_get_text(lbl), txt) != 0) lv_label_set_text(lbl, txt);

    if (txt[0]) lv_obj_clear_flag(lbl, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
}

void ui_label_set_text_boxed(lv_obj_t *lbl, const char *txt, int box_w)
{
    if (!lbl) return;
    txt = txt ? txt : "";
    if (box_w < 8) box_w = 8;

    if (strcmp(lv_label_get_text(lbl), txt) != 0) {
        // Size the label to its text, capped at the box width — the anchored
        // label re-centers itself on every width change, so the box centre stays
        // put. The scrim plate's horizontal padding counts into the object width,
        // so add it on top: otherwise the content area ends up narrower than the
        // text and SCROLL_CIRCULAR kicks in even for text that fits the box.
        lv_point_t size;
        const lv_font_t *font = lv_obj_get_style_text_font(lbl, LV_PART_MAIN);
        lv_text_get_size(&size, txt, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_coord_t pad = lv_obj_get_style_pad_left(lbl, LV_PART_MAIN)
                       + lv_obj_get_style_pad_right(lbl, LV_PART_MAIN);
        lv_obj_set_width(lbl, LV_CLAMP(1, size.x + pad, box_w + pad));
    }
    ui_label_set_text(lbl, txt);
}
