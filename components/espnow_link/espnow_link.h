#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESPNOW_MAC_LEN 6

/** Longest command echoed back in the status snapshot, NUL included. */
#define ESPNOW_LAST_CMD_MAX 24

/** `last_seen_s` when no frame has ever been accepted. */
#define ESPNOW_NEVER UINT32_MAX

/**
 * ESP-NOW control link for the hardware remote (AtlasCubeController).
 * Wire contract: docs/espnow_link.md. Command vocabulary: docs/ws_protocol.md —
 * frames land in media_command_execute_text(), the same dispatcher the WS
 * server uses, so the two transports cannot drift apart.
 *
 * Runs alongside the STA interface on the channel the router imposed; in AP
 * fallback mode it binds to the AP interface instead.
 *
 * Compiled out entirely without HAS_ESPNOW_REMOTE (defines.h): every function
 * below becomes a stub, so callers need no #ifdef of their own — they read
 * `supported` from the status snapshot.
 */

/**
 * Brings the link up if — and only if — a remote is already paired. On a radio
 * that has never seen one this allocates nothing and starts no task; the link
 * comes up later from espnow_link_pair_window_open(), which is always a
 * deliberate human action.
 *
 * Must be called AFTER wifi_init() has settled the mode — ESP-NOW needs a
 * started WiFi interface. Safe to call twice (second call is a no-op).
 */
esp_err_t espnow_link_init(void);

/**
 * Re-adds the paired peer against whichever WiFi interface is up now. Call
 * after a run-mode change (AP↔STA) — an ESP-NOW peer remembers the interface it
 * was added on, so one that outlived its interface stops delivering silently.
 * No-op when the link isn't running or no remote is paired.
 */
void espnow_link_rebind(void);

/**
 * Opens the pairing window for `seconds`. Outside the window `pair` frames are
 * ignored, so an unpaired remote in range cannot attach itself unattended.
 * Re-pairing simply overwrites the stored peer — there is no separate "forget".
 *
 * Starts the link first when espnow_link_init() left it down. A window that
 * closes with nobody paired leaves it up until the next reboot: tearing it back
 * down means killing a task that may be mid-frame, and the only way to reach
 * this path is to have pressed the button yourself.
 */
void espnow_link_pair_window_open(uint32_t seconds);

/** Seconds left in the pairing window; 0 when closed. */
uint32_t espnow_link_pair_window_left(void);

/** True once a remote MAC is stored (survives reboots, kept in NVS). */
bool espnow_link_is_paired(void);

/** Copies the paired remote's MAC into out. False (out untouched) when unpaired. */
bool espnow_link_get_peer(uint8_t out[ESPNOW_MAC_LEN]);

/**
 * What the radio knows about the link's activity.
 *
 * There is deliberately no "connected" flag. ESP-NOW is connectionless and the
 * remote sleeps between button presses, so silence for hours is a healthy remote,
 * not a fault — `last_seen_s` is the honest reading and a UI should show the age,
 * not a verdict.
 */
typedef struct {
    bool     supported;                     ///< false when built without HAS_ESPNOW_REMOTE
    bool     started;                       ///< esp_now_init() has run
    bool     paired;                        ///< a remote MAC is stored
    uint8_t  mac[ESPNOW_MAC_LEN];           ///< the stored MAC; zeroed when unpaired
    uint32_t window_s;                      ///< seconds left in the pairing window, 0 = closed
    uint32_t last_seen_s;                   ///< age of the last accepted frame; ESPNOW_NEVER if none
    int8_t   rssi;                          ///< RSSI of that frame, dBm; 0 when none
    uint32_t rx_frames;                     ///< frames accepted from the remote since boot
    uint32_t tx_ok;                          ///< replies the remote's MAC ACKed
    uint32_t tx_fail;                       ///< replies it did not
    char     last_cmd[ESPNOW_LAST_CMD_MAX]; ///< last accepted command, truncated; "" if none
} espnow_link_status_t;

/** Fills `out` with the current link status. Never fails; check out->supported. */
void espnow_link_get_status(espnow_link_status_t *out);

#ifdef __cplusplus
}
#endif
