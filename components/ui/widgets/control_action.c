#include "control_action.h"
#include "app_state.h"
#include "settings.h"
#include "radio_service.h"
#include "sd_player.h"
#include "bt.h"
#include "ui_manager.h"
#include "screen_playlist.h"
#include "screen_sd_browser.h"

static int clamp_volume(int volume)
{
    if (volume < 0) return 0;
    if (volume > 100) return 100;
    return volume;
}

static ui_screen_id_t screen_for_source(control_source_t source)
{
    switch (source) {
        case CONTROL_SOURCE_BT: return SCREEN_BT;
        case CONTROL_SOURCE_SD: return SCREEN_SD;
        case CONTROL_SOURCE_RADIO:
        default: return SCREEN_RADIO;
    }
}

void control_action_execute(control_source_t source, control_action_t action)
{
    app_state_t *s = app_state_get();

    if (action == CONTROL_ACTION_OPEN_PLAYLIST) {
        screen_playlist_set_return(screen_for_source(source));
        ui_navigate(SCREEN_PLAYLIST);
        return;
    }

    if (action == CONTROL_ACTION_OPEN_SD_BROWSER) {
        screen_sd_browser_set_return(screen_for_source(source));
        ui_navigate(SCREEN_SD_BROWSER);
        return;
    }

    if (source == CONTROL_SOURCE_RADIO) {
        switch (action) {
            case CONTROL_ACTION_VOLUME_UP:
            case CONTROL_ACTION_VOLUME_DOWN:
                settings_set_volume(clamp_volume(s->volume +
                    (action == CONTROL_ACTION_VOLUME_UP ? 2 : -2)));
                break;
            case CONTROL_ACTION_PLAY_TOGGLE:
            case CONTROL_ACTION_PLAY_PAUSE:   // a stream can't pause; same as play/stop
                if (s->radio_state == RADIO_STATE_PLAYING) radio_stop();
                else radio_play_index(s->curr_index);
                break;
            case CONTROL_ACTION_NEXT:
                radio_play_index(s->curr_index + 1);
                break;
            case CONTROL_ACTION_PREVIOUS:
                radio_play_index(s->curr_index - 1);
                break;
            case CONTROL_ACTION_STOP:
                radio_stop();
                break;
            case CONTROL_ACTION_OPEN_PLAYLIST:
            case CONTROL_ACTION_OPEN_SD_BROWSER:
                break; // handled before the source-specific dispatcher
        }
        return;
    }

    if (source == CONTROL_SOURCE_BT) {
        switch (action) {
            case CONTROL_ACTION_VOLUME_UP:
            case CONTROL_ACTION_VOLUME_DOWN:
                settings_set_bt_volume(clamp_volume(s->bt_volume +
                    (action == CONTROL_ACTION_VOLUME_UP ? 2 : -2)));
                break;
            case CONTROL_ACTION_PLAY_TOGGLE:
            case CONTROL_ACTION_PLAY_PAUSE:   // AVRCP toggle is already play/pause
                s->bt_playing ? bt_pause() : bt_play();
                break;
            case CONTROL_ACTION_NEXT:     bt_next(); break;
            case CONTROL_ACTION_PREVIOUS: bt_prev(); break;
            case CONTROL_ACTION_STOP:     bt_pause(); break;
            case CONTROL_ACTION_OPEN_PLAYLIST:
            case CONTROL_ACTION_OPEN_SD_BROWSER:
                break; // handled before the source-specific dispatcher
        }
        return;
    }

    switch (action) {
        case CONTROL_ACTION_VOLUME_UP:
        case CONTROL_ACTION_VOLUME_DOWN:
            settings_set_volume(clamp_volume(s->volume +
                (action == CONTROL_ACTION_VOLUME_UP ? 2 : -2)));
            break;
        case CONTROL_ACTION_PLAY_TOGGLE:
            if (sd_player_is_active()) sd_player_stop_keep();
            else sd_player_resume_current();
            break;
        case CONTROL_ACTION_PLAY_PAUSE:
            // In-place pause/resume (keeps the pipeline and the position);
            // falls back to play when nothing is active, so the button also
            // works as "play" after a stop.
            if (sd_player_is_active()) sd_player_toggle_pause();
            else sd_player_resume_current();
            break;
        case CONTROL_ACTION_NEXT:     sd_player_next(); break;
        case CONTROL_ACTION_PREVIOUS: sd_player_prev(); break;
        case CONTROL_ACTION_STOP:     sd_player_stop_keep(); break;
        case CONTROL_ACTION_OPEN_PLAYLIST:
        case CONTROL_ACTION_OPEN_SD_BROWSER:
            break; // handled before the source-specific dispatcher
    }
}
