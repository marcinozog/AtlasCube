// Auto-update: on-device firmware version check + pull-OTA.
//
// The implementation lives in a PREBUILT static library (lib/libupdater_impl.a,
// source kept private under TMP/private/updater/). This public component ships
// only the interface + the app_fw_variant() accessor the private lib reads at
// runtime — the prebuilt is compiled once and cannot bake a per-variant macro.
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called (from the updater task context) when a check finds a newer, non-skipped
// version. The callback must be cheap and thread-safe — the UI registers one that
// just posts a navigate to SCREEN_UPDATE. Registering here (rather than the lib
// calling ui_navigate itself) keeps the prebuilt free of any UI dependency, so
// the ui→updater dependency stays one-way (no circular REQUIRES).
typedef void (*updater_notify_cb_t)(void);

// Download progress during updater_apply(): pct 0..100, or -1 on failure. Same
// contract as UI_EVT_OTA_PROGRESS — the UI registers a cb that forwards it to
// SCREEN_OTA. Called from the OTA task, so keep it to a thread-safe queue post.
typedef void (*updater_progress_cb_t)(int pct);

// Compile-time HW variant string, equal to the CI release-asset key
// (e.g. "co5300-cst816d", "ili9341-xpt2046", bare "ssd1322"). Defined in the
// public build via add_compile_definitions(FW_VARIANT=...) in CMakeLists.txt;
// the private updater lib requests exactly this variant's .bin so it never
// flashes a foreign-variant image. Implemented in updater.c (public).
const char *app_fw_variant(void);

// Kick off a background firmware-version check (boot + periodic). Non-blocking:
// spawns its own task, must not touch LVGL — results reach the UI over UI_EVT_*.
// The caller passes the HW variant key (fw_variant, from app_fw_variant()); the
// lib caches it for the check + later download. Passing it in (rather than the
// lib calling app_fw_variant() itself) keeps the prebuilt free of cross-archive
// back-references, so it links without any --whole-archive. Implemented in the
// prebuilt lib.
void updater_start(const char *fw_variant);

// Register the "update available" notification (see updater_notify_cb_t). Call
// once at UI init, before updater_start(). Implemented in the prebuilt lib.
void updater_set_notify_cb(updater_notify_cb_t cb);

// Register the download-progress callback (see updater_progress_cb_t). Call once
// at UI init. Implemented in the prebuilt lib.
void updater_set_progress_cb(updater_progress_cb_t cb);

// Latest release tag reported by the last successful check ("" until then).
// For SCREEN_UPDATE to display. Implemented in the prebuilt lib.
const char *updater_latest_version(void);

// True if the last check found a strictly newer version. Implemented in the
// prebuilt lib.
bool updater_update_available(void);

// Download + flash the confirmed newer version (called from SCREEN_UPDATE on user
// confirm, after the caller navigated to SCREEN_OTA and stopped audio). Spawns an
// OTA task that pulls the app-only .bin over HTTPS via the atlascube.net proxy and
// reboots on success; progress arrives via the registered progress cb. Implemented
// in the prebuilt lib.
void updater_apply(void);

#ifdef __cplusplus
}
#endif
