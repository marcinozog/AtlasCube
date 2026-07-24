#pragma once
#include "ui_screen.h"
#include "ui_events.h"   // ui_screen_id_t

extern const ui_screen_t screen_equalizer;

// Where to navigate when the EQ screen is exited (swipe / long-press). Set by
// the opener before ui_navigate(SCREEN_EQ); defaults to SCREEN_SETTINGS (the
// menu entry). A touch hotspot on a source screen sets that source instead.
void screen_equalizer_set_return(ui_screen_id_t scr);
