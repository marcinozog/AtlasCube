#pragma once
#include "ui_screen.h"

extern const ui_screen_t screen_settings;

// Where to navigate when settings is exited. Set by the opener before
// ui_navigate(SCREEN_SETTINGS); defaults to SCREEN_CLOCK. The hub (screen_home)
// sets SCREEN_HOME so leaving settings returns to the hub. Sub-screens that bounce
// back into settings (EQ, events) must NOT call this — it stays at the opener's
// value across those round-trips.
void screen_settings_set_return(ui_screen_id_t scr);