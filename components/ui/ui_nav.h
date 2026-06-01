#pragma once

#include "ui_events.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// Home-screen navigation ring
// --------------------------------------------------------------------------
//
// The set of "home" screens the user cycles through with a swipe or a short
// encoder press. The order and visibility live in a single table in ui_nav.c
// (s_ring) — edit that table to add/remove/reorder home screens. Hidden
// entries (e.g. BT without bt_show_screen, MQTT without enabled) are skipped.
//
// Sub-screens (playlist, settings, eq, events) are NOT part of the ring; they
// keep their own parent/back navigation inside their on_input handlers.

/** Navigate to the next visible ring screen after `from` (persists via
 *  settings_set_screen). No-op if `from` is not a ring member. Wraps around. */
void ui_nav_ring_next(ui_screen_id_t from);

/** Navigate to the previous visible ring screen before `from`. Wraps around. */
void ui_nav_ring_prev(ui_screen_id_t from);

/** True if `id` is a ring member and currently visible. */
bool ui_nav_is_ring(ui_screen_id_t id);

#ifdef __cplusplus
}
#endif
