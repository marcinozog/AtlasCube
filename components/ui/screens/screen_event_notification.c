#include "screen_event_notification.h"
#include "ui_manager.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "SCR_EVENT";

// "Pending" data — set by prepare() right before navigation,
// read in create().
static ui_event_info_t s_pending_info;
static ui_screen_id_t  s_return_to = SCREEN_RADIO;

static lv_obj_t *s_icon        = NULL;
static lv_obj_t *s_type_label  = NULL;
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_time_label  = NULL;
static lv_obj_t *s_hint_label  = NULL;

void screen_event_notification_set_info(const ui_event_info_t *info)
{
    if (!info) return;
    s_pending_info = *info;
}

void screen_event_notification_set_return(ui_screen_id_t return_to)
{
    s_return_to = return_to;
}

static void scr_create(lv_obj_t *parent)
{
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    // Large BELL symbol
    s_icon = lv_label_create(parent);
    lv_label_set_text(s_icon, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(s_icon, &lv_font_montserrat_96, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_icon, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_align(s_icon, LV_ALIGN_TOP_MID, 0, 10);

    // Type
    s_type_label = lv_label_create(parent);
    lv_label_set_text(s_type_label,
        s_pending_info.type_label[0] ? s_pending_info.type_label : "EVENT");
    lv_obj_set_style_text_font(s_type_label, &lv_font_montserrat_14_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_type_label, lv_color_hex(th->text_secondary), LV_PART_MAIN);
    lv_obj_align(s_type_label, LV_ALIGN_TOP_MID, 0, 118);

    // Title
    s_title_label = lv_label_create(parent);
    lv_label_set_text(s_title_label, s_pending_info.title);
    lv_label_set_long_mode(s_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_title_label, 300);
    lv_obj_set_style_text_align(s_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_18_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 140);

    // Time
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", s_pending_info.hour, s_pending_info.minute);
    s_time_label = lv_label_create(parent);
    lv_label_set_text(s_time_label, buf);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_14_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_align(s_time_label, LV_ALIGN_TOP_MID, 0, 172);

    // Hint
    s_hint_label = lv_label_create(parent);
    lv_label_set_text(s_hint_label, LV_SYMBOL_OK "  press encoder");
    lv_obj_set_style_text_font(s_hint_label, &lv_font_montserrat_12_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint_label, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_align(s_hint_label, LV_ALIGN_BOTTOM_MID, 0, -8);

    ESP_LOGI(TAG, "Shown: [%s] %s (return_to=%d)",
             s_pending_info.id, s_pending_info.title, s_return_to);
}

static void scr_destroy(void)
{
    s_icon = s_type_label = s_title_label = s_time_label = s_hint_label = NULL;
}

static void scr_apply_theme(void)
{
    if (!s_icon) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(lv_scr_act(),       lv_color_hex(th->bg_primary),     LV_PART_MAIN);
    lv_obj_set_style_text_color(s_icon,           lv_color_hex(th->accent),         LV_PART_MAIN);
    lv_obj_set_style_text_color(s_type_label,     lv_color_hex(th->text_secondary), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_title_label,    lv_color_hex(th->text_primary),   LV_PART_MAIN);
    lv_obj_set_style_text_color(s_time_label,     lv_color_hex(th->accent),         LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint_label,     lv_color_hex(th->text_muted),     LV_PART_MAIN);
}

static void scr_on_input(ui_input_t input)
{
    if (input == UI_INPUT_ENCODER_PRESS || input == UI_INPUT_BTN_OK) {
        ui_navigate(s_return_to);
    }
}

const ui_screen_t screen_event_notification = {
    .create      = scr_create,
    .destroy     = scr_destroy,
    .apply_theme = scr_apply_theme,
    .on_event    = NULL,
    .on_input    = scr_on_input,
    .name        = "event_notification",
};
