#pragma once

#include "lvgl.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Virtualised vertical list, shared by the two list screens (station playlist,
// SD file browser).
//
// Only as many row widgets as fit the box exist (plus one above and one below);
// they are re-bound to entries as the list scrolls, so the widget count and the
// per-frame draw cost depend on the panel, not on how many entries there are.
// That matters twice over: a 512-entry playlist would otherwise put ~1000
// objects in LVGL's 64 KB PSRAM pool, and every scroll frame repaints the whole
// box anyway — the fewer widgets it has to walk and blend there, the smoother
// the scroll over a wallpaper.
//
// Singleton: the two screens that use it are never on screen at the same time.
// The rows carry their own plate, so the box itself stays transparent and the
// screen's wallpaper shows between them.

#define UI_LIST_TEXT_MAX 192

// What to show for one entry — filled by the owner in its bind callback.
typedef struct {
    char     text[UI_LIST_TEXT_MAX];
    uint32_t color;   // RGB text colour; preset to the configured row text colour
} ui_list_row_t;

// Called for an entry that just scrolled into the window. Runs on the LVGL task.
typedef void (*ui_list_bind_cb_t)(int idx, ui_list_row_t *row);
typedef void (*ui_list_click_cb_t)(int idx);

typedef struct {
    int16_t          x, y, w, h;    // list box, absolute LCD px
    int16_t          item_h;
    int16_t          item_pad;      // gap between rows
    int16_t          row_pad_left;  // text indent inside a row
    int16_t          row_bg_opa;    // row plate opacity, 0..100
    // Colours, 0 = follow the theme. Resolved on every restyle, so a theme
    // change still moves the parts that were left unset.
    uint32_t         row_bg_color;      // 0 = theme bg_secondary
    uint32_t         row_text_color;    // 0 = theme text_primary
    uint32_t         cursor_bg_color;   // 0 = theme accent
    uint32_t         cursor_text_color; // 0 = white
    const lv_font_t *font;
    ui_list_bind_cb_t  bind;
    ui_list_click_cb_t click;       // NULL = rows are not tappable
} ui_list_cfg_t;

// Build the list under `parent`. `count` may be 0 (filled in later via
// ui_list_set_count). Returns the scroll viewport, or NULL on bad config.
lv_obj_t *ui_list_create(lv_obj_t *parent, const ui_list_cfg_t *cfg, int count);

// Replace the entry count (e.g. the browser descending into a folder): resets
// the scroll to the top, re-binds every row and selects the first entry.
void ui_list_set_count(int count);

// Move the cursor to `idx` and centre it in the box.
void ui_list_select(int idx);
int  ui_list_get_selected(void);

// Re-read every visible row through the bind callback — after a theme change or
// when the underlying data changed without changing the count.
void ui_list_refresh(void);

// Forget the widget. The objects themselves are owned by the screen and already
// gone (lv_obj_clean) by the time a screen's destroy() runs.
void ui_list_forget(void);

#ifdef __cplusplus
}
#endif
