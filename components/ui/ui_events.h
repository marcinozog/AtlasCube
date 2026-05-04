#pragma once

#include "radio_service.h"
#include <stdint.h>
#include <stdbool.h>

// --------------------------------------------------------------------------
// Screens
// --------------------------------------------------------------------------

typedef enum {
    SCREEN_SPLASH = 0,
    SCREEN_RADIO,
    SCREEN_PLAYLIST,
    SCREEN_CLOCK,
    SCREEN_WEATHER,
    SCREEN_BT,
    SCREEN_WIFI,
    SCREEN_SETTINGS,
    SCREEN_EQ,
    SCREEN_EVENTS,
    SCREEN_EVENT_NOTIFICATION,
    SCREEN_COUNT
} ui_screen_id_t;

// --------------------------------------------------------------------------
// Backend → UI events
// --------------------------------------------------------------------------

typedef enum {
    UI_EVT_STATE_CHANGED,    // refresh full state (app_state callback)
    UI_EVT_TITLE_CHANGED,    // new ICY title
    UI_EVT_VOLUME_CHANGED,   // volume change
    UI_EVT_RADIO_STATE,      // radio state change
    UI_EVT_CLOCK_TICK,       // every second
    UI_EVT_WEATHER_UPDATE,   // after fetching weather data
    UI_EVT_NAVIGATE,         // switch screen
    UI_EVT_THEME_CHANGED,
    UI_EVT_PROFILE_CHANGED,  // ui_profile (layout) changed → rebuild active screen
    UI_EVT_INPUT,            // event from encoder / buttons
    UI_EVT_EVENT_FIRED,      // events_service fired an event (fullscreen toast)
} ui_event_type_t;

// --------------------------------------------------------------------------
// Payload for UI_EVT_EVENT_FIRED
// --------------------------------------------------------------------------

typedef struct {
    char  id[9];            // matches EVENT_ID_LEN
    char  title[64];
    char  type_label[16];   // e.g. "BIRTHDAY", "REMINDER" — ready to display
    int   hour;
    int   minute;
} ui_event_info_t;

// --------------------------------------------------------------------------
// Physical input (encoder / buttons)
// --------------------------------------------------------------------------

typedef enum {
    UI_INPUT_NONE = 0,
    UI_INPUT_ENCODER_CW,    // clockwise rotation
    UI_INPUT_ENCODER_CCW,   // counter-clockwise rotation
    UI_INPUT_ENCODER_PRESS, // encoder press
    UI_INPUT_ENCODER_LONG_PRESS,
    UI_INPUT_BTN_PREV,
    UI_INPUT_BTN_NEXT,
    UI_INPUT_BTN_OK,
    UI_INPUT_BTN_BACK,
} ui_input_t;

typedef struct {
    ui_event_type_t type;
    union {
        int volume;                // UI_EVT_VOLUME_CHANGED
        char title[128];           // UI_EVT_TITLE_CHANGED
        radio_state_t radio_state; // UI_EVT_RADIO_STATE
        ui_screen_id_t screen_id;  // UI_EVT_NAVIGATE
        // UI_EVT_CLOCK_TICK, UI_EVT_STATE_CHANGED: no payload
        // UI_EVT_WEATHER_UPDATE: extend once a weather data model exists
        ui_input_t input;          // UI_EVT_INPUT
        ui_event_info_t event_info;// UI_EVT_EVENT_FIRED
    };
} ui_event_t;

