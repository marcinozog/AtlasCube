#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define SD_MOUNT_POINT "/sdcard"

// Mounts the SD card as FAT over SDMMC in 1-bit mode at SD_MOUNT_POINT.
// Non-fatal: if no card is present or the mount fails it logs a warning and
// returns the error — the rest of the system runs fine without SD (the radio
// streams over WiFi). Idempotent: a second call while already mounted is a no-op.
esp_err_t sdcard_init(void);

// True once the card is mounted and ready for filesystem access.
bool sdcard_is_mounted(void);
