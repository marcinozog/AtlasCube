#include "now_playing_widget.h"
#include "ui_label.h"
#include "app_state.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "NPW";

// Two independent single-line labels, each center-anchored on its own box
// (x + w/2) with its own background plate. Long metadata scrolls inside the
// box instead of growing into the widgets around it.
static lv_obj_t *s_label_station = NULL;
static lv_obj_t *s_label_title   = NULL;
static int       s_station_w     = 8;
static int       s_title_w       = 8;
static int       s_label_bg_opa  = 0;

static lv_obj_t *make_line(lv_obj_t *parent, int x, int y, int w,
                           const lv_font_t *font, uint32_t color)
{
    lv_obj_t *lbl = ui_anchored_label(parent, x + w / 2, y, UI_ALIGN_CENTER);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    ui_label_scrim(lbl, s_label_bg_opa);
    return lbl;
}

void now_playing_widget_create(lv_obj_t *parent,
                               int station_x, int station_y, int station_w,
                               const lv_font_t *station_font,
                               bool show_title,
                               int title_x, int title_y, int title_w,
                               const lv_font_t *title_font,
                               int label_bg_opa)
{
    const ui_theme_colors_t *th = theme_get();
    s_label_bg_opa = label_bg_opa;

    if (!station_font) station_font = &lv_font_montserrat_18_pl;
    if (!title_font)   title_font   = &lv_font_montserrat_14_pl;
    s_station_w = station_w < 8 ? 8 : station_w;
    s_title_w   = title_w   < 8 ? 8 : title_w;

    s_label_station = make_line(parent, station_x, station_y, s_station_w,
                                station_font, th->accent);
    if (show_title) {
        s_label_title = make_line(parent, title_x, title_y, s_title_w,
                                  title_font, th->text_secondary);
    }

    now_playing_widget_update();
    ESP_LOGI(TAG, "Created");
}

void now_playing_widget_destroy(void)
{
    s_label_station = NULL;
    s_label_title   = NULL;
    s_station_w     = 8;
    s_title_w       = 8;
    s_label_bg_opa  = 0;
    ESP_LOGI(TAG, "Destroyed");
}

// Size the label to its text, capped at the box width — the anchored label
// re-centers itself on every width change, so the box centre stays put.
static void set_single_line_text(lv_obj_t *label, const char *text, int box_w)
{
    lv_point_t size;
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_width(label, LV_CLAMP(1, size.x, box_w));
    lv_label_set_text(label, text);
}

void now_playing_widget_update(void)
{
    if (!s_label_station) return;
    app_state_t *s = app_state_get();
    set_single_line_text(s_label_station,
        s->station_name[0] ? s->station_name : "Atlas Radio", s_station_w);
    // Show the title only while radio is the active source — otherwise the
    // shared app_state.title holds the SD track and would leak onto this screen.
    // Hide (not just empty) the title label so no empty plate shows.
    if (s_label_title) {
        bool has_title = !s->sd_active && s->title[0];
        set_single_line_text(s_label_title, has_title ? s->title : "", s_title_w);
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
    ui_label_scrim(s_label_station, s_label_bg_opa);
    if (s_label_title) {
        lv_obj_set_style_text_color(s_label_title,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
        ui_label_scrim(s_label_title, s_label_bg_opa);
    }
}
