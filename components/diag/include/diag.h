#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// One-shot snapshot of the numbers a bug report needs: firmware identity, both
// heaps, and the link/storage state. The on-device Diagnostics screen and
// GET /api/diag both render this same struct, so a photo of the screen and a
// JSON dump can never disagree.
//
// Deliberately knows nothing about the UI: the panel geometry lives in
// ui_profile.h (component `ui`, which depends on this one), so the two callers
// fill that in themselves.
typedef struct {
    /* ── firmware ── */
    char        fw_version[48];     // git describe, as shown on the splash
    char        fw_build[40];       // "MMM DD YYYY HH:MM:SS" from the app descriptor
    char        idf_version[32];
    const char *fw_variant;         // display-touch key the updater asks releases for
    bool        www_outdated;       // boot check found web-UI files older than the app
    bool        update_available;   // a newer release was seen
    char        update_latest[32];  // its tag, "" when none

    /* ── memory ── */
    // PSRAM totals are what the heap owns, which is slightly under the physical
    // chip size: EXT_RAM_BSS_ATTR statics (LVGL's 64 KB pool among them) are
    // carved out before the allocator sees the region.
    size_t   psram_total, psram_free, psram_min_free;
    size_t   int_total, int_free, int_min_free, int_largest;

    /* ── hardware ── */
    char     chip[16];
    int      chip_rev, chip_cores;
    uint32_t flash_size;
    // Die temperature in tenths of a degree Celsius — the whole struct stays
    // float-free because LVGL's sprintf has no %f. Reads well above ambient
    // (typically +15..25 °C with the radio playing), so it is a trend to watch,
    // not a room thermometer. False when diag_init() never ran or the sensor
    // failed to install.
    bool     temp_valid;
    int      temp_c10;

    /* ── link ── */
    bool     wifi_connected;
    char     ssid[33];              // "" when not associated
    char     ip[16];
    int      rssi;                  // dBm, 0 when not associated
    char     mac[18];

    /* ── storage ── */
    // Reports the card only when it is already mounted — collecting diagnostics
    // must never trigger sdcard_init()'s lazy mount as a side effect.
    bool     sd_mounted;
    uint64_t sd_total, sd_free;

    uint32_t uptime_s;
} diag_info_t;

// Installs and enables the chip's temperature sensor. Call once from app_main:
// diag_collect() runs from both the Diagnostics screen's LVGL timer and the
// /api/diag handler, so a lazy first-use install would be a race between tasks.
// Everything else in the snapshot works without it — a device that skips this
// simply reports temp_valid = false.
void diag_init(void);

// Fill `out` with a fresh snapshot. Safe from any task.
void diag_collect(diag_info_t *out);

// Per-core CPU load is a delta measurement, so each caller keeps its own
// baseline: the screen polling at 1 Hz and a browser polling /api/diag would
// otherwise consume each other's reference point. Zero-initialise once; the
// first call reports 0/0 and only establishes the baseline.
typedef struct {
    uint32_t last_idle[2];
    uint32_t last_total;
} diag_cpu_state_t;

// Percentages cover the interval since this state's previous call.
// Needs CONFIG_FREERTOS_USE_TRACE_FACILITY + GENERATE_RUN_TIME_STATS.
void diag_cpu_usage(diag_cpu_state_t *st, int *cpu0, int *cpu1);
