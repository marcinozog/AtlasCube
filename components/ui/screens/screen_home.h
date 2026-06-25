#pragma once
#include "ui_screen.h"

// Unified "home" hub: a clock face (time / date / now-playing strip) plus the
// hub_overlay (tap-to-show) whose buttons drive the active source and jump to the
// playlist / SD browser / BT / settings. Reuses the screen_clock layout fields
// from ui_profile (clock_*), so it needs no profile fields of its own.
extern const ui_screen_t screen_home;
