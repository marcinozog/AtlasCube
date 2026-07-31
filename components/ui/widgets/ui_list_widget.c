#include "ui_list_widget.h"
#include "ui_profile.h"   // UI_LIST_BOX_PAD
#include "ui_manager.h"   // ui_input_send — swipes that started on a row
#include "theme.h"
#include "esp_log.h"

static const char *TAG = "UI_LIST";

// Row widgets held in reserve. The pool only has to cover one screenful, so this
// caps what any panel can ask for (a 4 px row on a 320 px panel would want 82).
#define ROW_POOL_MAX 32

static lv_obj_t *s_list    = NULL;   // scroll viewport (the box)
static lv_obj_t *s_content = NULL;   // full-height child that gives the scroll range
static lv_obj_t *s_rows[ROW_POOL_MAX];
static int       s_row_idx[ROW_POOL_MAX];   // entry each slot shows (-1 = none)
static uint32_t  s_row_fg[ROW_POOL_MAX];    // its bound text colour (cursor overrides)
static int       s_pool     = 0;
static int       s_pitch    = 1;     // item_h + item_pad
static int       s_view_h   = 1;     // scrollable height inside the box
static int       s_count    = 0;
static int       s_selected = 0;
static ui_list_cfg_t s_cfg;

// --------------------------------------------------------------------------
// Rows
// --------------------------------------------------------------------------

static void style_row(int k)
{
    const ui_theme_colors_t *th = theme_get();
    bool is_cursor = (s_row_idx[k] == s_selected);

    uint32_t bg = is_cursor ? theme_color_or(s_cfg.cursor_bg_color, th->accent)
                            : theme_color_or(s_cfg.row_bg_color,    th->bg_secondary);
    uint32_t fg = is_cursor ? theme_color_or(s_cfg.cursor_text_color, 0xFFFFFF)
                            : s_row_fg[k];

    lv_obj_set_style_bg_color(s_rows[k], lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_rows[k], lv_color_hex(fg), LV_PART_MAIN);
}

// Restyle the slot showing `idx`, if that entry is on screen right now.
static void restyle_index(int idx)
{
    for (int k = 0; k < s_pool; k++)
        if (s_row_idx[k] == idx) { style_row(k); return; }
}

// Point slot `k` at entry `idx`. Text and styles are only touched when the
// binding actually changes, so scrolling past one row costs one bind — not one
// per visible row.
static void bind_row(int k, int idx)
{
    if (s_row_idx[k] == idx) return;
    lv_obj_t *row = s_rows[k];

    if (idx < 0 || idx >= s_count) {
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        s_row_idx[k] = -1;
        return;
    }

    // Zero-initialised, so a callback that writes nothing yields an empty row
    // rather than stale bytes.
    ui_list_row_t data = {
        .color = theme_color_or(s_cfg.row_text_color, theme_get()->text_primary)
    };
    if (s_cfg.bind) s_cfg.bind(idx, &data);

    lv_label_set_text(row, data.text);
    lv_obj_set_y(row, idx * s_pitch);
    lv_obj_set_user_data(row, (void *)(intptr_t)idx);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
    s_row_idx[k] = idx;
    s_row_fg[k]  = data.color;
    style_row(k);
}

// Re-bind the pool to the window the current scroll position exposes. Slot
// `idx % s_pool` always owns entry `idx`, so scrolling past one row rebinds
// exactly one widget — the one that just left the box — instead of the pool.
static void refresh_window(void)
{
    if (!s_list || s_pool == 0) return;
    int first = lv_obj_get_scroll_y(s_list) / s_pitch;
    if (first > s_count - s_pool) first = s_count - s_pool;
    if (first < 0) first = 0;
    for (int idx = first; idx < first + s_pool; idx++) bind_row(idx % s_pool, idx);
}

static void list_scroll_cb(lv_event_t *e)
{
    (void)e;
    refresh_window();
}

// A sideways drag over a row finds nothing scrollable in that direction (the box
// scrolls vertically only), so LVGL keeps the press on the row and turns the
// release into a click — a swipe-to-exit that fell short of LVGL's gesture
// threshold would pick the entry instead of leaving the screen. Rows therefore
// only take a click that stayed put, and pass anything longer on as the swipe it
// was meant to be. The two paths can't both fire: when the gesture does trigger,
// ui_manager consumes the release (lv_indev_wait_release) and no click follows.
#define ROW_CLICK_SLOP 20   // px of sideways travel still counted as a tap

static int32_t s_press_x = 0;

static void row_press_cb(lv_event_t *e)
{
    (void)e;
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    s_press_x = p.x;
}

static void row_click_cb(lv_event_t *e)
{
    lv_obj_t *row = lv_event_get_target(e);
    if (!row || !s_cfg.click) return;

    lv_indev_t *indev = lv_indev_active();
    if (indev) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        int32_t dx = p.x - s_press_x;
        if (dx >  ROW_CLICK_SLOP) { ui_input_send(UI_INPUT_SWIPE_RIGHT); return; }
        if (dx < -ROW_CLICK_SLOP) { ui_input_send(UI_INPUT_SWIPE_LEFT);  return; }
    }

    s_cfg.click((int)(intptr_t)lv_obj_get_user_data(row));
}

// --------------------------------------------------------------------------
// Public
// --------------------------------------------------------------------------

