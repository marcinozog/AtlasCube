#include "defines.h"
#include "screen_radio.h"
#include "screen_playlist.h"
#include "ui_events.h"
#include "ui_screen.h"
#include "ui_manager.h"
#include "ui_nav.h"
#include "ui_label.h"
#include "clock_widget.h"
#include "now_playing_widget.h"
#include "station_icon_widget.h"
#include "mode_indicator_widget.h"
#include "event_indicator_widget.h"
#include "weather_widget.h"
#include "controls_overlay_widget.h"
#include "touch_hotspots_widget.h"
#include "vol_overlay_widget.h"
#include "vu_widget.h"
#include "animated_wheels_widget.h"
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
        case RADIO_STATE_FINISHED:  return "STOPPED";
        default:                    return "?";
    }
}

static void refresh_from_state(void)
{
    app_state_t *s = app_state_get();

    now_playing_widget_update();
    animated_wheels_widget_set_running(s->radio_state == RADIO_STATE_PLAYING);

    if (s_label_state)
        lv_label_set_text(s_label_state, radio_state_str(s->radio_state));

    if (s_label_audio_info) {
        char info[80];
        if (s->sample_rate > 0) {
            snprintf(info, sizeof(info), "%d Hz  %dch  %dkbps   VOL: %d%%",
                     s->sample_rate, s->channels, s->bitrate / 1000, s->volume);
        } else {
            snprintf(info, sizeof(info), "VOL: %d%%", s->volume);
        }
        lv_label_set_text(s_label_audio_info, info);
    }

    controls_overlay_refresh();   // keep center play/stop in sync with external changes
}

static void radio_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    if (p->radio_show_cassette &&
        (p->radio_show_wheel_left || p->radio_show_wheel_right)) {
        animated_wheels_widget_create(parent, (animated_wheels_style_t)p->radio_animation_style,
                                      p->radio_show_wheel_left,
                                      p->radio_cassette_l_x, p->radio_cassette_l_y, p->radio_cassette_l_size,
                                      p->radio_show_wheel_right,
                                      p->radio_cassette_r_x, p->radio_cassette_r_y, p->radio_cassette_r_size);
    }

    if (p->radio_show_np) {
        now_playing_widget_create(parent, p->radio_np_x, p->radio_np_y, LV_TEXT_ALIGN_CENTER,
                                  p->radio_np_station_font, p->radio_show_np_title,
                                  p->radio_np_title_font);
    }
    if (p->radio_show_station_icon) {
        station_icon_widget_create(parent,
                                   p->radio_station_icon_x, p->radio_station_icon_y,
                                   p->radio_station_icon_size);
    }
    if (p->radio_show_mode_indicator) {
        mode_indicator_create(parent, p->radio_mode_indic_x, p->radio_mode_indic_y);
    }
    if (p->radio_show_clock) {
        clock_widget_create(parent, p->radio_clock_widget_x, p->radio_clock_widget_y,
                            p->radio_clock_font, UI_ALIGN_CENTER);
    }
    if (p->radio_show_event_indicator) {
        event_indicator_create(parent, p->radio_event_indic_x, p->radio_event_indic_y);
    }
    if (p->radio_show_vu) {
        vu_widget_create(parent, p->radio_vu_x, p->radio_vu_y, p->radio_vu_w, p->radio_vu_h);
    }
    if (p->radio_show_weather) {
        weather_widget_create(parent, p->radio_weather_x, p->radio_weather_y,
                              p->radio_weather_w, p->radio_weather_font);
    }

    if (p->radio_show_playback_status) {
        // Screen-centered, content-hugging (so the label_bg plate tracks the
        // text, not the full width) — mirrors the home clock labels.
        s_label_state = ui_anchored_label(parent, DISPLAY_WIDTH / 2, p->radio_state_y,
                                          UI_ALIGN_CENTER);
        lv_label_set_text(s_label_state, "");
        lv_obj_set_style_text_font(s_label_state, p->radio_state_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_label_state, lv_color_hex(th->status_ok), LV_PART_MAIN);
        ui_label_scrim(s_label_state);

        s_label_audio_info = ui_anchored_label(parent, DISPLAY_WIDTH / 2,
                                               p->radio_audio_info_y, UI_ALIGN_CENTER);
        lv_label_set_text(s_label_audio_info, "");
        lv_obj_set_style_text_font(s_label_audio_info, p->radio_audio_info_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_label_audio_info, lv_color_hex(th->text_muted), LV_PART_MAIN);
        ui_label_scrim(s_label_audio_info);
    }

    refresh_from_state();

    controls_overlay_create(parent, CTRL_OVL_MODE_RADIO);
    touch_hotspots_widget_create(parent, CONTROL_SOURCE_RADIO, p->radio_touch_hotspots);

    ESP_LOGI(TAG, "Created (theme=%d)", theme_current());
}

static void radio_destroy(void)
{
    touch_hotspots_widget_destroy();
    controls_overlay_destroy();
    vol_overlay_hide();
    vu_widget_destroy();
    animated_wheels_widget_destroy();
    weather_widget_destroy();
    station_icon_widget_destroy();
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
            if (ui_profile_get()->radio_show_station_icon) {
                station_icon_widget_update();
            }
            mode_indicator_update();
            event_indicator_update();
            clock_widget_tick();
            break;
        case UI_EVT_TITLE_CHANGED:
            now_playing_widget_update();
            break;
        case UI_EVT_WEATHER_UPDATE:
            weather_widget_update();
            break;
        case UI_EVT_RADIO_STATE:
            animated_wheels_widget_set_running(ev->radio_state == RADIO_STATE_PLAYING);
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

            settings_set_volume(vol);   // → audio_engine + app_state + save
            vol_overlay_show(s_root, vol, true);
            break;
        }

        case UI_INPUT_ENCODER_PRESS:
        case UI_INPUT_SWIPE_RIGHT:
            ui_nav_ring_next(SCREEN_RADIO);
            break;
        case UI_INPUT_ENCODER_LONG_PRESS:
            screen_playlist_set_return(SCREEN_RADIO);
            ui_navigate(SCREEN_PLAYLIST);
            break;
        case UI_INPUT_SWIPE_LEFT:
            ui_nav_ring_prev(SCREEN_RADIO);
            break;
        case UI_INPUT_SWIPE_UP:
            screen_playlist_set_return(SCREEN_RADIO);
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

    if (s_label_state) {
        lv_obj_set_style_text_color(s_label_state,
            lv_color_hex(th->status_ok), LV_PART_MAIN);
        ui_label_scrim(s_label_state);
    }
    if (s_label_audio_info) {
        lv_obj_set_style_text_color(s_label_audio_info,
            lv_color_hex(th->text_muted), LV_PART_MAIN);
        ui_label_scrim(s_label_audio_info);
    }

    now_playing_widget_apply_theme();
    vu_widget_apply_theme();
    animated_wheels_widget_apply_theme();
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
