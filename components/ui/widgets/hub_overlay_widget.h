#pragma once
#include "lvgl.h"
#include "controls_overlay_widget.h"   // controls_overlay_mode_t (RADIO / BT / SD)

#ifdef __cplusplus
extern "C" {
#endif

// Touch overlay for the unified hub (screen_home). Like controls_overlay it is a
// tap-to-show, auto-hiding modal dim layer, but it carries two rows of buttons:
//
//   row 1 (transport, source-aware):  vol-  prev  play/stop  next  vol+
//   row 2 (actions / navigation):     source(BT)  playlist  sd  settings
//
// The transport row drives whichever audio source is active (same per-mode logic
// as controls_overlay). The action row jumps to the source lists: playlist (pick
// a station → radio), the SD browser (pick a file → SD) and the BT screen, plus
// settings — so all three sources stay reachable from one screen.
//
// Self-laying-out from DISPLAY_WIDTH/HEIGHT (no ui_profile fields), like
// controls_overlay. The owning screen passes the active source as `mode`.
void hub_overlay_create(lv_obj_t *parent, controls_overlay_mode_t mode);
void hub_overlay_destroy(void);

// Re-point the (already created) overlay at a different audio source and refresh
// the center play/stop symbol. No-op if not created.
void hub_overlay_set_mode(controls_overlay_mode_t mode);

// Re-evaluate the play/stop glyph from the current playback state (web, schedule,
// …). No-op if not created.
void hub_overlay_refresh(void);

#ifdef __cplusplus
}
#endif
