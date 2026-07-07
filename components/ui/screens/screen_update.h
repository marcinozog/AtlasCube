#pragma once

#include "ui_screen.h"

// Transient "new firmware available" prompt. Shown when the updater's boot check
// finds a newer, non-skipped release (updater registers a notify callback that
// navigates here). Offers Update / Later / Skip. Outside the normal nav ring,
// like SCREEN_OTA.
extern const ui_screen_t screen_update;
