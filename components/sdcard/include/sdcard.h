#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define SD_MOUNT_POINT "/sdcard"

// Ensures the SD card is mounted as FAT over SDMMC in 1-bit mode at
// SD_MOUNT_POINT, mounting it lazily on first call (not at boot — a radio-only
// session never pays the SDMMC+FATFS internal-RAM cost). The actual mount runs
// on a short-lived worker task with its own stack, so this is safe to call from
// shallow tasks (httpd/WS, UI, events). Returns ESP_OK once mounted.
// Non-fatal: a missing/failed card just logs a warning and returns the error —
// the rest of the system runs fine without SD. Idempotent: returns immediately
// when already mounted. On a build without HAS_SD_CARD it is a no-op returning
// ESP_ERR_NOT_SUPPORTED.
esp_err_t sdcard_init(void);

// True once the card is mounted and ready for filesystem access.
bool sdcard_is_mounted(void);
