#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"
#include "settings.h"   // WALLPAPER_SLOTS — the persisted slot count

#ifdef __cplusplus
extern "C" {
#endif

// Internet wallpapers live in PSRAM, one per slot, and are lost on reboot (the
// scheduler re-fetches them after boot). A slot costs a panel-sized RGB565
// buffer only once something has been fetched into it. Screens pick a slot with
// the "net0".."net9" per-screen override; bare "net" means slot 0.
#define NET_WP_SLOTS  WALLPAPER_SLOTS

// Fetch `url` into `slot` and convert it on-device (vendored libjpeg: decode +
// aspect-preserving downscale + centered crop) into a panel-sized RGB565
// wallpaper. Runs asynchronously on its own task; a playing radio stream is
// stopped for the duration of the transfer (so only one TLS session's worth of
// internal RAM is in use at a time) and resumed afterwards. NASA APOD API URLs
// are resolved through their JSON envelope first; `{w}`/`{h}` placeholders in
// the URL are replaced with the panel size. Returns false on a bad slot or when
// a fetch is already running — poll net_wallpaper_status().
bool net_wallpaper_fetch(int slot, const char *url, int panel_w, int panel_h);

// Fetch every slot whose URL is set, in one pass. The radio is stopped ONCE for
// the whole batch rather than per slot, so a fully loaded device costs one silent
// window at boot instead of ten. A slot that fails does not stop the others;
// the batch reports failure at the end. Returns false when a fetch is already
// running or no slot has a URL. Progress via net_wallpaper_progress().
bool net_wallpaper_fetch_all(int panel_w, int panel_h);

// Last fetch outcome for the web UI: "idle", "busy", "ok" or an error message.
const char *net_wallpaper_status(void);

// Batch progress ("Updating wallpapers 2/5"): slots finished and slots planned
// for the fetch in flight. Both are 0 before the first fetch of a session.
void net_wallpaper_progress(int *done, int *total);

// Committed wallpaper of `slot`, or NULL when that slot holds none (or the
// fetch hasn't been adopted yet). Valid until the next net_wallpaper_commit().
const lv_image_dsc_t *net_wallpaper_image(int slot);

// True when any slot holds a committed image — the web UI uses it to tell
// whether an internet wallpaper is in play at all.
bool net_wallpaper_any_image(void);

// Adopt the buffers of finished fetches across all slots: drop the LVGL cache
// entry for each replaced image and free it. Call only from the LVGL task —
// ui_background does, on UI_EVT_BG_CHANGED. No-op for slots with nothing
// pending (or consumes a pending dismiss, see below).
void net_wallpaper_commit(void);

// Write `slot`'s wallpaper as an LVGL RGB565 .bin under
// /sdcard/wallpapers/<width>x<height>/internet/ (timestamped name), ready for the
// SD-wallpaper picker. Synchronous (~1 s of SD I/O) — call from the httpd task,
// not LVGL.
// Returns true and fills out_path; on failure sets *err to a short reason
// ("no SD card", "no wallpaper fetched", "fetch in progress", …).
bool net_wallpaper_save_to_sd(int slot, char *out_path, size_t out_cap, const char **err);

// Snapshot `slot` as a complete LVGL RGB565 .bin blob (header + pixels) in a
// fresh PSRAM allocation, ready to stream over HTTP — the web layout editor
// decodes it with the same lvbin.js path as SD wallpapers. Returns NULL when
// the slot is empty (or on OOM); the caller frees the blob with
// heap_caps_free().
uint8_t *net_wallpaper_bin_snapshot(int slot, size_t *out_len);

// Drop a fetched wallpaper so the configured background (gradient/solid/SD
// wallpaper) shows again — a net image otherwise outranks them until reboot.
// `slot` < 0 clears every slot. Safe from any task: only marks the request; the
// actual free happens on the LVGL task inside the next net_wallpaper_commit().
// The caller must trigger that commit by posting UI_EVT_BG_CHANGED (http_server
// does, on an explicit background choice).
void net_wallpaper_dismiss(int slot);

// Fired from the fetch task when a download+decode finished (ok) or failed. For
// a batch it fires once, at the end. The UI uses it to post UI_EVT_BG_CHANGED
// over to the LVGL task.
void net_wallpaper_set_done_cb(void (*cb)(bool ok));

// Fired from the fetch task right before the transfer starts (and before a
// playing radio stream is stopped) — the UI shows its "Updating wallpaper" pill
// from this, so the sudden silence is explained on screen.
void net_wallpaper_set_start_cb(void (*cb)(void));

#ifdef __cplusplus
}
#endif
