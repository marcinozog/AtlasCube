#pragma once
#include "ui_screen.h"

extern const ui_screen_t screen_playlist;

// Where to navigate after a station is picked (or the list is exited). Set by the
// opener before ui_navigate(SCREEN_PLAYLIST); defaults to SCREEN_RADIO. The hub
// (screen_home) sets SCREEN_HOME so picking a station returns to the hub instead
// of jumping to the radio screen (which may be disabled).
void screen_playlist_set_return(ui_screen_id_t scr);