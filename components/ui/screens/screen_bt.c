#include "defines.h"
#include "screen_bt.h"
#include "ui_screen.h"
#include "clock_widget.h"
#include "mode_indicator_widget.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "app_state.h"
#include "settings.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "SCR_BT";

static lv_obj_t *s_root         = NULL;
static lv_obj_t *s_circle       = NULL;
static lv_obj_t *s_icon         = NULL;
static lv_obj_t *s_brand_label  = NULL;
static lv_obj_t *s_vol_label    = NULL;
static lv_obj_t *s_slider       = NULL;
static lv_obj_t *s_status_label = NULL;

// ---------------------------------------------------------------------------

static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    settings_set_bt_volume(value);   // → bt_set_volume + app_state + save
}

static void refresh_from_state(void)
{
    if (!s_status_label && !s_slider && !s_vol_label) return;
    
    app_state_t *s = app_state_get();

    // BT volume slider
    if (!lv_obj_has_state(s_slider, LV_STATE_PRESSED)) {
        lv_slider_set_value(s_slider, s->bt_volume, LV_ANIM_OFF);
    }

    // Volume
    lv_slider_set_value(s_slider, s->bt_volume, LV_ANIM_OFF);
    char vol_buf[16];
    snprintf(vol_buf, sizeof(vol_buf), "%d%%", s->bt_volume);
    lv_label_set_text(s_vol_label, vol_buf);

    // Connection status
    const ui_theme_colors_t *th = theme_get();
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(th->status_ok), LV_PART_MAIN);
    if(s->bt_state == BT_CONNECTED) {
        lv_label_set_text(s_status_label, "Connected");
    }
    else if(s->bt_state == BT_DISCONNECTED) {
        lv_label_set_text(s_status_label, "Not connected");
    }
    else if(s->bt_state == BT_DISCOVERABLE) {
        lv_label_set_text(s_status_label, "Discoverable");
    }
}

static void bt_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    if (p->bt_show_mode_indicator) {
        mode_indicator_create(parent, p->bt_mode_indic_x, p->bt_mode_indic_y);
    }
    if (p->bt_show_clock) {
        clock_widget_create(parent, p->bt_clock_widget_x, p->bt_clock_widget_y, false);
    }

    if (p->bt_show_circle) {
        s_circle = lv_obj_create(parent);
        lv_obj_set_size(s_circle, p->bt_circle_w, p->bt_circle_h);
        lv_obj_set_pos(s_circle, p->bt_circle_x, p->bt_circle_y);
        lv_obj_set_style_radius(s_circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_circle, lv_color_hex(th->bt_brand), LV_PART_MAIN);
        lv_obj_set_style_border_width(s_circle, 0, LV_PART_MAIN);

        s_icon = lv_label_create(s_circle);
        lv_label_set_text(s_icon, LV_SYMBOL_BLUETOOTH);
        lv_obj_set_style_text_font(s_icon, p->bt_icon_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_icon, lv_color_hex(th->text_primary), LV_PART_MAIN);
        lv_obj_center(s_icon);
    }

    s_brand_label = lv_label_create(parent);
    lv_label_set_text(s_brand_label, "Bluetooth Audio");
    lv_obj_set_style_text_font(s_brand_label, p->bt_brand_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_brand_label, lv_color_hex(th->bt_brand), LV_PART_MAIN);
    lv_obj_set_pos(s_brand_label, p->bt_brand_x, p->bt_brand_y);

    s_status_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_status_label, p->bt_status_font, LV_PART_MAIN);
    lv_obj_set_pos(s_status_label, p->bt_status_x, p->bt_status_y);

    bt_state_t btState = app_state_get()->bt_state;

    lv_obj_set_style_text_color(s_status_label, lv_color_hex(th->status_ok), LV_PART_MAIN);
    if(btState == BT_CONNECTED) {
        lv_label_set_text(s_status_label, "Connected");
    }
    else if(btState == BT_DISCONNECTED) {
        lv_label_set_text(s_status_label, "Not connected");
    }
    else if(btState == BT_DISCOVERABLE) {
        lv_label_set_text(s_status_label, "Discoverable");
    }
    // lv_label_set_text(s_status_label, connected == BT_CONNECTED ? LV_SYMBOL_OK " Connected" : "Not connected");
    // lv_obj_set_style_text_color(s_status_label,
    //     lv_color_hex(connected == BT_CONNECTED ? 0x00C853 : th->text_muted), LV_PART_MAIN);

    s_slider = lv_slider_create(parent);
    lv_slider_set_range(s_slider, 0, 100);
    lv_obj_set_size(s_slider, p->bt_slider_w, p->bt_slider_h);
    lv_obj_set_pos(s_slider, p->bt_slider_x, p->bt_slider_y);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(th->bt_brand),     LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(th->text_primary), LV_PART_KNOB);
    lv_slider_set_value(s_slider, app_state_get()->bt_volume, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_slider, slider_event_cb, LV_EVENT_RELEASED, NULL);

    s_vol_label = lv_label_create(parent);
    lv_label_set_text(s_vol_label, "0%");
    lv_obj_set_style_text_font(s_vol_label, p->bt_vol_label_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_vol_label, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_set_pos(s_vol_label, p->bt_vol_label_x, p->bt_vol_label_y);

    refresh_from_state();

    ESP_LOGI(TAG, "Created (bt_volume=%d, theme=%d)",
             app_state_get()->bt_volume, theme_current());
}

