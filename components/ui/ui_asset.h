#pragma once

#include <stdbool.h>
#include "lvgl.h"
#include "ui_events.h"   // ui_screen_id_t

#ifdef __cplusplus
extern "C" {
#endif

// Resolves the image references that ui_profile fields carry (knob artwork
// today). A reference is either
//   "asset0".."asset3" → an internet asset slot, fetched into PSRAM at boot
//                        alongside the wallpapers (net_asset)
//   "<path>"           → an RGB565 .bin on SD, the original spelling
// so a layout can point at downloaded artwork on a device with no SD card, and
// existing layouts keep working unchanged.

// True when `ref` names an internet asset slot rather than a file path. Lets
// callers skip SD-specific work (mounting, existence checks) for slot refs.
bool ui_asset_is_slot(const char *ref);

// Load what `ref` points at, at its native size, into a descriptor the CALLER
// owns and frees with lv_bin_image_free(). An asset slot is copied rather than
// referenced: the slot's pixels are shared by every widget bound to it and are
// replaced under them by the next fetch. Returns NULL when the reference is
// empty, the file is missing, or the slot has not been fetched (yet) — callers
// fall back to their plain themed drawing.
lv_image_dsc_t *ui_asset_load(const char *ref);

// Does `screen`'s current layout reference an asset slot anywhere? Artwork only
// reaches a widget through create(), so a finished fetch has to rebuild the
// screen — but only a screen that actually reads a slot. Asking the profile
// rather than keeping a list of "screens with knobs" here means this stays true
// as more fields learn to take an asset reference, and it keeps screens that own
// transient state (splash, OTA progress) from being rebuilt under a download.
bool ui_asset_screen_uses_slots(ui_screen_id_t screen);

#ifdef __cplusplus
}
#endif
