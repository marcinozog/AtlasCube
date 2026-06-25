#pragma once
#include "ui_screen.h"

// Unified "home" hub: a clock face (time / date / now-playing strip) plus the
// hub_overlay (tap-to-show) whose buttons drive the active source and jump to the
// playlist / SD browser / BT / settings. Reuses the clock_* layout fields from
// ui_profile (it superseded the old clock screen), so it needs none of its own.
extern const ui_screen_t screen_home;
