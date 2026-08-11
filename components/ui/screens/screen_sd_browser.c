#include "screen_sd_browser.h"
#include "ui_events.h"
#include "ui_screen.h"
#include "ui_manager.h"
#include "ui_list_widget.h"
#include "ui_label.h"
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

// On-device SD file browser. Mirrors screen_playlist (same ui_list_widget, its
// own browser_* ui_profile section): a header + a scrollable list of an "up"
// entry, subfolders and audio files. Tapping a folder descends into it; tapping
// a track plays it and returns to the SD player. Long press / back goes to the
// player.

static const char *TAG = "SCR_SD_BR";

#define SD_BR_NAME_MAX 128
#define SD_BR_DIR_MAX  192

static lv_obj_t *s_root         = NULL;
static lv_obj_t *s_list         = NULL;   // ui_list_widget viewport
static lv_obj_t *s_header_label = NULL;
static int       s_header_w     = 0;     // space left for the header text beside the hint
static char      s_dir[SD_BR_DIR_MAX];   // current browse dir (persists between visits)
static int       s_count        = 0;     // number of rows
static ui_screen_id_t s_return  = SCREEN_SD;   // where to go after pick/exit

void screen_sd_browser_set_return(ui_screen_id_t scr) { s_return = scr; }

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
// List callbacks
// --------------------------------------------------------------------------

static void bind_row(int idx, ui_list_row_t *row)
{
    if (!s_entries) return;
    switch (s_entries[idx].kind) {
        case ENT_UP:
            snprintf(row->text, sizeof(row->text), LV_SYMBOL_UP "  ..");
            break;
        case ENT_FOLDER:
            snprintf(row->text, sizeof(row->text), LV_SYMBOL_DIRECTORY " %s",
                     s_entries[idx].name);
            break;
        default:
            snprintf(row->text, sizeof(row->text), LV_SYMBOL_AUDIO " %s",
                     s_entries[idx].name);
            break;
    }
    // Navigation stands out from playable files.
    if (s_entries[idx].kind != ENT_TRACK)
        row->color = theme_color_or(ui_profile_get()->browser_row_accent_color,
                                    theme_get()->accent);
}

// --------------------------------------------------------------------------
// Content
// --------------------------------------------------------------------------

static void activate(int idx);

// (Re)scan s_dir and hand the new entry count to the list.
static void populate(void)
{
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
        ui_list_set_count(0);
        ui_label_set_text_boxed(s_header_label, "SD: (empty)", s_header_w);
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

    // Resets the scroll to the top and re-binds every row from s_entries.
    ui_list_set_count(s_count);

    char header[SD_BR_DIR_MAX + 8];
    snprintf(header, sizeof(header), "SD: %s",
             basename_of(s_dir)[0] ? basename_of(s_dir) : "music");
    ui_label_set_text_boxed(s_header_label, header, s_header_w);

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
            size_t len = strlen(s_dir);
            snprintf(s_dir + len, sizeof(s_dir) - len, "/%s", e->name);
            populate();
            break;
        }

        case ENT_TRACK: {
            char path[SD_BR_DIR_MAX + SD_BR_NAME_MAX];
            snprintf(path, sizeof(path), "%s/%s", s_dir, e->name);
            sd_player_play_path(path);
            ui_navigate(s_return);
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

    int16_t list_x, list_y, list_w, list_h;
    ui_profile_browser_list_box(p, &list_x, &list_y, &list_w, &list_h);

    // Background (gradient / global wallpaper / browser_wallpaper) comes from
    // ui_manager — only the header strip and the rows paint over it.
    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);

    // Header
    if (!p->browser_header_hide) {
        lv_obj_t *header = lv_obj_create(parent);
        lv_obj_set_size(header, DISPLAY_WIDTH, p->browser_header_h);
        lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
        lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        // The hint comes first so its measured width can be subtracted from the
        // space left for the folder name.
        int hint_w = 0;
        if (!p->browser_hint_hide) {
            lv_obj_t *hint = lv_label_create(header);
            lv_label_set_text(hint, "press - open   swipe<>/long - back");
            lv_obj_set_style_text_font(hint, p->browser_row_font, LV_PART_MAIN);
            lv_obj_set_style_text_color(hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
            lv_obj_align(hint, LV_ALIGN_RIGHT_MID, p->browser_hint_x, p->browser_hint_y);
            lv_obj_update_layout(hint);
            hint_w = lv_obj_get_width(hint) - p->browser_hint_x;   // hint_x is a left shift
        }

        s_header_label = lv_label_create(header);
        lv_label_set_text(s_header_label, "SD");
        lv_label_set_long_mode(s_header_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        // A long folder name scrolls in the space left of the hint instead of
        // wrapping into a second (clipped) line running under it.
        s_header_w = DISPLAY_WIDTH - p->browser_label_x - hint_w - 8;
        if (s_header_w < 24) s_header_w = 24;
        lv_obj_set_style_text_font(s_header_label, p->browser_header_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_header_label, lv_color_hex(th->accent), LV_PART_MAIN);
        lv_obj_align(s_header_label, LV_ALIGN_LEFT_MID, p->browser_label_x, p->browser_label_y);
    }

    // List — populate() fills in the count once the folder has been scanned.
    const ui_list_cfg_t cfg = {
        .x = list_x, .y = list_y, .w = list_w, .h = list_h,
        .item_h       = p->browser_item_h,
        .item_pad     = p->browser_item_pad,
        .row_pad_left = p->browser_row_pad_left,
        .row_bg_opa   = p->browser_label_bg_opa,
        .row_bg_color      = p->browser_row_bg_color,
        .row_text_color    = p->browser_row_text_color,
        .cursor_bg_color   = p->browser_cursor_bg_color,
        .cursor_text_color = p->browser_cursor_text_color,
        .font         = p->browser_row_font,
        .bind         = bind_row,
        .click        = activate,
    };
    s_list = ui_list_create(parent, &cfg, 0);

    populate();
    ESP_LOGI(TAG, "Created at %s", s_dir);
}

static void sd_browser_destroy(void)
{
    ui_list_forget();
    free(s_entries);
    s_entries      = NULL;
    s_count        = 0;
    s_root         = NULL;
    s_list         = NULL;
    s_header_label = NULL;
    s_header_w     = 0;
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
            int next = ui_list_get_selected() + 1;
            if (next >= s_count) next = 0;
            ui_list_select(next);
            break;
        }
        case UI_INPUT_ENCODER_CCW: {
            if (s_count == 0) break;
            int prev = ui_list_get_selected() - 1;
            if (prev < 0) prev = s_count - 1;
            ui_list_select(prev);
            break;
        }
        case UI_INPUT_ENCODER_PRESS:
            activate(ui_list_get_selected());
            break;
        case UI_INPUT_ENCODER_LONG_PRESS:
        case UI_INPUT_SWIPE_RIGHT:
        case UI_INPUT_SWIPE_LEFT:
            ui_navigate(s_return);
            break;
        default:
            break;
    }
}

static void sd_browser_apply_theme(void)
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

const ui_screen_t screen_sd_browser = {
    .create      = sd_browser_create,
    .destroy     = sd_browser_destroy,
    .apply_theme = sd_browser_apply_theme,
    .on_event    = sd_browser_on_event,
    .on_input    = sd_browser_on_input,
    .name        = "sd_browser",
};
