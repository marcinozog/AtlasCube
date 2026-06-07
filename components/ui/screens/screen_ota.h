#pragma once

#include "ui_screen.h"

// Full-screen firmware-update view. Shown while an OTA write is in progress
// (navigated to from the web OTA handler). Listens for UI_EVT_OTA_PROGRESS and
// renders a progress bar; on failure it auto-returns to the radio screen.
extern const ui_screen_t screen_ota;
