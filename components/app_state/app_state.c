#include "app_state.h"
#include "theme.h"
#include "esp_log.h"
#include <string.h>

static app_state_cb_t s_cbs[APP_STATE_MAX_SUBSCRIBERS];
static int s_cb_count = 0;

static app_state_t s_state;
// static app_state_cb_t s_cb = NULL;

void app_state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
}

app_state_t* app_state_get(void)
{
    return &s_state;
}

void app_state_subscribe(app_state_cb_t cb)
{
    if (s_cb_count < APP_STATE_MAX_SUBSCRIBERS) {
        s_cbs[s_cb_count++] = cb;
    } else {
        ESP_LOGE("STATE", "Too many subscribers!");
    }
}

static void notify(void)
{
    for (int i = 0; i < s_cb_count; i++) {
        if (s_cbs[i]) s_cbs[i]();
    }
}

void app_state_update(const app_state_patch_t *patch)
{
    if (!patch) return;

    if (patch->has_radio) {
        s_state.radio_state = patch->radio_state;
    }

    if (patch->has_bt_enable) {
        s_state.bt_enable = patch->bt_enable;
    }

    if (patch->has_bt_show_screen) {
        s_state.bt_show_screen = patch->bt_show_screen;
    }

    if (patch->has_bt_state) {
        s_state.bt_state = patch->bt_state;
    }

    if (patch->has_bt_volume) {
        s_state.bt_volume = patch->bt_volume;
    }

    if (patch->has_bt_title && patch->bt_title) {
        strncpy(s_state.bt_title, patch->bt_title, sizeof(s_state.bt_title) - 1);
        s_state.bt_title[sizeof(s_state.bt_title) - 1] = 0;
    }

    if (patch->has_bt_artist && patch->bt_artist) {
        strncpy(s_state.bt_artist, patch->bt_artist, sizeof(s_state.bt_artist) - 1);
        s_state.bt_artist[sizeof(s_state.bt_artist) - 1] = 0;
    }

    if (patch->has_bt_duration_ms) {
        s_state.bt_duration_ms = patch->bt_duration_ms;
    }

    if (patch->has_bt_position_s) {
        s_state.bt_position_s = patch->bt_position_s;
    }

    if (patch->has_volume) {
        s_state.volume = patch->volume;
    }

    if (patch->has_eq) {
        memcpy(s_state.eq, patch->eq, sizeof(s_state.eq));
    }

    if (patch->has_eq_enabled) {
        s_state.eq_enabled = patch->eq_enabled;
    }

    if (patch->has_url && patch->url) {
        strncpy(s_state.url, patch->url, sizeof(s_state.url) - 1);
        s_state.url[sizeof(s_state.url) - 1] = 0;
    }

    if (patch->has_station_name && patch->station_name) {
        strncpy(s_state.station_name, patch->station_name, sizeof(s_state.station_name) - 1);
        s_state.station_name[sizeof(s_state.station_name) - 1] = 0;
    }

    if (patch->has_title && patch->title) {
        strncpy(s_state.title, patch->title, sizeof(s_state.title) - 1);
        s_state.title[sizeof(s_state.title) - 1] = 0;
    }

    if(patch->has_curr_index) {
        s_state.curr_index = patch->curr_index;
    }

    if (patch->has_audio_info) {
        s_state.sample_rate = patch->sample_rate;
        s_state.bits = patch->bits;
        s_state.channels = patch->channels;
        s_state.bitrate = patch->bitrate;
        s_state.codec_fmt = patch->codec_fmt;
    }

    if (patch->has_time_synced) s_state.time_synced = patch->time_synced;

    if(patch->has_screen) s_state.screen = patch->screen;

    if(patch->has_display_brightness) s_state.brightness = patch->display_brightness;

    if (patch->has_theme) {
        s_state.theme = patch->theme;
        theme_set(patch->theme);
    }

    if (patch->has_scrsaver_delay)  s_state.scrsaver_delay  = patch->scrsaver_delay;
    if (patch->has_scrsaver_id)     s_state.scrsaver_id     = patch->scrsaver_id;

    notify();

    char buf[256];
    int n = 0;
    if (patch->has_radio)             n += snprintf(buf + n, sizeof(buf) - n, " radio=%d", s_state.radio_state);
    if (patch->has_screen)            n += snprintf(buf + n, sizeof(buf) - n, " screen=%d", s_state.screen);
    if (patch->has_display_brightness) n += snprintf(buf + n, sizeof(buf) - n, " brightness=%d", s_state.brightness);
    if (patch->has_bt_enable)         n += snprintf(buf + n, sizeof(buf) - n, " bt_enable=%d", s_state.bt_enable);
    if (patch->has_bt_state)          n += snprintf(buf + n, sizeof(buf) - n, " bt_connected=%d", s_state.bt_state);
    if (patch->has_bt_volume)         n += snprintf(buf + n, sizeof(buf) - n, " bt_volume=%d", s_state.bt_volume);
    if (patch->has_bt_title)          n += snprintf(buf + n, sizeof(buf) - n, " bt_title=%s", s_state.bt_title);
    if (patch->has_bt_artist)         n += snprintf(buf + n, sizeof(buf) - n, " bt_artist=%s", s_state.bt_artist);
    if (patch->has_bt_duration_ms)    n += snprintf(buf + n, sizeof(buf) - n, " bt_duration=%dms", s_state.bt_duration_ms);
    if (patch->has_bt_position_s)     n += snprintf(buf + n, sizeof(buf) - n, " bt_pos=%ds", s_state.bt_position_s);
    if (patch->has_volume)            n += snprintf(buf + n, sizeof(buf) - n, " volume=%d", s_state.volume);
    if (patch->has_eq)                n += snprintf(buf + n, sizeof(buf) - n, " eq=*");
    if (patch->has_eq_enabled)        n += snprintf(buf + n, sizeof(buf) - n, " eq_enabled=%d", s_state.eq_enabled);
    if (patch->has_curr_index)        n += snprintf(buf + n, sizeof(buf) - n, " curr_index=%d", s_state.curr_index);
    if (patch->has_url)               n += snprintf(buf + n, sizeof(buf) - n, " url=%s", s_state.url);
    if (patch->has_station_name)      n += snprintf(buf + n, sizeof(buf) - n, " station=%s", s_state.station_name);
    if (patch->has_title)             n += snprintf(buf + n, sizeof(buf) - n, " title=%s", s_state.title);
    if (patch->has_audio_info)        n += snprintf(buf + n, sizeof(buf) - n, " audio=%luHz/%db/%dch/%lubps/fmt%d",
                                                    (unsigned long)s_state.sample_rate, s_state.bits, s_state.channels,
                                                    (unsigned long)s_state.bitrate, s_state.codec_fmt);
    if (patch->has_time_synced)       n += snprintf(buf + n, sizeof(buf) - n, " time_synced=%d", s_state.time_synced);
    if (patch->has_theme)             n += snprintf(buf + n, sizeof(buf) - n, " theme=%d", s_state.theme);
    if (patch->has_scrsaver_delay)    n += snprintf(buf + n, sizeof(buf) - n, " scrsaver_delay=%d", s_state.scrsaver_delay);
    if (patch->has_scrsaver_id)       n += snprintf(buf + n, sizeof(buf) - n, " scrsaver_id=%d", s_state.scrsaver_id);

    if (n > 0) {
        ESP_LOGI("STATE", "Updated:%s", buf);
    }
}