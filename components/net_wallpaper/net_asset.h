#pragma once

#include <stdbool.h>
#include "lvgl.h"
#include "settings.h"   // ASSET_SLOTS — the persisted slot count

#ifdef __cplusplus
extern "C" {
#endif

// Internet assets: small PNG artwork (slider knobs today) downloaded from a URL
// and decoded into PSRAM, the same way internet wallpapers work — and in the
// same batch, under the same display.wallpaper_fetch_mode. A ui_profile image
// field names a slot with "asset0".."asset3" instead of an SD path.
//
// PNG, not JPEG, deliberately: a knob needs an alpha channel and JPEG has none.
// Decoding uses LVGL's bundled lodepng (CONFIG_LV_USE_LODEPNG), which allocates
// through lv_malloc — so it MUST run on the LVGL task (LVGL's pool has no lock,
// CONFIG_LV_USE_OS=0). Hence the split below: the fetch task only downloads,
// net_asset_commit() decodes.
#define NET_ASSET_SLOTS  ASSET_SLOTS

// Largest source image accepted, in pixels. lodepng peaks at ~8 B per source
// pixel (inflated scanlines + ARGB8888 output, both live at once) inside LVGL's
// 256 KB pool — see CONFIG_LV_MEM_SIZE_KILOBYTES in sdkconfig.defaults. Raising
// this means raising that pool too.
#define NET_ASSET_MAX_PX (128 * 128)

// Download one slot's PNG and park the bytes for the next commit. Called from
// net_wallpaper's fetch task (never on its own task — assets ride the wallpaper
// batch so the radio is stopped once for both). Returns false on any failure,
// with the shared status line carrying the reason.
bool net_asset_fetch_slot(int slot, const char *url);

// How many slots have a URL configured. Used to size the batch's progress count
// and to decide whether a batch is worth running at all.
int net_asset_url_count(void);

// Decode everything a fetch left pending, replacing the committed images.
// LVGL TASK ONLY (lodepng → lv_malloc, and replaced images are dropped from the
// LVGL image cache). Returns true when at least one slot changed, i.e. when the
// caller has to rebuild the screen so widgets pick the new artwork up.
bool net_asset_commit(void);

// Committed artwork of `slot` as an RGB565A8 descriptor, or NULL when the slot
// holds none. LVGL TASK ONLY — a commit can replace the pixels underneath it.
// Widgets take their own scaled copy rather than referencing this directly.
const lv_image_dsc_t *net_asset_image(int slot);

// "Does this slot hold artwork?" — the pointer-free question, safe to ask from
// any task (the web UI marks filled slots with it).
bool net_asset_filled(int slot);

#ifdef __cplusplus
}
#endif
