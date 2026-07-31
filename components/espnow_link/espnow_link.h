#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESPNOW_MAC_LEN 6

/**
 * ESP-NOW control link for the hardware pilot (AtlasCubeController).
 * Wire contract: docs/espnow_link.md. Command vocabulary: docs/ws_protocol.md —
 * frames land in media_command_execute_text(), the same dispatcher the WS
 * server uses, so the two transports cannot drift apart.
 *
 * Runs alongside the STA interface on the channel the router imposed; in AP
 * fallback mode it binds to the AP interface instead.
 */

/**
 * Starts the link: esp_now_init(), the RX callback, and the worker task that
 * drains it. Must be called AFTER wifi_init() has settled the mode — ESP-NOW
 * needs a started WiFi interface. Safe to call twice (second call is a no-op).
 */
esp_err_t espnow_link_init(void);

/**
 * Re-adds the paired peer against whichever WiFi interface is up now. Call
 * after a run-mode change (AP↔STA) — an ESP-NOW peer remembers the interface it
 * was added on, so one that outlived its interface stops delivering silently.
 * No-op when the link isn't running or no pilot is paired.
 */
void espnow_link_rebind(void);

/**
 * Opens the pairing window for `seconds`. Outside the window `pair` frames are
 * ignored, so an unpaired pilot in range cannot attach itself unattended.
 * Re-pairing simply overwrites the stored peer — there is no separate "forget".
 */
void espnow_link_pair_window_open(uint32_t seconds);

/** Seconds left in the pairing window; 0 when closed. */
uint32_t espnow_link_pair_window_left(void);

/** True once a pilot MAC is stored (survives reboots, kept in NVS). */
bool espnow_link_is_paired(void);

/** Copies the paired pilot's MAC into out. False (out untouched) when unpaired. */
bool espnow_link_get_peer(uint8_t out[ESPNOW_MAC_LEN]);

#ifdef __cplusplus
}
#endif
