#include "control_action.h"
#include "media_control.h"
#include "ui_manager.h"
#include "screen_playlist.h"
#include "screen_sd_browser.h"

static ui_screen_id_t screen_for_source(control_source_t source)
{
    switch (source) {
        case CONTROL_SOURCE_BT: return SCREEN_BT;
        case CONTROL_SOURCE_SD: return SCREEN_SD;
        case CONTROL_SOURCE_RADIO:
        default: return SCREEN_RADIO;
    }
}

static media_source_t to_media_source(control_source_t source)
{
    switch (source) {
        case CONTROL_SOURCE_BT: return MEDIA_SOURCE_BT;
        case CONTROL_SOURCE_SD: return MEDIA_SOURCE_SD;
        case CONTROL_SOURCE_RADIO:
        default: return MEDIA_SOURCE_RADIO;
    }
}

static media_action_t to_media_action(control_action_t action)
{
    switch (action) {
        case CONTROL_ACTION_PREVIOUS:    return MEDIA_ACTION_PREVIOUS;
        case CONTROL_ACTION_NEXT:        return MEDIA_ACTION_NEXT;
        case CONTROL_ACTION_VOLUME_DOWN: return MEDIA_ACTION_VOLUME_DOWN;
        case CONTROL_ACTION_VOLUME_UP:   return MEDIA_ACTION_VOLUME_UP;
        case CONTROL_ACTION_STOP:        return MEDIA_ACTION_STOP;
        case CONTROL_ACTION_PLAY_PAUSE:  return MEDIA_ACTION_PLAY_PAUSE;
        case CONTROL_ACTION_PLAY_TOGGLE:
        default:                         return MEDIA_ACTION_PLAY_TOGGLE;
    }
}

void control_action_execute(control_source_t source, control_action_t action)
{
    // Navigation actions are UI-only; transport goes through the shared
    // source-aware dispatcher in services/media_control.
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

    media_control_execute(to_media_source(source), to_media_action(action));
}
