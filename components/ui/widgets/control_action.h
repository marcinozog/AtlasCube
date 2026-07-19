#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONTROL_ACTION_PLAY_TOGGLE = 0,
    CONTROL_ACTION_PREVIOUS,
    CONTROL_ACTION_NEXT,
    CONTROL_ACTION_VOLUME_DOWN,
    CONTROL_ACTION_VOLUME_UP,
    CONTROL_ACTION_STOP,
    CONTROL_ACTION_PLAY_PAUSE,
    CONTROL_ACTION_OPEN_PLAYLIST,
    CONTROL_ACTION_OPEN_SD_BROWSER,
} control_action_t;

typedef enum {
    CONTROL_SOURCE_RADIO = 0,
    CONTROL_SOURCE_BT,
    CONTROL_SOURCE_SD,
} control_source_t;

// Transport dispatcher for layout-defined invisible touch hotspots. It uses
// the same service entry points and source-specific semantics as the overlay.
void control_action_execute(control_source_t source, control_action_t action);

#ifdef __cplusplus
}
#endif
