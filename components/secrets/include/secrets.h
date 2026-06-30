#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

// Tiny NVS-backed store for the few secrets we must keep off the
// network-served config files (Wi-Fi / MQTT passwords). Values live in the
// `nvs` partition (namespace "secrets"), so they are never part of the raw
// settings.json / mqtt.json the web UI can fetch.
//
// NOTE: plain NVS is still stored in clear text in flash — this protects the
// HTTP surface only, not a physical `esptool read_flash`. Use Flash Encryption
// if the flash-dump threat matters.

// Stored keys (kept short — NVS keys are limited to 15 chars).
#define SECRET_WIFI_PASS "wifi_pass"
#define SECRET_MQTT_PASS "mqtt_pass"

// Store a secret. A NULL or empty value erases the key.
esp_err_t secrets_set(const char *key, const char *value);

// Read a secret into `out` (always NUL-terminated when out_sz > 0).
// Returns true if the key existed and fit, false otherwise (out left empty).
bool secrets_get(const char *key, char *out, size_t out_sz);
