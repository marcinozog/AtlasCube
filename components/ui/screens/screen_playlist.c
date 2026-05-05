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

static lv_obj_t *s_root     = NULL;
static lv_obj_t *s_list     = NULL;    // scrollable container
static lv_obj_t *s_header_label = NULL;
static int       s_selected = 0;       // kursor enkodera (display index)
static int       s_playing  = -1;      // aktualnie odtwarzana stacja (display index)
static int       s_count    = 0;
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
// Helpers
// --------------------------------------------------------------------------

static lv_obj_t *get_row(int idx)
{
    if (!s_list || idx < 0 || idx >= s_count) return NULL;
    return lv_obj_get_child(s_list, idx);
}

static void style_row(int idx)
{
    lv_obj_t *row = get_row(idx);
    if (!row) return;
    lv_obj_t *lbl = lv_obj_get_child(row, 0);

    const ui_theme_colors_t *th = theme_get();
    bool is_cursor  = (idx == s_selected);
    bool is_playing = (idx == s_playing);

    uint32_t bg = is_cursor ? th->accent : th->bg_secondary;
    uint32_t fg = is_cursor  ? 0xFFFFFF
                : is_playing ? th->accent
                             : th->text_primary;

    lv_obj_set_style_bg_color(row, lv_color_hex(bg), LV_PART_MAIN);
    if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(fg), LV_PART_MAIN);
}

static void highlight_item(int idx)
{
    int prev = s_selected;
    s_selected = idx;

    style_row(prev);
    style_row(s_selected);

    // Center the selected item in the list view
    lv_obj_t *cur = get_row(s_selected);
    if (cur) {
        lv_obj_update_layout(s_list);
        lv_coord_t row_y      = lv_obj_get_y(cur);
        lv_coord_t row_h      = lv_obj_get_height(cur);
        lv_coord_t view_h     = lv_obj_get_height(s_list);
        lv_coord_t scroll_y   = lv_obj_get_scroll_y(s_list);
        lv_coord_t max_scroll = scroll_y + lv_obj_get_scroll_bottom(s_list);
        lv_coord_t target     = row_y + row_h / 2 - view_h / 2;
        if (target < 0)          target = 0;
        if (target > max_scroll) target = max_scroll;
        lv_obj_scroll_to_y(s_list, target, LV_ANIM_ON);
    }
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
    const int16_t list_h = p->screen_h - p->playlist_header_h;

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);

    // ----- Header -----
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_size(header, p->screen_w, p->playlist_header_h);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    s_header_label = lv_label_create(header);
    lv_label_set_text(s_header_label, "Playlist");
    lv_obj_set_style_text_font(s_header_label, p->playlist_header_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_header_label, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_align(s_header_label, LV_ALIGN_LEFT_MID, p->playlist_row_pad_left, 0);

    lv_obj_t *hint = lv_label_create(header);
    lv_label_set_text(hint, "press - play   long press - exit");
    lv_obj_set_style_text_font(hint, p->playlist_row_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_RIGHT_MID, -p->playlist_row_pad_left, 0);

    // ----- Scrollable list -----
    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, p->screen_w, list_h);
    lv_obj_align(s_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_list, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_list, p->playlist_item_pad, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_ACTIVE);

    // ----- Items -----
    for (int i = 0; i < s_count; i++) {
        const playlist_entry_t *e = playlist_get(s_order[i]);
        if (!e) continue;

        lv_obj_t *row = lv_obj_create(s_list);
        lv_obj_set_size(row, p->playlist_row_w, p->playlist_item_h);
        lv_obj_set_style_bg_color(row, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 3, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(row, p->playlist_row_pad_left, LV_PART_MAIN);
        lv_obj_set_style_pad_right(row, 0, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(row);
        // '*' prefix marks favorites; constant width keeps numbers aligned.
        lv_label_set_text_fmt(lbl, "%c%2d. %s",
                              e->favorite ? '*' : ' ', i + 1, e->name);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl, p->playlist_row_label_w);
        lv_obj_set_style_text_font(lbl, p->playlist_row_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(th->text_primary), LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    }

    // Return to the last selected station (curr_index from app_state lives in
    // real-index space — translate to display position).
    int curr_real = app_state_get()->curr_index;
    int curr_disp = (curr_real >= 0 && curr_real < s_count) ? real_to_display(curr_real) : -1;
    s_playing  = curr_disp;
    s_selected = (curr_disp >= 0) ? curr_disp : 0;
    highlight_item(s_selected);

    ESP_LOGI(TAG, "Created, %d stations, selected=%d", s_count, s_selected);
}

static void playlist_destroy(void)
{
    s_root         = NULL;
    s_list         = NULL;
    s_header_label = NULL;
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
            app_state_t *s = app_state_get();
            // Play the selected station and return to radio
            if (s_selected >= 0 && s_selected < s_count) {
                int real_idx = s_order[s_selected];
                if (real_idx != s->curr_index || s->radio_state != RADIO_STATE_PLAYING) {
                    ESP_LOGI(TAG, "Play display=%d real=%d", s_selected, real_idx);
                    radio_play_index(real_idx);
                }
            }

            if(s->bt_enable)
                settings_set_bt_enable(false);

            ui_navigate(SCREEN_RADIO);
            break;

        case UI_INPUT_ENCODER_LONG_PRESS:
            // Exit without changing the station
            ui_navigate(SCREEN_RADIO);
            break;

        default:
            break;
    }
}

static void playlist_apply_theme(void)
{
    if (!s_root || !s_list) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    if (s_header_label)
        lv_obj_set_style_text_color(s_header_label, lv_color_hex(th->accent), LV_PART_MAIN);

    int n = lv_obj_get_child_count(s_list);
    for (int i = 0; i < n; i++) {
        lv_obj_t *row = lv_obj_get_child(s_list, i);
        lv_obj_t *lbl = lv_obj_get_child(row, 0);
        if (i == s_selected) {
            lv_obj_set_style_bg_color(row, lv_color_hex(th->accent), LV_PART_MAIN);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(row, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(th->text_primary), LV_PART_MAIN);
        }
    }

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