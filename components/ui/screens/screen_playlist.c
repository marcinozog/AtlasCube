#include "screen_playlist.h"
#include "ui_events.h"
#include "ui_screen.h"
#include "ui_manager.h"
#include "playlist.h"
#include "radio_service.h"
#include "settings.h"
#include "app_state.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "SCR_PLAYLIST";

// The list is virtual: only as many row widgets as fit the viewport (plus one
// above and one below) exist, and they are re-bound to entries as the list
// scrolls. With PLAYLIST_MAX_ENTRIES at 512 the old one-widget-per-station list
// meant ~1000 LVGL objects in the 64 KB LVGL pool (PSRAM) and a full-viewport
// repaint that had to walk all of them — this keeps both bounded by the panel.
// Each row is a single lv_label carrying its own plate (background + padding);
// splitting it into an lv_obj + child label doubled the objects and draw calls
// for no gain.
#define ROW_POOL_MAX 32

static lv_obj_t *s_root         = NULL;
static lv_obj_t *s_list         = NULL;   // scroll viewport (the list box)
static lv_obj_t *s_content      = NULL;   // full-height child that gives the scroll range
static lv_obj_t *s_header_label = NULL;
static lv_obj_t *s_rows[ROW_POOL_MAX];
static int       s_row_idx[ROW_POOL_MAX]; // display index each slot shows (-1 = none)
static int       s_pool         = 0;      // slots in use
static int       s_pitch        = 1;      // item_h + item_pad
static int       s_view_h       = 1;      // scrollable height of the list box
static int       s_selected     = 0;      // kursor enkodera (display index)
static int       s_playing      = -1;     // aktualnie odtwarzana stacja (display index)
static int       s_count        = 0;
static ui_screen_id_t s_return  = SCREEN_RADIO;   // where to go after pick/exit

void screen_playlist_set_return(ui_screen_id_t scr) { s_return = scr; }
// Maps display position → real playlist index. Favorites first, original order
// preserved within each group. Real indices stay stable so app_state.curr_index
// and radio_play_index() can keep operating on the underlying playlist.
static int       s_order[PLAYLIST_MAX_ENTRIES];

static int real_to_display(int real_idx)
{
    for (int i = 0; i < s_count; i++) if (s_order[i] == real_idx) return i;
    return -1;
}

static void build_order(void)
{
    int n = 0;
    for (int i = 0; i < s_count; i++) {
        const playlist_entry_t *e = playlist_get(i);
        if (e && e->favorite) s_order[n++] = i;
    }
    for (int i = 0; i < s_count; i++) {
        const playlist_entry_t *e = playlist_get(i);
        if (e && !e->favorite) s_order[n++] = i;
    }
}

// --------------------------------------------------------------------------
// Row pool
// --------------------------------------------------------------------------

