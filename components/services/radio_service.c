#include "radio_service.h"
#include "audio_engine.h"
#include "audio_net_player.h"
#include "audio_file_player.h"
#include "sd_player.h"
#include "app_state.h"
#include "settings.h"
#include "bt.h"
#include "playlist.h"
#include "sdcard.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "RADIO_SERVICE";

// ── volume fade-in (night-mode wake) ────────────────────────────────────────
// A periodic esp_timer steps the live output volume (audio_engine_set_volume,
// no SPIFFS write) from 0 toward the target, then persists the final value
// once via settings_set_volume(). The handle is created lazily and reused.
#define RAMP_STEP_MS 200

static esp_timer_handle_t s_ramp_timer;
static int s_ramp_target;
static int s_ramp_total;   // number of steps
static int s_ramp_idx;     // steps done

// ── voice notification (interrupt + restore) ────────────────────────────────
// State captured when a voice notification starts, used by
// on_notification_finished() to restore the previous audio source.
static bool s_notif_active   = false;
static bool s_notif_was_radio = false;
static bool s_notif_was_bt    = false;
static int  s_notif_prev_index  = 0;
static int  s_notif_prev_volume = 0;

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
    audio_engine_set_volume(vol);
}



/*
static void on_bt_play_event(bool playing)
Runs on the BT UART RX task on a phone play-state change. We only switch the
source on the *rising edge* (not playing → playing): the module re-emits BT_PA
repeatedly while playing, and reacting to every one would constantly override a
manual "switch back to radio". The edge is reset on pause/stop/disconnect, so a
genuine new playback start switches again.

Must stay lightweight: no flash I/O, no blocking — the UART task has a small
stack and stalling it drops incoming metadata. The source switch is runtime
only (no SPIFFS persist — a transient phone-play shouldn't rewrite the saved
source), and the teardown is deferred to audio_play_task.
*/
static void on_bt_play_event(bool playing)
{
    static bool s_bt_playing = false;

    bool was = s_bt_playing;
    s_bt_playing = playing;

    if (!playing || was) return;        // only the not-playing → playing edge acts

    app_state_t *s = app_state_get();
    if (!s->bt_auto_switch) return;

    // Switch source to BT. The volatile variant keeps s_settings/app_state/GPIO
    // in sync (so radio_play_url can later switch back) without a flash write.
    settings_set_bt_enable_volatile(true);

    if (sd_player_is_active()) sd_player_stop();    // BT supersedes SD music

    if (s->radio_state == RADIO_STATE_PLAYING ||
        s->radio_state == RADIO_STATE_BUFFERING) {
        audio_engine_request_stop();                // async teardown on audio task
        app_state_update(&(app_state_patch_t){
            .has_radio = true, .radio_state = RADIO_STATE_STOPPED,
            .has_title = true, .title = ""
        });
    }
}


/*
void radio_service_init(void)
*/
// Runs on the audio task when a voice-notification WAV reaches its end.
// Restores whatever was playing before the notification interrupted it.
static void on_notification_finished(void)
{
    if (!s_notif_active) return;
    s_notif_active = false;

    ESP_LOGI(TAG, "Notification done → restore (radio=%d, bt=%d, idx=%d, vol=%d)",
             s_notif_was_radio, s_notif_was_bt, s_notif_prev_index, s_notif_prev_volume);

    audio_engine_set_volume(s_notif_prev_volume);   // undo the notification level

    if (s_notif_was_bt) {
        settings_set_bt_enable(true);    // mux back to the BT module
        bt_play();                       // resume phone playback
    } else if (s_notif_was_radio) {
        radio_play_index(s_notif_prev_index);
    }
    // else: radio was stopped → stay stopped
}


// Single file-finished dispatcher (audio_file_player has one callback). A voice
// notification restoring the previous source takes priority; otherwise, if the
// SD music player owns the file, advance its queue.
static void on_file_finished(void)
{
    if (s_notif_active) {
        on_notification_finished();
        return;
    }
    if (sd_player_is_active()) {
        sd_player_on_track_end();
        return;
    }
}


void radio_service_init(void)
{
    bt_set_play_event_cb(on_bt_play_event);
    audio_file_player_set_finished_cb(on_file_finished);

    app_state_update(&(app_state_patch_t){
        .has_radio = true,
        .radio_state = RADIO_STATE_STOPPED
    });
}


/*
void radio_play_notification(const char *filename, int volume)
*/
void radio_play_notification(const char *filename, int volume)
{
    if (!filename || !filename[0]) {
        ESP_LOGW(TAG, "notification: empty filename");
        return;
    }

    char path[128];
    snprintf(path, sizeof(path), "%s/voice/%s", SD_MOUNT_POINT, filename);

    app_state_t *s = app_state_get();
    s_notif_was_bt     = s->bt_enable;
    s_notif_was_radio  = (s->radio_state == RADIO_STATE_PLAYING ||
                          s->radio_state == RADIO_STATE_BUFFERING);
    s_notif_prev_index  = s->curr_index;
    s_notif_prev_volume = s->volume;
    s_notif_active = true;

    ESP_LOGI(TAG, "Voice notification: %s (vol=%d, was_radio=%d, was_bt=%d)",
             path, volume, s_notif_was_radio, s_notif_was_bt);

    if (volume >= 0) audio_engine_set_volume(volume);   // live only, not persisted

    // Hand the I2S output to the ESP side: pause + un-mux the BT module if it
    // was the active source. (Radio teardown is handled by audio_file_player_play.)
    if (s_notif_was_bt) {
        if (s->bt_auto_switch) bt_pause();
        settings_set_bt_enable(false);
    }

    audio_file_player_play(path);
}


/*
void radio_play_file(const char *path)
Test hook for SD music playback: plays a local audio file over I2S, taking the
output from BT if that was the active source. No playlist/queue or radio_state
integration yet — when the file ends the pipeline just stops.
*/
void radio_play_file(const char *path)
{
    if (!path || !path[0]) {
        ESP_LOGW(TAG, "play_file: empty path");
        return;
    }

    ESP_LOGI(TAG, "Play file: %s", path);

    // Take the I2S output from the BT module if it was the active source
    // (same handover as radio_play_url).
    if (app_state_get()->bt_enable) {
        if (app_state_get()->bt_auto_switch) bt_pause();
        settings_set_bt_enable(false);
    }

    audio_file_player_play(path);
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

    // Only act on BT when it is actually the active source — otherwise an
    // always-on radio would blast spurious AT+PU at the module on every
    // (re)connect, disrupting an idle phone session. Taking over is an explicit
    // action, so pause the phone unconditionally — it should actually stop, not
    // keep playing muted behind the hardware mux (same as the SD takeover).
    if (app_state_get()->bt_enable) {
        bt_pause();
        settings_set_bt_enable(false);
    }

    if (sd_player_is_active()) sd_player_stop();   // radio supersedes SD music

    audio_net_player_play(url);

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
    radio_play_url(entry->url);           // or audio_net_player_play(entry->url)
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

    // Generic stop: also clear the SD player if it was the active source
    // (clears its state; the engine teardown below is shared).
    if (sd_player_is_active()) sd_player_stop();

    audio_engine_stop();

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

    audio_engine_set_volume(0);        // start quiet
    esp_timer_start_periodic(s_ramp_timer, RAMP_STEP_MS * 1000);
}