// Size the content to the entry count and rebuild the pool binding. Split out
// because set_count has to redo exactly this after the data changed.
static void resize_content(void)
{
    lv_obj_set_size(s_content, lv_obj_get_width(s_content), s_count * s_pitch);
    for (int k = 0; k < s_pool; k++) s_row_idx[k] = -1;   // force a full re-bind
    lv_obj_update_layout(s_list);
}

lv_obj_t *ui_list_create(lv_obj_t *parent, const ui_list_cfg_t *cfg, int count)
{
    if (!parent || !cfg || cfg->w <= 0 || cfg->h <= 0) {
        ESP_LOGW(TAG, "bad config");
        return NULL;
    }

    s_cfg      = *cfg;
    s_count    = count > 0 ? count : 0;
    s_selected = 0;

    s_pitch = cfg->item_h + cfg->item_pad;
    if (s_pitch < 1) s_pitch = 1;

    // ----- Scroll viewport -----
    s_list = lv_obj_create(parent);
    lv_obj_set_pos(s_list, cfg->x, cfg->y);
    lv_obj_set_size(s_list, cfg->w, cfg->h);
    // Transparent: the screen's wallpaper (or gradient) is what shows between
    // and around the rows, and keeping the box unpainted is one less full-box
    // fill per scroll frame.
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_list, UI_LIST_BOX_PAD, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    // A scrollbar adds its own strip to every scroll frame, and elastic
    // overshoot repaints the box twice per flick for nothing.
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_event_cb(s_list, list_scroll_cb, LV_EVENT_SCROLL, NULL);

    s_view_h = cfg->h - 2 * UI_LIST_BOX_PAD;
    if (s_view_h < 1) s_view_h = 1;

    const int16_t row_w = (int16_t)(cfg->w - 2 * UI_LIST_BOX_PAD);

    // Stand-in for the rows that don't exist: it alone defines how far the
    // viewport can scroll.
    s_content = lv_obj_create(s_list);
    lv_obj_set_pos(s_content, 0, 0);
    lv_obj_set_size(s_content, row_w > 8 ? row_w : 8, s_count * s_pitch);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    // ----- Row pool -----
    const int fh      = cfg->font ? lv_font_get_line_height(cfg->font) : cfg->item_h;
    const int pad_top = (cfg->item_h - fh) / 2;
    const lv_opa_t row_opa = (lv_opa_t)(cfg->row_bg_opa * 255 / 100);

    s_pool = s_view_h / s_pitch + 2;
    if (s_pool > ROW_POOL_MAX) s_pool = ROW_POOL_MAX;

    for (int k = 0; k < s_pool; k++) {
        // One label per row, carrying its own plate: an lv_obj wrapper with a
        // child label would double the objects and the draw calls per frame.
        lv_obj_t *row = lv_label_create(s_content);
        lv_obj_set_size(row, row_w > 8 ? row_w : 8, cfg->item_h);
        lv_obj_set_style_bg_opa(row, row_opa, LV_PART_MAIN);
        // Square corners on purpose: a radius makes every row a masked draw, and
        // rows are the one thing repainted on every scroll frame.
        lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(row, cfg->row_pad_left, LV_PART_MAIN);
        lv_obj_set_style_pad_top(row, pad_top > 0 ? pad_top : 0, LV_PART_MAIN);
        lv_label_set_long_mode(row, LV_LABEL_LONG_CLIP);
        if (cfg->font) lv_obj_set_style_text_font(row, cfg->font, LV_PART_MAIN);
        if (cfg->click) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(row, row_press_cb, LV_EVENT_PRESSED, NULL);
            lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED, NULL);
        }
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);   // until bound
        s_rows[k]    = row;
        s_row_idx[k] = -1;
        s_row_fg[k]  = theme_color_or(cfg->row_text_color, theme_get()->text_primary);
    }

    // One layout pass so the viewport knows its scroll range before the first
    // ui_list_select(); after this the list never needs another one.
    lv_obj_update_layout(s_list);
    refresh_window();

    ESP_LOGI(TAG, "created: %d entries, %d row widgets", s_count, s_pool);
    return s_list;
}

void ui_list_set_count(int count)
{
    if (!s_list) return;
    s_count    = count > 0 ? count : 0;
    s_selected = 0;
    lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
    resize_content();
    refresh_window();
}

void ui_list_select(int idx)
{
    if (!s_list || s_count == 0) return;
    if (idx < 0) idx = 0;
    if (idx >= s_count) idx = s_count - 1;

    int prev = s_selected;
    s_selected = idx;

    // Centre the selection. The rows sit on a fixed pitch, so the target scroll
    // is arithmetic — no layout pass and no widget lookup needed.
    int max_scroll = s_count * s_pitch - s_view_h;
    if (max_scroll < 0) max_scroll = 0;
    int target = idx * s_pitch + s_pitch / 2 - s_view_h / 2;
    if (target < 0)          target = 0;
    if (target > max_scroll) target = max_scroll;
    if (target != lv_obj_get_scroll_y(s_list))
        lv_obj_scroll_to_y(s_list, target, LV_ANIM_OFF);   // fires the scroll cb

    refresh_window();
    restyle_index(prev);
    restyle_index(s_selected);
}

int ui_list_get_selected(void)
{
    return s_selected;
}

void ui_list_refresh(void)
{
    if (!s_list) return;
    for (int k = 0; k < s_pool; k++) s_row_idx[k] = -1;
    refresh_window();
}

void ui_list_forget(void)
{
    s_list    = NULL;
    s_content = NULL;
    s_pool    = 0;
    s_count   = 0;
}
