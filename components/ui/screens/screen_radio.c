#include "defines.h"
#include "screen_radio.h"
#include "ui_events.h"
#include "ui_screen.h"
#include "ui_manager.h"
#include "ui_nav.h"
#include "clock_widget.h"
#include "now_playing_widget.h"
#include "mode_indicator_widget.h"
#include "event_indicator_widget.h"
#include "controls_overlay_widget.h"
#include "vol_overlay_widget.h"
#include "app_state.h"
#include "settings.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "SCR_RADIO";

static lv_obj_t *s_root            = NULL;
static lv_obj_t *s_label_state;
static lv_obj_t *s_label_audio_info;

static const char *radio_state_str(radio_state_t st)
{
    switch (st) {
        case RADIO_STATE_STOPPED:   return "STOPPED";
        case RADIO_STATE_PLAYING:   return "PLAYING";
        case RADIO_STATE_BUFFERING: return "BUFFERING";
        case RADIO_STATE_ERROR:     return "ERROR";
        default:                    return "?";
    }
}

static void refresh_from_state(void)
{
    if (!s_label_state) return;
    app_state_t *s = app_state_get();

    now_playing_widget_update();

    lv_label_set_text(s_label_state, radio_state_str(s->radio_state));

    char info[80];
    if (s->sample_rate > 0) {
        snprintf(info, sizeof(info), "%d Hz  %dch  %dkbps   VOL: %d%%",
                 s->sample_rate, s->channels, s->bitrate / 1000, s->volume);
    } else {
        snprintf(info, sizeof(info), "VOL: %d%%", s->volume);
    }
    lv_label_set_text(s_label_audio_info, info);
}

static void radio_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    if (p->radio_show_np) {
        now_playing_widget_create(parent, p->radio_np_x, p->radio_np_y, LV_TEXT_ALIGN_CENTER,
                                  p->radio_np_station_font, p->radio_np_title_font);
    }
    if (p->radio_show_mode_indicator) {
        mode_indicator_create(parent, p->radio_mode_indic_x, p->radio_mode_indic_y);
    }
    if (p->radio_show_clock) {
        clock_widget_create(parent, p->radio_clock_widget_x, p->radio_clock_widget_y, p->radio_clock_font);
    }
    if (p->radio_show_event_indicator) {
        event_indicator_create(parent, p->radio_event_indic_x, p->radio_event_indic_y);
    }

    s_label_state = lv_label_create(parent);
    lv_label_set_text(s_label_state, "");
    lv_obj_set_style_text_font(s_label_state, p->radio_state_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_state, lv_color_hex(th->status_ok), LV_PART_MAIN);
    lv_obj_set_width(s_label_state, lv_pct(100));
    lv_obj_set_style_text_align(s_label_state, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_label_state, 0, p->radio_state_y);

    s_label_audio_info = lv_label_create(parent);
    lv_label_set_text(s_label_audio_info, "");
    lv_obj_set_style_text_font(s_label_audio_info, p->radio_audio_info_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_audio_info, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_set_width(s_label_audio_info, lv_pct(100));
    lv_obj_set_style_text_align(s_label_audio_info, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(s_label_audio_info, 0, p->radio_audio_info_y);

    refresh_from_state();

    controls_overlay_create(parent, CTRL_OVL_MODE_RADIO);

    ESP_LOGI(TAG, "Created (theme=%d)", theme_current());
}

static void radio_destroy(void)
{
    controls_overlay_destroy();
    vol_overlay_hide();
    now_playing_widget_destroy();
    mode_indicator_destroy();
    event_indicator_destroy();
    clock_widget_destroy();
    s_root             = NULL;
    s_label_state      = NULL;
    s_label_audio_info = NULL;

    ESP_LOGI(TAG, "Destroyed");
}

static void radio_on_event(const ui_event_t *ev)
{
    switch (ev->type) {
        case UI_EVT_STATE_CHANGED:
            refresh_from_state();
            mode_indicator_update();
            event_indicator_update();
            clock_widget_tick();
            break;
        case UI_EVT_TITLE_CHANGED:
            now_playing_widget_update();
            break;
        case UI_EVT_RADIO_STATE:
            if (s_label_state)
                lv_label_set_text(s_label_state, radio_state_str(ev->radio_state));
            break;
        default:
            break;
    }
}

static void radio_on_input(ui_input_t input)
{
    switch (input) {
        case UI_INPUT_ENCODER_CW:
        case UI_INPUT_ENCODER_CCW: {
            app_state_t *s = app_state_get();
            int vol = s->volume;
            vol += (input == UI_INPUT_ENCODER_CW) ? RADIO_VOL_STEP : -RADIO_VOL_STEP;
            if (vol < 0)   vol = 0;
            if (vol > 100) vol = 100;

            settings_set_volume(vol);   // → audio_player + app_state + save
            vol_overlay_show(s_root, vol, true);
            break;
        }

        case UI_INPUT_ENCODER_PRESS:
        case UI_INPUT_SWIPE_RIGHT:
            ui_nav_ring_next(SCREEN_RADIO);
            break;
        case UI_INPUT_ENCODER_LONG_PRESS:
            ui_navigate(SCREEN_PLAYLIST);
            break;
        case UI_INPUT_SWIPE_LEFT:
            ui_nav_ring_prev(SCREEN_RADIO);
            break;
        case UI_INPUT_SWIPE_UP:
            ui_navigate(SCREEN_PLAYLIST);
            break;

        default:
            break;
    }
}

static void radio_apply_theme(void)
{
    if (!s_root) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);

    lv_obj_set_style_text_color(s_label_state,
        lv_color_hex(th->status_ok), LV_PART_MAIN);

    lv_obj_set_style_text_color(s_label_audio_info,
        lv_color_hex(th->text_muted), LV_PART_MAIN);

    now_playing_widget_apply_theme();
    clock_widget_apply_theme();
    mode_indicator_apply_theme();
    event_indicator_apply_theme();

    lv_obj_invalidate(s_root);
}

const ui_screen_t screen_radio = {
    .create         = radio_create,
    .destroy        = radio_destroy,
    .apply_theme    = radio_apply_theme,
    .on_event       = radio_on_event,
    .on_input       = radio_on_input,
    .name           = "radio",
};