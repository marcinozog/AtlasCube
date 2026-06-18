#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_ALIGN_LEFT,    // x = left edge of the text (like plain lv_obj_set_pos)
    UI_ALIGN_CENTER,  // x = horizontal middle of the text
    UI_ALIGN_RIGHT,   // x = right edge of the text
} ui_label_align_t;

// Auto-width label whose horizontal anchor point (per `align`) stays nailed to
// (x, y) regardless of text width; `y` is the top edge.
//
// Use instead of `lv_label_create` + `lv_obj_set_pos` for labels whose text
// changes (clock digits, station/title, IP, ...). The chosen anchor — and thus
// the layout-editor coordinate — stays put as the string width varies, instead
// of the label wandering from its top-left corner.
//
// Returns the label — set its font, color and text as usual.
lv_obj_t *ui_anchored_label(lv_obj_t *parent, int x, int y, ui_label_align_t align);

#ifdef __cplusplus
}
#endif
