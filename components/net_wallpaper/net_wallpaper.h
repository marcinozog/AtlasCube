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
// does, on UI_EVT_BG_CHANGED. No-op when nothing is pending.
void net_wallpaper_commit(void);

// Fired from the fetch task when a download+decode finished (ok) or failed.
// The UI uses it to post UI_EVT_BG_CHANGED over to the LVGL task.
void net_wallpaper_set_done_cb(void (*cb)(bool ok));

#ifdef __cplusplus
}
#endif
