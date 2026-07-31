#pragma once

/**
 * Lifecycle for everything that needs the LAN/internet: NTP, weather, mDNS,
 * MQTT, the auto-updater, the internet-wallpaper schedule and the radio resume.
 *
 * These used to be started inline in app_main() behind
 * `wifi_get_run_mode() == WIFI_RUN_MODE_STA`, which silently assumed the link is
 * either up by the end of boot or never. It is not: the WiFi recovery supervisor
 * can reach the router minutes later (see TMP/TODO/wifi_recovery.md), and
 * without this layer such a device would have an IP address and nothing else.
 *
 * Call once, late in app_main() — after the UI, WebSocket, HTTP server and
 * ESP-NOW are up, because the services started here expect them to exist. It
 * registers a WiFi link callback and, if the link is already up from boot, runs
 * the start-up path immediately. Both are idempotent, so the race between them
 * is harmless.
 */
void network_services_start(void);
