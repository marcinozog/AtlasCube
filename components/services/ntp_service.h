#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * Initializes the SNTP client with hardcoded defaults.
 * Call after WiFi connection is established.
 * After sync, sends UI_EVT_STATE_CHANGED via ui_event_send().
 *
 * Tip: if you want to use settings from settings_get(), call
 *      ntp_service_reconfigure() right after ntp_service_init().
 */
void ntp_service_init(void);

/**
 * Stops SNTP, updates servers and timezone, restarts it.
 * Safe to call at runtime (e.g. from an HTTP handler after settings change).
 * Resets the time_synced flag.
 */
void ntp_service_reconfigure(const char *server1, const char *server2, const char *tz);

/** Returns true if the time is synchronized. */
bool ntp_service_is_synced(void);

#ifdef __cplusplus
}
#endif