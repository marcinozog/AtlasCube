#pragma once
#include <stddef.h>

/**
 * Starts mDNS: advertises the device as "<hostname>.local" and announces an
 * "_http._tcp" service on port 80. The hostname comes from settings
 * (device.hostname); when empty it falls back to an auto name derived from the
 * MAC ("atlascube-xxxx") so two fresh units never collide on the same LAN.
 * Intended for STA mode only — in AP mode the IP is fixed (192.168.4.1).
 */
void mdns_service_start(void);

/**
 * Re-applies the hostname from settings to the running mDNS instance (live,
 * no reboot). No-op if mDNS was not started.
 */
void mdns_service_apply_hostname(void);

/**
 * Writes the effective hostname (resolved auto name when settings are empty)
 * into buf. Useful for showing the actual "<name>.local" in the web UI.
 * Returns buf.
 */
const char *mdns_effective_hostname(char *buf, size_t len);
