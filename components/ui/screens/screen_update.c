#include "screen_update.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "fonts/ui_fonts.h"
#include "theme.h"
#include "updater.h"
#include "radio_service.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include <stdint.h>

static const char *TAG = "SCR_UPD";

// Where to go when the prompt is dismissed (parity with screen_ota).
#define UPDATE_RETURN_SCREEN SCREEN_RADIO

enum { ACT_UPDATE = 0, ACT_LATER, ACT_COUNT };

static lv_obj_t *s_btns[ACT_COUNT];
static int       s_sel;                 // highlighted button (encoder navigation)

// ── Actions ───────────────────────────────────────────────────────────────────
static void do_action(int act)
{
    switch (act) {
    case ACT_UPDATE:
        ESP_LOGI(TAG, "user confirmed update → %s", updater_latest_version());
        ui_navigate(SCREEN_OTA);        // reuse the OTA progress screen
        radio_stop();                   // free internal RAM for the sustained HTTPS pull
        updater_apply();                // spawns the download task; progress → SCREEN_OTA
        break;
    case ACT_LATER:
    default:
        // Dismiss until the next boot check (no persistence — global opt-out is
        // settings.update.enable).
        ui_navigate(UPDATE_RETURN_SCREEN);
        break;
    }
}

static void btn_clicked_cb(lv_event_t *e)
{
    do_action((int)(intptr_t)lv_event_get_user_data(e));
}

// ── Encoder-selection highlight ───────────────────────────────────────────────
static void refresh_highlight(void)
{
    const ui_theme_colors_t *th = theme_get();
    for (int i = 0; i < ACT_COUNT; i++) {
        if (!s_btns[i]) continue;
        bool on = (i == s_sel);
        lv_obj_set_style_border_width(s_btns[i], on ? 3 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_btns[i], lv_color_hex(th->accent), LV_PART_MAIN);
    }
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int act, uint32_t bg)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, btn_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)act);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18_pl, LV_PART_MAIN);
    lv_obj_center(lbl);
    return btn;
}

static void update_create(lv_obj_t *parent)
{
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    // Flex goes on an inner container, never on the shared parent: layout
    // styles survive lv_obj_clean() and would leak into the next screen.
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 10, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(col);
    lv_label_set_text(title, "NEW FIRMWARE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(th->text_primary), LV_PART_MAIN);

    lv_obj_t *ver = lv_label_create(col);
    lv_label_set_text_fmt(ver, "%s available", updater_latest_version());
    lv_obj_set_style_text_font(ver, &lv_font_montserrat_18_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(ver, lv_color_hex(th->accent), LV_PART_MAIN);

    lv_obj_t *cur = lv_label_create(col);
    lv_label_set_text_fmt(cur, "current %s", esp_app_get_description()->version);
    lv_obj_set_style_text_font(cur, &lv_font_montserrat_12_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(cur, lv_color_hex(th->text_muted), LV_PART_MAIN);

    // Button row.
    lv_obj_t *row = lv_obj_create(col);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(92), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(row, 6, LV_PART_MAIN);

    s_btns[ACT_UPDATE] = make_button(row, "Update", ACT_UPDATE, th->accent);
    s_btns[ACT_LATER]  = make_button(row, "Later",  ACT_LATER,  th->bg_secondary);

    s_sel = ACT_UPDATE;
    refresh_highlight();
    ESP_LOGI(TAG, "Created");
}

static void update_destroy(void)
{
    for (int i = 0; i < ACT_COUNT; i++) s_btns[i] = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

// Encoder / button navigation for touch-less panels.
static void update_on_input(ui_input_t input)
{
    switch (input) {
    case UI_INPUT_ENCODER_CW:   s_sel = (s_sel + 1) % ACT_COUNT;            refresh_highlight(); break;
    case UI_INPUT_ENCODER_CCW:  s_sel = (s_sel + ACT_COUNT - 1) % ACT_COUNT; refresh_highlight(); break;
    case UI_INPUT_ENCODER_PRESS:
    case UI_INPUT_BTN_OK:       do_action(s_sel); break;
    default: break;
    }
}

const ui_screen_t screen_update = {
    .create      = update_create,
    .destroy     = update_destroy,
    .apply_theme = NULL,
    .on_event    = NULL,
    .on_input    = update_on_input,
    .name        = "update",
};
