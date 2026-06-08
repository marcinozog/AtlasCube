#include "radio_service.h"
#include "audio_player.h"
#include "app_state.h"
#include "settings.h"
#include "playlist.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "RADIO_SERVICE";

// ── volume fade-in (night-mode wake) ────────────────────────────────────────
// A periodic esp_timer steps the live output volume (audio_player_set_volume,
// no SPIFFS write) from 0 toward the target, then persists the final value
// once via settings_set_volume(). The handle is created lazily and reused.
#define RAMP_STEP_MS 200

static esp_timer_handle_t s_ramp_timer;
static int s_ramp_target;
static int s_ramp_total;   // number of steps
static int s_ramp_idx;     // steps done

static void ramp_tick(void *arg)
{
    (void)arg;
    s_ramp_idx++;
    if (s_ramp_idx >= s_ramp_total) {
        esp_timer_stop(s_ramp_timer);
        settings_set_volume(s_ramp_target);   // persist final value + app_state, once
        return;
    }
    int vol = s_ramp_target * s_ramp_idx / s_ramp_total;   // 0 → target
    audio_player_set_volume(vol);
}



/*
void radio_service_init(void)
*/
void radio_service_init(void)
{
    app_state_update(&(app_state_patch_t){
        .has_radio = true,
        .radio_state = RADIO_STATE_STOPPED
    });
}


/*
void radio_play_url(const char *url)
*/
void radio_play_url(const char *url)
{
    if (!url) {
        ESP_LOGE(TAG, "URL is NULL");
        app_state_update(&(app_state_patch_t){
            .has_radio = true,
            .radio_state = RADIO_STATE_ERROR
        });
        return;
    }

    ESP_LOGI(TAG, "Play: %s", url);

    app_state_update(&(app_state_patch_t){
        .has_url = true,
        .url = url,
        .has_radio = true,
        .radio_state = RADIO_STATE_BUFFERING,
        .has_title = true,
        .title = ""
    });

    if(app_state_get()->bt_enable)
        settings_set_bt_enable(false);

    audio_player_play(url);

    app_state_update(&(app_state_patch_t){
        .has_radio = true,
        .radio_state = RADIO_STATE_PLAYING
    });

    settings_set_was_playing(true);   // persist for resume-on-boot (no-op if unchanged)
}

void radio_play_index(int index)
{
    const playlist_entry_t *entry = playlist_get(index);
    if (!entry) {
        ESP_LOGW("RADIO", "play_index: invalid index %d", index);
        return;
    }

    // Set station name and clear old ICY title before playback starts
    app_state_update(&(app_state_patch_t){
        .has_station_name = true,  .station_name = entry->name,
        .has_title        = true,  .title        = "",
        .has_url          = true,  .url          = entry->url,
    });

    settings_set_curr_index(index);   // save + update curr_index in state
    radio_play_url(entry->url);           // or audio_player_play(entry->url)
}


void radio_resync_curr_index(void)
{
    const char *url = app_state_get()->url;
    if (!url || !url[0]) return;          // nothing selected yet

    int n = playlist_get_count();
    for (int i = 0; i < n; i++) {
        const playlist_entry_t *e = playlist_get(i);
        if (e && strcmp(e->url, url) == 0) {
            if (i != app_state_get()->curr_index)
                settings_set_curr_index(i);   // persist + broadcast new position
            return;
        }
    }
    ESP_LOGW(TAG, "resync: current station no longer in playlist (curr_index left as-is)");
}


/*
void radio_stop(void)
*/
void radio_stop(void)
{
    ESP_LOGI(TAG, "Stop");

    if (s_ramp_timer) esp_timer_stop(s_ramp_timer);   // cancel a fade-in in progress

    audio_player_stop();

    app_state_update(&(app_state_patch_t){
        .has_radio = true,
        .radio_state = RADIO_STATE_STOPPED,
        .has_title = true,
        .title = ""
    });

    settings_set_was_playing(false);   // persist for resume-on-boot (no-op if unchanged)
}


/*
void radio_resume_on_boot(void)
*/
void radio_resume_on_boot(void)
{
    app_settings_t *s = settings_get();
    if (!s->playlist.resume_on_boot || !s->playlist.was_playing) return;

    ESP_LOGI(TAG, "Resume on boot: replaying station idx=%d", s->playlist.curr_index);
    radio_play_index(s->playlist.curr_index);
}


/*
radio_state_t radio_get_state(void)
*/
radio_state_t radio_get_state(void)
{
    return app_state_get()->radio_state;
}


/*
const char* radio_get_current_url(void)
*/
const char* radio_get_current_url(void)
{
    return app_state_get()->url;
}


void radio_volume_ramp_to(int target_pct, int duration_ms)
{
    if (target_pct < 0)   target_pct = 0;
    if (target_pct > 100) target_pct = 100;

    if (duration_ms <= 0) {            // no ramp requested → set directly
        settings_set_volume(target_pct);
        return;
    }

    if (!s_ramp_timer) {
        const esp_timer_create_args_t args = {
            .callback = ramp_tick,
            .name     = "vol_ramp",
        };
        if (esp_timer_create(&args, &s_ramp_timer) != ESP_OK) {
            settings_set_volume(target_pct);   // fallback: instant
            return;
        }
    }
    esp_timer_stop(s_ramp_timer);      // cancel any in-flight ramp

    s_ramp_target = target_pct;
    s_ramp_total  = duration_ms / RAMP_STEP_MS;
    if (s_ramp_total < 1) s_ramp_total = 1;
    s_ramp_idx    = 0;

    audio_player_set_volume(0);        // start quiet
    esp_timer_start_periodic(s_ramp_timer, RAMP_STEP_MS * 1000);
}