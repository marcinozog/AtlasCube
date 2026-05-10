#include "screen_events.h"
#include "events_service.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "lvgl.h"
#include "esp_attr.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "SCR_EVENTS";

static lv_obj_t *s_root         = NULL;
static lv_obj_t *s_list         = NULL;
static lv_obj_t *s_header_label = NULL;
static lv_obj_t *s_empty_label  = NULL;

// ~5KB events copy for rendering. Goes to PSRAM, accessed only from LVGL task.
EXT_RAM_BSS_ATTR static event_t s_items[EVENTS_MAX];
static int     s_count    = 0;
static int     s_selected = 0;

static lv_obj_t *get_row(int idx)
{
    if (!s_list || idx < 0 || idx >= s_count) return NULL;
    return lv_obj_get_child(s_list, idx);
}

static void highlight_item(int idx)
{
    if (s_count == 0) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_t *prev = get_row(s_selected);
    if (prev) {
        lv_obj_set_style_bg_color(prev, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        lv_obj_t *title = lv_obj_get_child(prev, 0);
        lv_obj_t *meta  = lv_obj_get_child(prev, 1);
        if (title) lv_obj_set_style_text_color(title, lv_color_hex(th->text_primary),   LV_PART_MAIN);
        if (meta)  lv_obj_set_style_text_color(meta,  lv_color_hex(th->text_secondary), LV_PART_MAIN);
    }

    s_selected = idx;

    lv_obj_t *cur = get_row(s_selected);
    if (cur) {
        lv_obj_set_style_bg_color(cur, lv_color_hex(th->accent), LV_PART_MAIN);
        lv_obj_t *title = lv_obj_get_child(cur, 0);
        lv_obj_t *meta  = lv_obj_get_child(cur, 1);
        if (title) lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        if (meta)  lv_obj_set_style_text_color(meta,  lv_color_hex(0xFFFFFF), LV_PART_MAIN);

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

static void build_rows(void)
{
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    for (int i = 0; i < s_count; i++) {
        const event_t *e = &s_items[i];

        lv_obj_t *row = lv_obj_create(s_list);
        lv_obj_set_size(row, p->events_row_w, p->events_item_h);
        lv_obj_set_style_bg_color(row, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 3, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(row, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_left(row, p->events_row_pad_hor, LV_PART_MAIN);
        lv_obj_set_style_pad_right(row, p->events_row_pad_hor, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *title = lv_label_create(row);
        lv_label_set_text(title, e->title[0] ? e->title : "(no title)");
        lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(title, p->events_row_label_w);
        lv_obj_set_style_text_font(title, p->events_title_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(th->text_primary), LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

        char meta_buf[64];
        snprintf(meta_buf, sizeof(meta_buf), "%s  %02d.%02d  %02d:%02d%s",
                 events_type_label(e->type),
                 e->day, e->month,
                 e->hour, e->minute,
                 e->enabled ? "" : "  (off)");

        lv_obj_t *meta = lv_label_create(row);
        lv_label_set_text(meta, meta_buf);
        lv_label_set_long_mode(meta, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(meta, p->events_row_label_w);
        lv_obj_set_style_text_font(meta, p->events_meta_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(meta, lv_color_hex(th->text_secondary), LV_PART_MAIN);
        lv_obj_align(meta, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
}

static void scr_create(lv_obj_t *parent)
{
    s_root = parent;

    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();
    const int16_t list_h = DISPLAY_HEIGHT - p->events_header_h;

    s_count = events_get_all(s_items, EVENTS_MAX);
    if (s_count < 0) s_count = 0;
    s_selected = 0;

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);

    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_size(header, DISPLAY_WIDTH, p->events_header_h);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    s_header_label = lv_label_create(header);
    lv_label_set_text_fmt(s_header_label, "Events (%d)", s_count);
    lv_obj_set_style_text_font(s_header_label, p->events_header_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_header_label, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_align(s_header_label, LV_ALIGN_CENTER, 0, 0);

    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, DISPLAY_WIDTH, list_h);
    lv_obj_align(s_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_list, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_list, p->events_item_pad, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_ACTIVE);

    if (s_count == 0) {
        s_empty_label = lv_label_create(s_list);
        lv_label_set_text(s_empty_label, "No events configured");
        lv_obj_set_style_text_font(s_empty_label, p->events_meta_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_empty_label, lv_color_hex(th->text_muted), LV_PART_MAIN);
    } else {
        build_rows();
        highlight_item(0);
    }

    ESP_LOGI(TAG, "Created, %d events", s_count);
}

static void scr_destroy(void)
{
    s_root         = NULL;
    s_list         = NULL;
    s_header_label = NULL;
    s_empty_label  = NULL;
    s_count        = 0;
    s_selected     = 0;
    ESP_LOGI(TAG, "Destroyed");
}

static void scr_on_event(const ui_event_t *ev)
{
    (void)ev;
}

static void scr_on_input(ui_input_t input)
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
        case UI_INPUT_BTN_BACK:
        case UI_INPUT_ENCODER_LONG_PRESS:
            ui_navigate(SCREEN_SETTINGS);
            break;

        default:
            break;
    }
}

static void scr_apply_theme(void)
{
    if (!s_root || !s_list) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    if (s_header_label)
        lv_obj_set_style_text_color(s_header_label, lv_color_hex(th->accent), LV_PART_MAIN);
    if (s_empty_label)
        lv_obj_set_style_text_color(s_empty_label, lv_color_hex(th->text_muted), LV_PART_MAIN);

    int n = lv_obj_get_child_count(s_list);
    for (int i = 0; i < n && i < s_count; i++) {
        lv_obj_t *row   = lv_obj_get_child(s_list, i);
        lv_obj_t *title = lv_obj_get_child(row, 0);
        lv_obj_t *meta  = lv_obj_get_child(row, 1);
        if (i == s_selected) {
            lv_obj_set_style_bg_color(row, lv_color_hex(th->accent), LV_PART_MAIN);
            if (title) lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            if (meta)  lv_obj_set_style_text_color(meta,  lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(row, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
            if (title) lv_obj_set_style_text_color(title, lv_color_hex(th->text_primary),   LV_PART_MAIN);
            if (meta)  lv_obj_set_style_text_color(meta,  lv_color_hex(th->text_secondary), LV_PART_MAIN);
        }
    }

    lv_obj_invalidate(s_root);
}

const ui_screen_t screen_events = {
    .create      = scr_create,
    .destroy     = scr_destroy,
    .apply_theme = scr_apply_theme,
    .on_event    = scr_on_event,
    .on_input    = scr_on_input,
    .name        = "events",
};
