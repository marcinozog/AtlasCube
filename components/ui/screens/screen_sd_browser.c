#include "screen_sd_browser.h"
#include "ui_events.h"
#include "ui_screen.h"
#include "ui_manager.h"
#include "sd_player.h"
#include "app_state.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// On-device SD file browser. Mirrors screen_playlist (reuses the playlist_*
// ui_profile fields): a header + a scrollable list of an "up" entry, subfolders
// and audio files. Tapping a folder descends into it; tapping a track plays it
// and returns to the SD player. Long press / back goes to the player.

static const char *TAG = "SCR_SD_BR";

#define SD_BR_NAME_MAX 128
#define SD_BR_DIR_MAX  192

static lv_obj_t *s_root         = NULL;
static lv_obj_t *s_list         = NULL;
static lv_obj_t *s_header_label = NULL;
static char      s_dir[SD_BR_DIR_MAX];   // current browse dir (persists between visits)
static int       s_selected     = 0;
static int       s_count        = 0;     // number of rows

// Snapshot of the folder content, copied out of the sd_player scan buffers right
// after scanning. Owning a copy keeps the list stable even if a background
// auto-advance re-scans the (shared) sd_player buffers while we're browsing.
typedef enum { ENT_UP, ENT_FOLDER, ENT_TRACK } ent_kind_t;
typedef struct { ent_kind_t kind; char name[SD_BR_NAME_MAX]; } sd_entry_t;
static sd_entry_t *s_entries = NULL;

