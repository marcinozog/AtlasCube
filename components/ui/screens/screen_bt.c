#include "defines.h"
#include "screen_bt.h"
#include "ui_screen.h"
#include "ui_label.h"
#include "clock_widget.h"
#include "mode_indicator_widget.h"
#include "controls_overlay_widget.h"
#include "vol_overlay_widget.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "ui_nav.h"
#include "app_state.h"
#include "settings.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SCR_BT";

static lv_obj_t *s_root         = NULL;
static lv_obj_t *s_circle       = NULL;
static lv_obj_t *s_icon         = NULL;
static lv_obj_t *s_brand_label  = NULL;
static lv_obj_t *s_vol_label    = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_title_label  = NULL;
static lv_obj_t *s_artist_label = NULL;
static lv_obj_t *s_time_label   = NULL;

static void format_time(int seconds, char *out, size_t out_size)
{
    if (seconds < 0) seconds = 0;
    int m = seconds / 60;
    int s = seconds % 60;
    snprintf(out, out_size, "%d:%02d", m, s);
}

// ---------------------------------------------------------------------------

static void refresh_from_state(void)
{
    if (!s_status_label && !s_vol_label) return;

    app_state_t *s = app_state_get();

    // Volume
    char vol_buf[16];
    snprintf(vol_buf, sizeof(vol_buf), "VOL: %d%%", s->bt_volume);
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

    // Track metadata — set_text only on real change so SCROLL_CIRCULAR
    // doesn't restart every second when +PYPS triggers a state refresh.
    if (s_title_label && strcmp(lv_label_get_text(s_title_label), s->bt_title) != 0) {
        lv_label_set_text(s_title_label, s->bt_title);
    }
    if (s_artist_label && strcmp(lv_label_get_text(s_artist_label), s->bt_artist) != 0) {
        lv_label_set_text(s_artist_label, s->bt_artist);
    }
    if (s_time_label) {
        char cur[8], total[8], buf[24];
        format_time(s->bt_position_s, cur, sizeof(cur));
        format_time(s->bt_duration_ms / 1000, total, sizeof(total));
        snprintf(buf, sizeof(buf), "%s / %s", cur, total);
        lv_label_set_text(s_time_label, buf);
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
        clock_widget_create(parent, p->bt_clock_widget_x, p->bt_clock_widget_y,
                            p->bt_clock_font, UI_ALIGN_LEFT);
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

    s_brand_label = ui_anchored_label(parent, p->bt_brand_x, p->bt_brand_y, UI_ALIGN_CENTER);
    lv_label_set_text(s_brand_label, "Bluetooth Audio");
    lv_obj_set_style_text_font(s_brand_label, p->bt_brand_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_brand_label, lv_color_hex(th->bt_brand), LV_PART_MAIN);

    s_status_label = ui_anchored_label(parent, p->bt_status_x, p->bt_status_y, UI_ALIGN_CENTER);
    lv_obj_set_style_text_font(s_status_label, p->bt_status_font, LV_PART_MAIN);

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

    // Center-anchored on bt_time_x, one line below the time label (same model as
    // the layout editor), so it no longer needs lv_obj_align_to.
    s_vol_label = ui_anchored_label(parent, p->bt_time_x,
                                    p->bt_time_y + lv_font_get_line_height(p->bt_time_font) + 4,
                                    UI_ALIGN_CENTER);
    lv_label_set_text(s_vol_label, "VOL: 0%");
    lv_obj_set_style_text_font(s_vol_label, p->bt_vol_label_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_vol_label, lv_color_hex(th->text_muted), LV_PART_MAIN);

    // Track metadata labels
    s_title_label = lv_label_create(parent);
    lv_label_set_long_mode(s_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_title_label, p->bt_title_w);
    lv_obj_set_style_text_font(s_title_label, p->bt_title_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_set_pos(s_title_label, p->bt_title_x, p->bt_title_y);
    lv_label_set_text(s_title_label, "");

    s_artist_label = lv_label_create(parent);
    lv_label_set_long_mode(s_artist_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_artist_label, p->bt_artist_w);
    lv_obj_set_style_text_font(s_artist_label, p->bt_artist_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_artist_label, lv_color_hex(th->text_secondary), LV_PART_MAIN);
    lv_obj_set_pos(s_artist_label, p->bt_artist_x, p->bt_artist_y);
    lv_label_set_text(s_artist_label, "");

    s_time_label = ui_anchored_label(parent, p->bt_time_x, p->bt_time_y, UI_ALIGN_CENTER);
    lv_obj_set_style_text_font(s_time_label, p->bt_time_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(th->text_secondary), LV_PART_MAIN);
    lv_label_set_text(s_time_label, "0:00 / 0:00");

    refresh_from_state();

    controls_overlay_create(parent, CTRL_OVL_MODE_BT);

    ESP_LOGI(TAG, "Created (bt_volume=%d, theme=%d)",
             app_state_get()->bt_volume, theme_current());
}

static void bt_destroy(void)
{
    controls_overlay_destroy();
    vol_overlay_hide();
    mode_indicator_destroy();
    clock_widget_destroy();
    s_root = s_circle = s_icon = s_brand_label = s_vol_label = s_status_label = NULL;
    s_title_label = s_artist_label = s_time_label = NULL;
    
    ESP_LOGI(TAG, "Destroyed");
}

static void bt_on_event(const ui_event_t *ev)
{
    switch (ev->type) {
        case UI_EVT_STATE_CHANGED:
            refresh_from_state();
            controls_overlay_refresh();   // center glyph follows bt_playing
            mode_indicator_update();
            clock_widget_tick();
            break;
        default:
            break;
    }
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

            if (s_vol_label) {
                char buf[16];
                snprintf(buf, sizeof(buf), "VOL: %d%%", vol);
                lv_label_set_text(s_vol_label, buf);
            }
            vol_overlay_show(s_root, vol, true);
            break;
        }
        case UI_INPUT_ENCODER_PRESS:
        case UI_INPUT_SWIPE_RIGHT:
            ui_nav_ring_next(SCREEN_BT);
            break;

        case UI_INPUT_ENCODER_LONG_PRESS: {
            app_state_t *s = app_state_get();
            s->bt_enable ? settings_set_bt_enable(false) : settings_set_bt_enable(true);
            break;
        }
        case UI_INPUT_SWIPE_LEFT:
            ui_nav_ring_prev(SCREEN_BT);
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

    lv_obj_set_style_text_color(s_vol_label,
        lv_color_hex(th->text_muted), LV_PART_MAIN);

    if (s_title_label) {
        lv_obj_set_style_text_color(s_title_label,
            lv_color_hex(th->text_primary), LV_PART_MAIN);
    }
    if (s_artist_label) {
        lv_obj_set_style_text_color(s_artist_label,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
    }
    if (s_time_label) {
        lv_obj_set_style_text_color(s_time_label,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
    }

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