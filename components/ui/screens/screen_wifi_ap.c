#include "screen_wifi_ap.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "wifi_manager.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "SCR_WIFI";

static lv_obj_t *s_root = NULL;

// ─────────────────────────────────────────────────────────────────────────────

static void add_row(lv_obj_t *parent,
                    const char *key, const char *value,
                    uint32_t val_color,
                    int y_offset,
                    const ui_theme_colors_t *th,
                    const ui_profile_t *p)
{
    lv_obj_t *k = lv_label_create(parent);
    lv_label_set_text(k, key);
    lv_obj_set_style_text_font(k, p->wifi_key_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(k, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_align(k, LV_ALIGN_TOP_LEFT, 0, y_offset);

    lv_obj_t *v = lv_label_create(parent);
    lv_label_set_text(v, value);
    lv_obj_set_style_text_font(v, p->wifi_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(v, lv_color_hex(val_color), LV_PART_MAIN);
    lv_obj_align(v, LV_ALIGN_TOP_RIGHT, 0, y_offset);
}

// ─────────────────────────────────────────────────────────────────────────────

static void wifi_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    // Background
    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    // ── Title ─────────────────────────────────────────────────────────────────
    lv_obj_t *title = lv_label_create(parent);
    // lv_label_set_text(title, LV_SYMBOL_WIFI "  Connect to AtlasCube");
    lv_label_set_text(title,  "Connect to AtlasCube");
    lv_obj_set_style_text_font(title, p->wifi_title_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, p->wifi_title_y);

    // ── Card (bg_secondary, rounded) ─────────────────────────────────────────
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, p->wifi_card_w, p->wifi_card_h);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, p->wifi_card_y);
    lv_obj_set_style_bg_color(card, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(card, p->wifi_card_pad_hor, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(card, p->wifi_card_pad_ver, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    add_row(card, "SSID",     wifi_get_ap_ssid(), th->text_primary, 0,               th, p);
    add_row(card, "Password", wifi_get_ap_pass(), th->text_primary, p->wifi_row2_y,  th, p);
    add_row(card, "Address",  "192.168.4.1",      th->accent,       p->wifi_row3_y,  th, p);

    // ── Hint at the bottom of the screen ─────────────────────────────────────
    lv_obj_t *hint = lv_label_create(parent);
    lv_label_set_text(hint, "Open browser to configure WiFi:\nhttp://192.168.4.1");
    lv_obj_set_style_text_font(hint, p->wifi_hint_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, p->wifi_hint_y);

    ESP_LOGI(TAG, "Created");
    
}

static void wifi_destroy(void)
{
    s_root = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

static void wifi_on_event(const ui_event_t *ev) { (void)ev; }
static void wifi_on_input(ui_input_t input)      { (void)input; }
static void wifi_apply_theme(void)               { /* static screen, theme does not change in AP */ }

const ui_screen_t screen_wifi = {
    .create      = wifi_create,
    .destroy     = wifi_destroy,
    .apply_theme = wifi_apply_theme,
    .on_event    = wifi_on_event,
    .on_input    = wifi_on_input,
    .name        = "wifi",
};