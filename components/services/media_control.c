#include "media_control.h"
#include "app_state.h"
#include "settings.h"
#include "playlist.h"
#include "radio_service.h"
#include "sd_player.h"
#include "audio_engine.h"
#include "bt.h"

static int clamp_volume(int volume)
{
    if (volume < 0) return 0;
    if (volume > 100) return 100;
    return volume;
}

/*
media_source_current
*/
media_source_t media_source_current(void)
{
    // The mux stops the SD player when BT takes over, so an active SD player
    // always means SD is the source the user hears.
    if (sd_player_is_active()) return MEDIA_SOURCE_SD;
    if (app_state_get()->bt_enable) return MEDIA_SOURCE_BT;
    // A stopped-keep/paused SD session still owns the source ("stop" then
    // "play" resumes the track, not the radio); starting the radio drops it
    // via sd_player_forget(), so a kept queue is a truthful signal.
    if (sd_player_has_queue()) return MEDIA_SOURCE_SD;
    return MEDIA_SOURCE_RADIO;
}

/*
media_control_execute
*/
void media_control_execute(media_source_t source, media_action_t action)
{
    app_state_t *s = app_state_get();

    if (source == MEDIA_SOURCE_RADIO) {
        switch (action) {
            case MEDIA_ACTION_VOLUME_UP:
            case MEDIA_ACTION_VOLUME_DOWN:
                settings_set_volume(clamp_volume(s->volume +
                    (action == MEDIA_ACTION_VOLUME_UP ? 2 : -2)));
                break;
            case MEDIA_ACTION_PLAY:
                if (s->radio_state != RADIO_STATE_PLAYING)
                    radio_play_index(s->curr_index);
                break;
            case MEDIA_ACTION_PLAY_TOGGLE:
            case MEDIA_ACTION_PLAY_PAUSE:   // a stream can't pause; same as play/stop
                if (s->radio_state == RADIO_STATE_PLAYING) radio_stop();
                else radio_play_index(s->curr_index);
                break;
            case MEDIA_ACTION_NEXT: {
                int n = playlist_get_count();
                if (n > 0) radio_play_index((s->curr_index + 1) % n);
                break;
            }
            case MEDIA_ACTION_PREVIOUS: {
                int n = playlist_get_count();
                if (n > 0) radio_play_index((s->curr_index - 1 + n) % n);
                break;
            }
            case MEDIA_ACTION_STOP:
                radio_stop();
                break;
        }
        return;
    }

    if (source == MEDIA_SOURCE_BT) {
        switch (action) {
            case MEDIA_ACTION_VOLUME_UP:
            case MEDIA_ACTION_VOLUME_DOWN:
                settings_set_bt_volume(clamp_volume(s->bt_volume +
                    (action == MEDIA_ACTION_VOLUME_UP ? 2 : -2)));
                break;
            case MEDIA_ACTION_PLAY:
                bt_play();
                break;
            case MEDIA_ACTION_PLAY_TOGGLE:
            case MEDIA_ACTION_PLAY_PAUSE:   // AVRCP toggle is already play/pause
                s->bt_playing ? bt_pause() : bt_play();
                break;
            case MEDIA_ACTION_NEXT:     bt_next(); break;
            case MEDIA_ACTION_PREVIOUS: bt_prev(); break;
            case MEDIA_ACTION_STOP:     bt_pause(); break;
        }
        return;
    }

    switch (action) {
        case MEDIA_ACTION_VOLUME_UP:
        case MEDIA_ACTION_VOLUME_DOWN:
            settings_set_volume(clamp_volume(s->volume +
                (action == MEDIA_ACTION_VOLUME_UP ? 2 : -2)));
            break;
        case MEDIA_ACTION_PLAY:
            if (!sd_player_is_active()) sd_player_resume_current();
            else if (audio_engine_is_paused()) sd_player_toggle_pause();  // resume in place
            break;
        case MEDIA_ACTION_PLAY_TOGGLE:
            if (sd_player_is_active()) sd_player_stop_keep();
            else sd_player_resume_current();
            break;
        case MEDIA_ACTION_PLAY_PAUSE:
            // In-place pause/resume (keeps the pipeline and the position);
            // falls back to play when nothing is active, so the button also
            // works as "play" after a stop.
            if (sd_player_is_active()) sd_player_toggle_pause();
            else sd_player_resume_current();
            break;
        case MEDIA_ACTION_NEXT:     sd_player_next(); break;
        case MEDIA_ACTION_PREVIOUS: sd_player_prev(); break;
        case MEDIA_ACTION_STOP:     sd_player_stop_keep(); break;
    }
}
