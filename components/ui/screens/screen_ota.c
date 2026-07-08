#include "screen_ota.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "theme.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "SCR_OTA";

// On a failed update we cannot stay here forever — this screen is outside the
// normal navigation cycle, so auto-return to the radio screen after a moment.
#define OTA_FAIL_RETURN_MS 3500
#define OTA_FAIL_COLOR     0xE0413E

static lv_obj_t *s_bar    = NULL;
static lv_obj_t *s_pct    = NULL;
static lv_obj_t *s_status = NULL;

static void return_timer_cb(lv_timer_t *t)
{
    (void)t;
    ui_navigate(SCREEN_RADIO);
}

static void ota_create(lv_obj_t *parent)
{
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    // Centered column: title · progress bar · percentage · hint.
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
    lv_label_set_text(title, "FIRMWARE UPDATE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(th->text_primary), LV_PART_MAIN);

    s_bar = lv_bar_create(col);
    lv_obj_set_size(s_bar, lv_pct(78), 14);
    lv_bar_set_range(s_bar, 0, 100);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(s_bar, 7, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar, 7, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(th->accent), LV_PART_INDICATOR);

    s_pct = lv_label_create(col);
    lv_label_set_text(s_pct, "0%");
    lv_obj_set_style_text_font(s_pct, &lv_font_montserrat_24_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_pct, lv_color_hex(th->accent), LV_PART_MAIN);

    s_status = lv_label_create(col);
    lv_label_set_text(s_status, "Do not power off");
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_12_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_status, lv_color_hex(th->text_muted), LV_PART_MAIN);

    ESP_LOGI(TAG, "Created");
}

static void ota_destroy(void)
{
    s_bar = s_pct = s_status = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

static void ota_on_event(const ui_event_t *ev)
{
    if (ev->type != UI_EVT_OTA_PROGRESS) return;
    if (!s_bar || !s_pct || !s_status)   return;

    int p = ev->ota_progress;

    if (p < 0) {                                   // failure
        lv_label_set_text(s_pct, "FAILED");
        lv_obj_set_style_text_color(s_pct, lv_color_hex(OTA_FAIL_COLOR), LV_PART_MAIN);
        lv_label_set_text(s_status, "Update failed - returning...");
        lv_timer_t *t = lv_timer_create(return_timer_cb, OTA_FAIL_RETURN_MS, NULL);
        lv_timer_set_repeat_count(t, 1);           // one-shot, auto-deletes
        return;
    }

    if (p > 100) p = 100;
    lv_bar_set_value(s_bar, p, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_pct, "%d%%", p);
    if (p >= 100) lv_label_set_text(s_status, "Done - rebooting...");
}

const ui_screen_t screen_ota = {
    .create      = ota_create,
    .destroy     = ota_destroy,
    .apply_theme = NULL,
    .on_event    = ota_on_event,
    .on_input    = NULL,
    .name        = "ota",
};