static const char *basename_of(const char *path)
{
    if (!path || !path[0]) return "";
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

// Parent dir within the music tree ("" if already at the root → no "up" entry).
static void compute_parent(char *parent, size_t sz)
{
    const char *root = sd_player_root();
    parent[0] = 0;
    if (strcmp(s_dir, root) == 0) return;

    strncpy(parent, s_dir, sz - 1);
    parent[sz - 1] = 0;
    char *slash = strrchr(parent, '/');
    if (slash && slash != parent) *slash = 0;
    if (strlen(parent) < strlen(root)) {
        strncpy(parent, root, sz - 1);
        parent[sz - 1] = 0;
    }
}

// --------------------------------------------------------------------------
// Row helpers
// --------------------------------------------------------------------------

static lv_obj_t *get_row(int idx)
{
    if (!s_list || idx < 0 || idx >= s_count) return NULL;
    return lv_obj_get_child(s_list, idx);
}

static void style_row(int idx)
{
    lv_obj_t *row = get_row(idx);
    if (!row || !s_entries) return;
    lv_obj_t *lbl = lv_obj_get_child(row, 0);

    const ui_theme_colors_t *th = theme_get();
    bool is_cursor   = (idx == s_selected);
    bool is_folder   = (s_entries[idx].kind != ENT_TRACK);

    uint32_t bg = is_cursor ? th->accent : th->bg_secondary;
    uint32_t fg = is_cursor ? 0xFFFFFF
                : is_folder ? th->accent
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
// Content
// --------------------------------------------------------------------------

static void activate(int idx);

static void row_click_cb(lv_event_t *e)
{
    lv_obj_t *row = lv_event_get_target(e);
    if (row) activate(lv_obj_get_index(row));
}

// (Re)scan s_dir and rebuild the row list.
static void populate(void)
{
    if (s_list) lv_obj_clean(s_list);
    free(s_entries);
    s_entries = NULL;
    s_count   = 0;

    int nt = sd_player_scan(s_dir);          // tracks; folders via accessor
    int nf = sd_player_folder_count();

    char parent[SD_BR_DIR_MAX];
    compute_parent(parent, sizeof(parent));
    bool has_up = parent[0] != 0;

    int n = (has_up ? 1 : 0) + nf + nt;
    if (n > 0) {
        s_entries = heap_caps_malloc((size_t)n * sizeof(sd_entry_t), MALLOC_CAP_SPIRAM);
    }
    if (!s_entries) {
        ESP_LOGW(TAG, "No entries / alloc failed for %s", s_dir);
        if (s_header_label)
            lv_label_set_text(s_header_label, "SD: (empty)");
        return;
    }

    int k = 0;
    if (has_up) {
        s_entries[k].kind = ENT_UP;
        strncpy(s_entries[k].name, parent, SD_BR_NAME_MAX - 1);
        s_entries[k].name[SD_BR_NAME_MAX - 1] = 0;
        k++;
    }
    for (int i = 0; i < nf; i++) {
        const char *f = sd_player_folder(i);
        s_entries[k].kind = ENT_FOLDER;
        strncpy(s_entries[k].name, f ? f : "", SD_BR_NAME_MAX - 1);
        s_entries[k].name[SD_BR_NAME_MAX - 1] = 0;
        k++;
    }
    for (int i = 0; i < nt; i++) {
        const char *t = sd_player_track(i);
        s_entries[k].kind = ENT_TRACK;
        strncpy(s_entries[k].name, t ? t : "", SD_BR_NAME_MAX - 1);
        s_entries[k].name[SD_BR_NAME_MAX - 1] = 0;
        k++;
    }
    s_count = k;

    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    for (int i = 0; i < s_count; i++) {
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
        lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *lbl = lv_label_create(row);
        switch (s_entries[i].kind) {
            case ENT_UP:
                lv_label_set_text(lbl, LV_SYMBOL_UP "  ..");
                break;
            case ENT_FOLDER:
                lv_label_set_text_fmt(lbl, LV_SYMBOL_DIRECTORY " %s", s_entries[i].name);
                break;
            default:
                lv_label_set_text_fmt(lbl, LV_SYMBOL_AUDIO " %s", s_entries[i].name);
                break;
        }
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl, p->playlist_row_label_w);
        lv_obj_set_style_text_font(lbl, p->playlist_row_font, LV_PART_MAIN);
        // Initial colour (style_row only restyles the cursor/prev rows, so set it
        // here too or non-cursor labels would inherit an invisible default).
        uint32_t fg = (s_entries[i].kind == ENT_TRACK) ? th->text_primary : th->accent;
        lv_obj_set_style_text_color(lbl, lv_color_hex(fg), LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    }

    if (s_header_label)
        lv_label_set_text_fmt(s_header_label, "SD: %s",
                              basename_of(s_dir)[0] ? basename_of(s_dir) : "music");

    s_selected = 0;
    highlight_item(0);
    ESP_LOGI(TAG, "%s → %d folders, %d tracks", s_dir, nf, nt);
}

static void activate(int idx)
{
    if (idx < 0 || idx >= s_count || !s_entries) return;
    sd_entry_t *e = &s_entries[idx];

    switch (e->kind) {
        case ENT_UP:
            strncpy(s_dir, e->name, sizeof(s_dir) - 1);
            s_dir[sizeof(s_dir) - 1] = 0;
            populate();
            break;

        case ENT_FOLDER: {
            char path[SD_BR_DIR_MAX + SD_BR_NAME_MAX];
            snprintf(path, sizeof(path), "%s/%s", s_dir, e->name);
            strncpy(s_dir, path, sizeof(s_dir) - 1);
            s_dir[sizeof(s_dir) - 1] = 0;
            populate();
            break;
        }

        case ENT_TRACK: {
            char path[SD_BR_DIR_MAX + SD_BR_NAME_MAX];
            snprintf(path, sizeof(path), "%s/%s", s_dir, e->name);
            sd_player_play_path(path);
            ui_navigate(SCREEN_SD);
            break;
        }
    }
}

// --------------------------------------------------------------------------
// Create / Destroy
// --------------------------------------------------------------------------

static void sd_browser_create(lv_obj_t *parent)
{
    s_root = parent;

    if (!s_dir[0]) {
        strncpy(s_dir, sd_player_root(), sizeof(s_dir) - 1);
        s_dir[sizeof(s_dir) - 1] = 0;
    }

    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();
    const int16_t list_h = DISPLAY_HEIGHT - p->playlist_header_h;

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);

    // Header
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_size(header, DISPLAY_WIDTH, p->playlist_header_h);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    s_header_label = lv_label_create(header);
    lv_label_set_text(s_header_label, "SD");
    lv_obj_set_style_text_font(s_header_label, p->playlist_header_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_header_label, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_align(s_header_label, LV_ALIGN_LEFT_MID, p->playlist_label_x, p->playlist_label_y);

    lv_obj_t *hint = lv_label_create(header);
    lv_label_set_text(hint, "press - open   long - back");
    lv_obj_set_style_text_font(hint, p->playlist_row_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_RIGHT_MID, p->playlist_hint_x, p->playlist_hint_y);

    // Scrollable list
    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, DISPLAY_WIDTH, list_h);
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

    populate();
    ESP_LOGI(TAG, "Created at %s", s_dir);
}

static void sd_browser_destroy(void)
{
    free(s_entries);
    s_entries      = NULL;
    s_count        = 0;
    s_root         = NULL;
    s_list         = NULL;
    s_header_label = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

static void sd_browser_on_event(const ui_event_t *ev)
{
    (void)ev;
}

static void sd_browser_on_input(ui_input_t input)
{
    switch (input) {
        case UI_INPUT_ENCODER_CW: {
            if (s_count == 0) break;
            int next = s_selected + 1;
            if (next >= s_count) next = 0;
            highlight_item(next);
            break;
        }
        case UI_INPUT_ENCODER_CCW: {
            if (s_count == 0) break;
            int prev = s_selected - 1;
            if (prev < 0) prev = s_count - 1;
            highlight_item(prev);
            break;
        }
        case UI_INPUT_ENCODER_PRESS:
            activate(s_selected);
            break;
        case UI_INPUT_ENCODER_LONG_PRESS:
            ui_navigate(SCREEN_SD);
            break;
        default:
            break;
    }
}

static void sd_browser_apply_theme(void)
{
    if (!s_root || !s_list) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    if (s_header_label)
        lv_obj_set_style_text_color(s_header_label, lv_color_hex(th->accent), LV_PART_MAIN);

    for (int i = 0; i < s_count; i++) style_row(i);
    lv_obj_invalidate(s_root);
}

const ui_screen_t screen_sd_browser = {
    .create      = sd_browser_create,
    .destroy     = sd_browser_destroy,
    .apply_theme = sd_browser_apply_theme,
    .on_event    = sd_browser_on_event,
    .on_input    = sd_browser_on_input,
    .name        = "sd_browser",
};
