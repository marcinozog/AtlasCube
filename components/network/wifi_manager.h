#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    WIFI_RUN_MODE_STA,  // connected to an external AP
    WIFI_RUN_MODE_AP,   // provisioning / fallback (192.168.4.1)
} wifi_run_mode_t;

/**
 * Initializes WiFi. If ssid is non-empty — attempts STA (timeout 15s),
 * falls back to AP on failure. If ssid is empty — goes straight to AP.
 * AP SSID: "AtlasCube", password: "99876543"
 * The function BLOCKS until the mode is settled.
 */
void            wifi_init(const char *ssid, const char *pass);
bool            wifi_is_connected(void);
wifi_run_mode_t wifi_get_run_mode(void);

const char *wifi_get_ap_ssid(void);
const char *wifi_get_ap_pass(void);

// ─────────────────────────────────────────────────────────────────────────────
// WiFi scan (used by the on-device AP setup screen).
// One scanned access point.
typedef struct {
    char   ssid[33];   // NUL-terminated, may be "" for hidden networks
    int8_t rssi;       // dBm (negative; closer to 0 = stronger)
    bool   secure;     // true if the AP requires a password (not WIFI_AUTH_OPEN)
} wifi_scan_ap_t;

#define WIFI_SCAN_MAX_AP  20

/**
 * Starts an asynchronous WiFi scan. In AP-only mode it transparently brings up
 * a STA interface (mode → APSTA; the AP stays online) so scanning is possible.
 * Returns immediately; when the scan completes a UI_EVT_WIFI_SCAN_DONE event is
 * posted and results become available via wifi_manager_scan_get(). Calling while
 * a scan is already in progress is a no-op.
 */
void wifi_manager_scan_start(void);

/** True while a scan started by wifi_manager_scan_start() is still running. */
bool wifi_manager_scan_busy(void);

/**
 * Registers a callback invoked (from the WiFi event task) when a scan finishes.
 * Kept as a callback so the network component stays independent of the UI layer;
 * the UI registers a thunk that posts UI_EVT_WIFI_SCAN_DONE. Pass NULL to clear.
 */
typedef void (*wifi_scan_done_cb_t)(void);
void wifi_manager_set_scan_done_cb(wifi_scan_done_cb_t cb);

/**
 * Copies up to max scan results (sorted by RSSI, SSID-deduplicated) into out.
 * Returns the number of entries written. Safe to call from the UI task after
 * UI_EVT_WIFI_SCAN_DONE.
 */
int  wifi_manager_scan_get(wifi_scan_ap_t *out, int max);

/**
 * Writes the current IPv4 address into buf ("192.168.x.y"). In STA mode returns
 * the address leased by the router; in AP mode the fixed 192.168.4.1. Falls back
 * to "0.0.0.0" when no interface has an address yet. Returns buf.
 */
const char *wifi_get_ip(char *buf, size_t len);
