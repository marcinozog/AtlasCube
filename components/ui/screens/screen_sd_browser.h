#pragma once
#include "ui_screen.h"

extern const ui_screen_t screen_sd_browser;

// Where to navigate after a track is picked (or the browser is exited). Set by the
// opener before ui_navigate(SCREEN_SD_BROWSER); defaults to SCREEN_SD. The hub
// (screen_home) sets SCREEN_HOME so picking a track returns to the hub instead of
// jumping to the SD player screen (which may be disabled).
void screen_sd_browser_set_return(ui_screen_id_t scr);
