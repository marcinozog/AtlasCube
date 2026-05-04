#pragma once
#include <stdbool.h>

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
