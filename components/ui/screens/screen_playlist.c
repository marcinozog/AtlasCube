#include "screen_playlist.h"
#include "ui_events.h"
#include "ui_screen.h"
#include "ui_manager.h"
#include "ui_list_widget.h"
#include "playlist.h"
#include "radio_service.h"
#include "settings.h"
#include "app_state.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "SCR_PLAYLIST";

static lv_obj_t *s_root         = NULL;
static lv_obj_t *s_list         = NULL;   // ui_list_widget viewport
static lv_obj_t *s_header_label = NULL;
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
// List callbacks
// --------------------------------------------------------------------------

static void bind_row(int idx, ui_list_row_t *row)
{
    const playlist_entry_t *e = playlist_get(s_order[idx]);
    // '*' prefix marks favorites; constant width keeps numbers aligned.
    snprintf(row->text, sizeof(row->text), "%c%2d. %s",
             (e && e->favorite) ? '*' : ' ', idx + 1, e ? e->name : "");
    if (idx == s_playing)
        row->color = theme_color_or(ui_profile_get()->playlist_row_accent_color,
                                    theme_get()->accent);
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

    // ----- List -----
    // Return to the last selected station (curr_index from app_state lives in
    // real-index space — translate to display position).
    int curr_real = app_state_get()->curr_index;
    int curr_disp = (curr_real >= 0 && curr_real < s_count) ? real_to_display(curr_real) : -1;
    s_playing = curr_disp;

    const ui_list_cfg_t cfg = {
        .x = list_x, .y = list_y, .w = list_w, .h = list_h,
        .item_h       = p->playlist_item_h,
        .item_pad     = p->playlist_item_pad,
        .row_pad_left = p->playlist_row_pad_left,
        .row_bg_opa   = p->playlist_label_bg_opa,
        .row_bg_color      = p->playlist_row_bg_color,
        .row_text_color    = p->playlist_row_text_color,
        .cursor_bg_color   = p->playlist_cursor_bg_color,
        .cursor_text_color = p->playlist_cursor_text_color,
        .font         = p->playlist_row_font,
        .bind         = bind_row,
        .click        = play_display_index,
    };
    s_list = ui_list_create(parent, &cfg, s_count);
    ui_list_select(curr_disp >= 0 ? curr_disp : 0);

    ESP_LOGI(TAG, "Created, %d stations, selected=%d", s_count, ui_list_get_selected());
}

static void playlist_destroy(void)
{
    ui_list_forget();
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
            int next = ui_list_get_selected() + 1;
            if (next >= s_count) next = 0;
            ui_list_select(next);
            break;
        }

        case UI_INPUT_ENCODER_CCW: {
            int prev = ui_list_get_selected() - 1;
            if (prev < 0) prev = s_count - 1;
            ui_list_select(prev);
            break;
        }

        case UI_INPUT_ENCODER_PRESS:
            play_display_index(ui_list_get_selected());
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

    ui_list_refresh();
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
