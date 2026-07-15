#include "now_playing_widget.h"
#include "ui_profile.h"
#include "ui_label.h"
#include "app_state.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NPW";

// s_frame is a full-width centering frame (transparent); s_pill hugs the
// station+title pair and carries the optional background plate — same pattern
// as the weather widget, so the plate tracks the content, not the screen width.
static lv_obj_t *s_frame         = NULL;
static lv_obj_t *s_pill          = NULL;
static lv_obj_t *s_icon          = NULL;
static lv_obj_t *s_text_col      = NULL;
static lv_obj_t *s_label_station = NULL;
static lv_obj_t *s_label_title   = NULL;
static int       s_icon_size     = 64;

// Long text wraps within the content cap (rather than running off both screen
// edges) — the plate hugs short text and never exceeds the panel width.
#define NPW_MAX_W  (DISPLAY_WIDTH - 20)

void now_playing_widget_create(lv_obj_t *parent, int x, int y, lv_text_align_t align,
                               const lv_font_t *station_font,
                               bool show_title,
                               const lv_font_t *title_font,
                               int icon_size)
{
    (void)x;   // horizontal position is screen-centered via the full-width frame
    const ui_theme_colors_t *th = theme_get();

    if (!station_font) station_font = &lv_font_montserrat_18_pl;
    if (!title_font)   title_font   = &lv_font_montserrat_14_pl;
    s_icon_size = icon_size < 16 ? 16 : icon_size > 64 ? 64 : icon_size;

    // Cross-axis alignment (horizontal, since the frame is a column) follows the
    // requested text align; the only caller uses CENTER.
    lv_flex_align_t cross = (align == LV_TEXT_ALIGN_LEFT)  ? LV_FLEX_ALIGN_START
                          : (align == LV_TEXT_ALIGN_RIGHT) ? LV_FLEX_ALIGN_END
                          :                                  LV_FLEX_ALIGN_CENTER;

    s_frame = lv_obj_create(parent);
    lv_obj_remove_style_all(s_frame);
    lv_obj_set_size(s_frame, DISPLAY_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_pos(s_frame, 0, y);
    lv_obj_set_flex_flow(s_frame, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_frame, LV_FLEX_ALIGN_START, cross, cross);
    lv_obj_clear_flag(s_frame, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // Content-sized pill centered in the frame — holds the station+title pair
    // and the optional plate (global display.label_bg setting).
    s_pill = lv_obj_create(s_frame);
    lv_obj_remove_style_all(s_pill);
    lv_obj_set_size(s_pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_pill, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_pill, 4, LV_PART_MAIN);
    lv_obj_clear_flag(s_pill, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    ui_label_scrim(s_pill);

    s_icon = lv_image_create(s_pill);
    lv_obj_add_flag(s_icon, LV_OBJ_FLAG_HIDDEN);

    s_text_col = lv_obj_create(s_pill);
    lv_obj_remove_style_all(s_text_col);
    lv_obj_set_size(s_text_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_text_col, LV_FLEX_ALIGN_CENTER, cross, cross);
    lv_obj_set_style_pad_row(s_text_col, 4, LV_PART_MAIN);
    lv_obj_clear_flag(s_text_col, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_label_station = lv_label_create(s_text_col);
    lv_obj_set_style_max_width(s_label_station, NPW_MAX_W, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_station, station_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_station, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_label_station, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    if (show_title) {
        s_label_title = lv_label_create(s_text_col);
        lv_obj_set_style_max_width(s_label_title, NPW_MAX_W, LV_PART_MAIN);
        lv_obj_set_style_text_font(s_label_title, title_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_label_title, lv_color_hex(th->text_secondary), LV_PART_MAIN);
        lv_obj_set_style_text_align(s_label_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    now_playing_widget_update();
    ESP_LOGI(TAG, "Created");
}

void now_playing_widget_destroy(void)
{
    s_frame         = NULL;
    s_pill          = NULL;
    s_icon          = NULL;
    s_text_col      = NULL;
    s_label_station = NULL;
    s_label_title   = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

void now_playing_widget_update(void)
{
    if (!s_label_station) return;
    app_state_t *s = app_state_get();
    lv_label_set_text(s_label_station,
        s->station_name[0] ? s->station_name : "Atlas Radio");
    // Show the title only while radio is the active source — otherwise the
    // shared app_state.title holds the SD track and would leak onto this screen.
    // Hide (not just empty) the title label so the pill hugs the station alone.
    if (s_label_title) {
        bool has_title = !s->sd_active && s->title[0];
        lv_label_set_text(s_label_title, has_title ? s->title : "");
        if (has_title) lv_obj_clear_flag(s_label_title, LV_OBJ_FLAG_HIDDEN);
        else           lv_obj_add_flag(s_label_title, LV_OBJ_FLAG_HIDDEN);
    }
}

void now_playing_widget_apply_theme(void)
{
    if (!s_label_station) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_text_color(s_label_station,
        lv_color_hex(th->accent), LV_PART_MAIN);
    if (s_label_title) {
        lv_obj_set_style_text_color(s_label_title,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
    }
    ui_label_scrim(s_pill);   // refresh plate colour for the new theme
}

void now_playing_widget_set_icon(const lv_image_dsc_t *icon)
{
    if (!s_icon) return;
    if (icon) {
        lv_image_set_src(s_icon, icon);
        lv_obj_set_size(s_icon, s_icon_size, s_icon_size);
        lv_image_set_inner_align(s_icon, LV_IMAGE_ALIGN_STRETCH);
        lv_obj_clear_flag(s_icon, LV_OBJ_FLAG_HIDDEN);
        int text_w = NPW_MAX_W - s_icon_size - 8;
        lv_obj_set_style_max_width(s_label_station, text_w, LV_PART_MAIN);
        if (s_label_title) lv_obj_set_style_max_width(s_label_title, text_w, LV_PART_MAIN);
    } else {
        lv_image_set_src(s_icon, NULL);
        lv_obj_add_flag(s_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_max_width(s_label_station, NPW_MAX_W, LV_PART_MAIN);
        if (s_label_title) lv_obj_set_style_max_width(s_label_title, NPW_MAX_W, LV_PART_MAIN);
    }
}
