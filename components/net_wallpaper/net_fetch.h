#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Internal to the net_wallpaper component: the HTTP getter and the single
// status line shared by both fetchers. Wallpapers and assets are downloaded by
// ONE task, in one batch, inside one radio-stop window — so they also report
// through one status string (/api/wallpaper/status). Not part of the component's
// public API; net_wallpaper.h / net_asset.h are.

// GET `url` into a fresh PSRAM buffer (caller frees with heap_caps_free),
// following up to 3 redirects manually — the open/fetch_headers flow doesn't
// auto-follow, and both picsum and the NASA image hosts answer with 30x. A body
// that fills the whole cap is treated as too large. Returns NULL with the status
// line set on error.
uint8_t *net_fetch_download(const char *url, size_t cap, int *out_len);

// Set the status line to a formatted error and log it as a warning.
void net_fetch_set_err(const char *fmt, ...);

// Set the status line to a literal that outlives the call ("idle"/"busy"/"ok").
void net_fetch_set_status(const char *literal);

// Current status line: "idle", "busy", "ok" or the last error message.
const char *net_fetch_status(void);

#ifdef __cplusplus
}
#endif
