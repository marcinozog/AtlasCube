#pragma once

#include "ui_screen.h"

extern const ui_screen_t screensaver_dashboard;

// Notify the dashboard screensaver that its persisted config changed
// (URL, JSON path, thresholds, etc.). If the screensaver is currently
// active, its fetcher reloads the snapshot before the next poll and
// triggers an immediate refetch. Safe to call when inactive — flag is
// cleared at next create().
void screensaver_dashboard_settings_changed(void);
