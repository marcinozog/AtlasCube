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
static lv_obj_t *s_text_col      = NULL;
static lv_obj_t *s_label_station = NULL;
static lv_obj_t *s_label_title   = NULL;
static int       s_content_w     = 8;
static int       s_label_bg_opa  = 0;

// Both lines have a fixed one-line viewport. Long metadata scrolls inside it
// instead of growing vertically into the widgets below.
#define NPW_MARGIN_RIGHT 10

void now_playing_widget_create(lv_obj_t *parent, int x, int y, lv_text_align_t align,
                               const lv_font_t *station_font,
                               bool show_title,
                               const lv_font_t *title_font,
                               int label_bg_opa)
{
    const ui_theme_colors_t *th = theme_get();
    s_label_bg_opa = label_bg_opa;

    if (!station_font) station_font = &lv_font_montserrat_18_pl;
    if (!title_font)   title_font   = &lv_font_montserrat_14_pl;
    int content_w = DISPLAY_WIDTH - x - NPW_MARGIN_RIGHT;
    if (content_w < 8) content_w = 8;
    s_content_w = content_w;

    // Cross-axis alignment (horizontal, since the frame is a column) follows the
    // requested text align; the only caller uses CENTER.
    lv_flex_align_t cross = (align == LV_TEXT_ALIGN_LEFT)  ? LV_FLEX_ALIGN_START
                          : (align == LV_TEXT_ALIGN_RIGHT) ? LV_FLEX_ALIGN_END
                          :                                  LV_FLEX_ALIGN_CENTER;

    s_frame = lv_obj_create(parent);
    lv_obj_remove_style_all(s_frame);
    lv_obj_set_size(s_frame, content_w, LV_SIZE_CONTENT);
    lv_obj_set_pos(s_frame, x, y);
    lv_obj_set_flex_flow(s_frame, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_frame, LV_FLEX_ALIGN_START, cross, cross);
    lv_obj_clear_flag(s_frame, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // Content-sized pill centered in the frame — holds the station+title pair
    // and the optional per-screen plate.
    s_pill = lv_obj_create(s_frame);
    lv_obj_remove_style_all(s_pill);
    lv_obj_set_size(s_pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_pill, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_pill, 4, LV_PART_MAIN);
    lv_obj_clear_flag(s_pill, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    ui_label_scrim(s_pill, s_label_bg_opa);

    s_text_col = lv_obj_create(s_pill);
    lv_obj_remove_style_all(s_text_col);
    lv_obj_set_size(s_text_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_text_col, LV_FLEX_ALIGN_CENTER, cross, cross);
    lv_obj_set_style_pad_row(s_text_col, 4, LV_PART_MAIN);
    lv_obj_clear_flag(s_text_col, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_label_station = lv_label_create(s_text_col);
    lv_obj_set_width(s_label_station, content_w);
    lv_label_set_long_mode(s_label_station, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(s_label_station, station_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_station, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_label_station, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    if (show_title) {
        s_label_title = lv_label_create(s_text_col);
        lv_obj_set_width(s_label_title, content_w);
        lv_label_set_long_mode(s_label_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
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
    s_text_col      = NULL;
    s_label_station = NULL;
    s_label_title   = NULL;
    s_content_w     = 8;
    s_label_bg_opa  = 0;
    ESP_LOGI(TAG, "Destroyed");
}

static void set_single_line_text(lv_obj_t *label, const char *text)
{
    lv_point_t size;
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_width(label, LV_CLAMP(1, size.x, s_content_w));
    lv_label_set_text(label, text);
}

void now_playing_widget_update(void)
{
    if (!s_label_station) return;
    app_state_t *s = app_state_get();
    set_single_line_text(s_label_station,
        s->station_name[0] ? s->station_name : "Atlas Radio");
    // Show the title only while radio is the active source — otherwise the
    // shared app_state.title holds the SD track and would leak onto this screen.
    // Hide (not just empty) the title label so the pill hugs the station alone.
    if (s_label_title) {
        bool has_title = !s->sd_active && s->title[0];
        set_single_line_text(s_label_title, has_title ? s->title : "");
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
    ui_label_scrim(s_pill, s_label_bg_opa);   // refresh plate colour for the new theme
}
