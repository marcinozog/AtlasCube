#pragma once

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Fetch an image from `url` and convert it on-device (esp_new_jpeg: decode +
// aspect-preserving downscale + centered crop) into a panel-sized RGB565
// wallpaper held in PSRAM — nothing is written to SD, the image is lost on
// reboot. Runs asynchronously on its own task; a playing radio stream is
// stopped for the duration of the transfer (so only one TLS session's worth of
// internal RAM is in use at a time) and resumed afterwards. NASA APOD API URLs
// are resolved through their JSON envelope first; `{w}`/`{h}` placeholders in
// the URL are replaced with the panel size. Returns false when a fetch is
// already running (or the task can't start) — poll net_wallpaper_status().
bool net_wallpaper_fetch(const char *url, int panel_w, int panel_h);

// Last fetch outcome for the web UI: "idle", "busy", "ok" or an error message.
const char *net_wallpaper_status(void);

// Committed wallpaper image, or NULL when none has been fetched (or the fetch
// hasn't been adopted yet). Valid until the next net_wallpaper_commit().
const lv_image_dsc_t *net_wallpaper_image(void);

// Adopt the buffer of a finished fetch: drop the LVGL cache entry for the
// previous image and free it. Call only from the LVGL task — ui_background
// does, on UI_EVT_BG_CHANGED. No-op when nothing is pending (or consumes a
// pending dismiss, see below).
void net_wallpaper_commit(void);

// Write the currently fetched wallpaper as an LVGL RGB565 .bin under
// /sdcard/wallpapers/saved/ (timestamped name), ready for the SD-wallpaper
// picker. Synchronous (~1 s of SD I/O) — call from the httpd task, not LVGL.
// Returns true and fills out_path; on failure sets *err to a short reason
// ("no SD card", "no wallpaper fetched", "fetch in progress", …).
bool net_wallpaper_save_to_sd(char *out_path, size_t out_cap, const char **err);

// Drop the fetched wallpaper so the configured background (gradient/solid/SD
// wallpaper) shows again — the net image otherwise outranks them until reboot.
// Safe from any task: only marks the request; the actual free happens on the
// LVGL task inside the next net_wallpaper_commit(). The caller must trigger
// that commit by posting UI_EVT_BG_CHANGED (http_server does, on an explicit
// background choice).
void net_wallpaper_dismiss(void);

// Fired from the fetch task when a download+decode finished (ok) or failed.
// The UI uses it to post UI_EVT_BG_CHANGED over to the LVGL task.
void net_wallpaper_set_done_cb(void (*cb)(bool ok));

// Fired from the fetch task right before the transfer starts (and before a
// playing radio stream is stopped) — the UI shows its "Updating wallpaper"
// pill from this, so the sudden silence is explained on screen.
void net_wallpaper_set_start_cb(void (*cb)(void));

#ifdef __cplusplus
}
#endif
