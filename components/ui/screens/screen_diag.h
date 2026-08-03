#pragma once

#include "ui_screen.h"

// Read-only diagnostics: firmware identity, PSRAM/internal heap, chip, link and
// SD state. Opened from Settings → System → Diagnostics; it replaced the single
// status line that used to sit in the System section header, which had no room
// for the version and low-water-mark figures a bug report actually needs.
// Outside the nav ring (like SCREEN_EQ from the menu) — any press or swipe
// returns to Settings. The same snapshot is served as JSON by GET /api/diag.
extern const ui_screen_t screen_diag;