static void style_row(lv_obj_t *row, int idx)
{
    const ui_theme_colors_t *th = theme_get();
    bool is_cursor  = (idx == s_selected);
    bool is_playing = (idx == s_playing);

    uint32_t bg = is_cursor ? th->accent : th->bg_secondary;
    uint32_t fg = is_cursor  ? 0xFFFFFF
                : is_playing ? th->accent
                             : th->text_primary;

    lv_obj_set_style_bg_color(row, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_text_color(row, lv_color_hex(fg), LV_PART_MAIN);
}

// Restyle the slot showing `idx`, if that entry is currently on screen.
static void restyle_index(int idx)
{
    for (int k = 0; k < s_pool; k++)
        if (s_row_idx[k] == idx) { style_row(s_rows[k], idx); return; }
}

// Point slot `k` at entry `idx`. Styles and text are only touched when the
// binding actually changes, so a scroll frame costs one set_text per row that
// scrolled past the edge — not one per visible row.
static void bind_row(int k, int idx)
{
    if (s_row_idx[k] == idx) return;
    lv_obj_t *row = s_rows[k];

    if (idx < 0 || idx >= s_count) {
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        s_row_idx[k] = -1;
        return;
    }

    const playlist_entry_t *e = playlist_get(s_order[idx]);
    // '*' prefix marks favorites; constant width keeps numbers aligned.
    lv_label_set_text_fmt(row, "%c%2d. %s",
                          (e && e->favorite) ? '*' : ' ', idx + 1, e ? e->name : "");
    lv_obj_set_y(row, idx * s_pitch);
    lv_obj_set_user_data(row, (void *)(intptr_t)idx);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
    s_row_idx[k] = idx;
    style_row(row, idx);
}

// Re-bind the pool to the window the current scroll position exposes. Slot
// `idx % s_pool` always owns entry `idx`, so scrolling past one row rebinds
// exactly one widget — the one that just left the viewport — instead of the
// whole pool.
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

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

static void highlight_item(int idx)
{
    int prev = s_selected;
    s_selected = idx;

    // Center the selection. The rows sit on a fixed pitch, so the target scroll
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

static void play_display_index(int disp_idx)
{
    if (disp_idx < 0 || disp_idx >= s_count) return;

    app_state_t *s = app_state_get();
    int real_idx = s_order[disp_idx];
    if (real_idx != s->curr_index || s->radio_state != RADIO_STATE_PLAYING) {
        ESP_LOGI(TAG, "Play display=%d real=%d", disp_idx, real_idx);
        radio_play_index(real_idx);
    }

    if (s->bt_enable)
        settings_set_bt_enable(false);

    ui_navigate(s_return);
}

static void row_click_cb(lv_event_t *e)
{
    lv_obj_t *row = lv_event_get_target(e);
    if (!row) return;
    play_display_index((int)(intptr_t)lv_obj_get_user_data(row));
}

// --------------------------------------------------------------------------
// Create / Destroy
// --------------------------------------------------------------------------

static void playlist_create(lv_obj_t *parent)
{
    s_root  = parent;
    s_count = playlist_get_count();
    build_order();

    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    int16_t list_x, list_y, list_w, list_h;
    ui_profile_playlist_list_box(p, &list_x, &list_y, &list_w, &list_h);
    // The row fills the box; its own left padding indents the text, and
    // LV_LABEL_LONG_CLIP cuts a long station name at the right edge.
    const int16_t row_w = ui_profile_playlist_row_w(p);

    // The screen background (gradient / global wallpaper / playlist_wallpaper)
    // is applied to lv_scr_act() by ui_manager, so nothing here paints over it —
    // only the header strip and the rows themselves are opaque.
    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);

    // ----- Header -----
    if (!p->playlist_header_hide) {
        lv_obj_t *header = lv_obj_create(parent);
        lv_obj_set_size(header, DISPLAY_WIDTH, p->playlist_header_h);
        lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
        lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        s_header_label = lv_label_create(header);
        lv_label_set_text(s_header_label, "Playlist");
        lv_obj_set_style_text_font(s_header_label, p->playlist_header_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_header_label, lv_color_hex(th->accent), LV_PART_MAIN);
        lv_obj_align(s_header_label, LV_ALIGN_LEFT_MID, p->playlist_label_x, p->playlist_label_y);

        if (!p->playlist_hint_hide) {
            lv_obj_t *hint = lv_label_create(header);
            lv_label_set_text(hint, "press - play   swipe<>/long - exit");
            lv_obj_set_style_text_font(hint, p->playlist_row_font, LV_PART_MAIN);
            lv_obj_set_style_text_color(hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
            lv_obj_align(hint, LV_ALIGN_RIGHT_MID, p->playlist_hint_x, p->playlist_hint_y);
        }
    }

    // ----- Scroll viewport -----
    s_list = lv_obj_create(parent);
    lv_obj_set_pos(s_list, list_x, list_y);
    lv_obj_set_size(s_list, list_w, list_h);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_list, UI_PLAYLIST_LIST_PAD, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    // A scrollbar would add its own strip to every scroll frame, and elastic
    // overshoot repaints the viewport twice per flick for nothing.
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_event_cb(s_list, list_scroll_cb, LV_EVENT_SCROLL, NULL);

    s_pitch  = p->playlist_item_h + p->playlist_item_pad;
    if (s_pitch < 1) s_pitch = 1;
    s_view_h = list_h - 2 * UI_PLAYLIST_LIST_PAD;
    if (s_view_h < 1) s_view_h = 1;

    // Full-height stand-in for the rows that don't exist: it alone defines how
    // far the viewport can scroll.
    s_content = lv_obj_create(s_list);
    lv_obj_set_pos(s_content, 0, 0);
    lv_obj_set_size(s_content, row_w, s_count * s_pitch);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    // ----- Row pool -----
    const int fh      = lv_font_get_line_height(p->playlist_row_font);
    const int pad_top = (p->playlist_item_h - fh) / 2;
    const lv_opa_t row_opa = (lv_opa_t)(p->playlist_label_bg_opa * 255 / 100);

    s_pool = s_view_h / s_pitch + 2;
    if (s_pool > ROW_POOL_MAX) s_pool = ROW_POOL_MAX;
    if (s_pool > s_count)      s_pool = s_count;

    for (int k = 0; k < s_pool; k++) {
        lv_obj_t *row = lv_label_create(s_content);
        lv_obj_set_size(row, row_w, p->playlist_item_h);
        lv_obj_set_x(row, 0);
        lv_obj_set_style_bg_opa(row, row_opa, LV_PART_MAIN);
        // Square corners on purpose: a radius makes every row a masked draw, and
        // rows are the one thing repainted on every scroll frame.
        lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(row, p->playlist_row_pad_left, LV_PART_MAIN);
        lv_obj_set_style_pad_top(row, pad_top > 0 ? pad_top : 0, LV_PART_MAIN);
        lv_label_set_long_mode(row, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_font(row, p->playlist_row_font, LV_PART_MAIN);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED, NULL);
        s_rows[k]    = row;
        s_row_idx[k] = -1;
    }

    // One layout pass so the viewport knows its scroll range before the first
    // scroll_to_y below; after this the list never needs another one.
    lv_obj_update_layout(s_list);

    // Return to the last selected station (curr_index from app_state lives in
    // real-index space — translate to display position).
    int curr_real = app_state_get()->curr_index;
    int curr_disp = (curr_real >= 0 && curr_real < s_count) ? real_to_display(curr_real) : -1;
    s_playing  = curr_disp;
    s_selected = (curr_disp >= 0) ? curr_disp : 0;
    highlight_item(s_selected);

    ESP_LOGI(TAG, "Created, %d stations, %d row widgets, selected=%d",
             s_count, s_pool, s_selected);
}

static void playlist_destroy(void)
{
    s_root         = NULL;
    s_list         = NULL;
    s_content      = NULL;
    s_header_label = NULL;
    s_pool         = 0;
    ESP_LOGI(TAG, "Destroyed");
}

// --------------------------------------------------------------------------
// Events / Input
// --------------------------------------------------------------------------

static void playlist_on_event(const ui_event_t *ev)
{
    (void)ev;  // screen does not need to react to app_state at this point
}

static void playlist_on_input(ui_input_t input)
{
    switch (input) {

        case UI_INPUT_ENCODER_CW: {
            int next = s_selected + 1;
            if (next >= s_count) next = 0;
            highlight_item(next);
            break;
        }

        case UI_INPUT_ENCODER_CCW: {
            int prev = s_selected - 1;
            if (prev < 0) prev = s_count - 1;
            highlight_item(prev);
            break;
        }

        case UI_INPUT_ENCODER_PRESS:
            play_display_index(s_selected);
            break;

        case UI_INPUT_ENCODER_LONG_PRESS:
        case UI_INPUT_SWIPE_RIGHT:
        case UI_INPUT_SWIPE_LEFT:
            // Exit without changing the station
            ui_navigate(s_return);
            break;

        default:
            break;
    }
}

static void playlist_apply_theme(void)
{
    if (!s_root || !s_list) return;
    const ui_theme_colors_t *th = theme_get();

    if (s_header_label) {
        lv_obj_set_style_text_color(s_header_label, lv_color_hex(th->accent), LV_PART_MAIN);
        lv_obj_t *header = lv_obj_get_parent(s_header_label);
        if (header) lv_obj_set_style_bg_color(header, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
    }

    for (int k = 0; k < s_pool; k++)
        if (s_row_idx[k] >= 0) style_row(s_rows[k], s_row_idx[k]);

    lv_obj_invalidate(s_root);
}

// --------------------------------------------------------------------------

const ui_screen_t screen_playlist = {
    .create      = playlist_create,
    .destroy     = playlist_destroy,
    .apply_theme = playlist_apply_theme,
    .on_event    = playlist_on_event,
    .on_input    = playlist_on_input,
    .name        = "playlist",
};