static void bt_destroy(void)
{
    mode_indicator_destroy();
    clock_widget_destroy();
    s_root = s_circle = s_icon = s_brand_label = s_vol_label = s_slider = s_status_label = NULL;
    
    ESP_LOGI(TAG, "Destroyed");
}

static void bt_on_event(const ui_event_t *ev)
{
    switch (ev->type) {
        case UI_EVT_STATE_CHANGED:
            refresh_from_state();
            mode_indicator_update();
            clock_widget_tick();
            break;
        // case UI_EVT_TITLE_CHANGED:
        //     break;
        // case UI_EVT_VOLUME_CHANGED:
        //     if (s_slider) {
        //         lv_slider_set_value(s_slider, ev->volume, LV_ANIM_ON);
        //         char buf[16];
        //         snprintf(buf, sizeof(buf), "%d%%", ev->volume);
        //         lv_label_set_text(s_vol_label, buf);
        //     }
        //     break;
        default:
            break;
    }

    // if (ev->type == UI_EVT_STATE_CHANGED && s_slider) {
    //     app_state_t *s = app_state_get();

    //     // BT volume slider
    //     if (!lv_obj_has_state(s_slider, LV_STATE_PRESSED)) {
    //         lv_slider_set_value(s_slider, s->bt_volume, LV_ANIM_OFF);
    //     }

    //     // Connection status
    //     const ui_theme_colors_t *th = theme_get();
    //     lv_obj_set_style_text_color(s_status_label, lv_color_hex(th->status_ok), LV_PART_MAIN);
    //     if(s->bt_state == BT_CONNECTED) {
    //         lv_label_set_text(s_status_label, "Connected");
    //     }
    //     else if(s->bt_state == BT_DISCONNECTED) {
    //         lv_label_set_text(s_status_label, "Not connected");
    //     }
    //     else if(s->bt_state == BT_DISCOVERABLE) {
    //         lv_label_set_text(s_status_label, "Discoverable");
    //     }
    // }
}

static void bt_on_input(ui_input_t input)
{
    switch (input) {
        case UI_INPUT_ENCODER_CW:
        case UI_INPUT_ENCODER_CCW: {
            app_state_t *s = app_state_get();
            int vol = s->bt_volume;
            vol += (input == UI_INPUT_ENCODER_CW) ? BT_VOL_STEP : -BT_VOL_STEP;
            if (vol < 0)  vol = 0;
            if (vol > 100) vol = 100;

            settings_set_bt_volume(vol);   // → bt_set_volume + app_state + save

            // optymistyczna aktualizacja slidera
            if (s_slider && !lv_obj_has_state(s_slider, LV_STATE_PRESSED)) {
                lv_slider_set_value(s_slider, vol, LV_ANIM_OFF);
                char buf[16];
                snprintf(buf, sizeof(buf), "%d%%", vol);
                lv_label_set_text(s_vol_label, buf);
            }
            break;
        }

        case UI_INPUT_ENCODER_PRESS:
            settings_set_screen(SCREEN_CLOCK); // set & save
            break;
        case UI_INPUT_ENCODER_LONG_PRESS:
            app_state_t *s = app_state_get();
            s->bt_enable ? settings_set_bt_enable(false) : settings_set_bt_enable(true);
            break;

        default:
            break;
        }
}

static void bt_apply_theme(void)
{
    if (!s_root) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);

    // BT circle + icon (optional)
    if (s_circle) {
        lv_obj_set_style_bg_color(s_circle,
            lv_color_hex(th->bt_brand), LV_PART_MAIN);
        lv_obj_set_style_text_color(s_icon,
            lv_color_hex(th->text_primary), LV_PART_MAIN);
    }

    // napis "Bluetooth Audio"
    lv_obj_set_style_text_color(s_brand_label,
        lv_color_hex(th->bt_brand), LV_PART_MAIN);

    // status (Connected / Not connected / Discoverable)
    lv_obj_set_style_text_color(s_status_label,
        lv_color_hex(th->status_ok), LV_PART_MAIN);

    // slider — track, indicator (brand), knob
    lv_obj_set_style_bg_color(s_slider,
        lv_color_hex(th->bg_secondary), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider,
        lv_color_hex(th->bt_brand), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider,
        lv_color_hex(th->text_primary), LV_PART_KNOB);

    lv_obj_set_style_text_color(s_vol_label,
        lv_color_hex(th->text_muted), LV_PART_MAIN);

    mode_indicator_apply_theme();
    clock_widget_apply_theme();

    lv_obj_invalidate(s_root);
}

const ui_screen_t screen_bt = {
    .create         = bt_create,
    .destroy        = bt_destroy,
    .apply_theme    = bt_apply_theme,
    .on_event       = bt_on_event,
    .on_input       = bt_on_input,
    .name           = "bt",
};