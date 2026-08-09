#include "cover_art.h"
#include "sdcard.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "jpeg_rgb565.h"  // shared decode-to-RGB565 (components/libjpeg)
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TAG = "COVER_ART";

// Stored square size. Matches what the web uploader writes, so both routes to
// a cover.bin produce the same file and the widget rescales either to whatever
// the layout asks for.
#define COVER_PX      240

// Fixed, short list of accepted spellings — the names album downloads actually
// use. Anything else is set from the web UI, which writes cover.bin directly.
static const char *k_sources[] = { "cover.jpg", "cover.jpeg", "folder.jpg", "front.jpg" };

#define COVER_DIR_LEN 192           // matches app_state's sd_dir
#define COVER_PATH_LEN (COVER_DIR_LEN + 16)

static char s_dir[COVER_DIR_LEN];   // folder being converted (owned by the task while busy)
static char s_tried[COVER_DIR_LEN]; // last folder with no usable source — don't ask the card again
static volatile bool s_busy;
static void (*s_done_cb)(void);

void cover_art_set_done_cb(void (*cb)(void)) { s_done_cb = cb; }

// ── Store ────────────────────────────────────────────────────────────────────
// LVGL v9 image header — the same 12 bytes the web uploader and
// scripts/img2lvgl.py write, so every route produces one readable format.
typedef struct __attribute__((packed)) {
    uint8_t  magic;      // 0x19
    uint8_t  cf;         // 0x12 = RGB565
    uint16_t flags;
    uint16_t w;
    uint16_t h;
    uint16_t stride;
    uint16_t reserved;
} cover_bin_header_t;

static bool write_bin(const char *path, const uint16_t *px)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return false;

    const cover_bin_header_t hdr = {
        .magic  = 0x19,
        .cf     = 0x12,
        .w      = COVER_PX,
        .h      = COVER_PX,
        .stride = COVER_PX * 2,
    };
    const size_t px_count = (size_t)COVER_PX * COVER_PX;
    bool ok = fwrite(&hdr, sizeof(hdr), 1, fp) == 1 &&
              fwrite(px, 2, px_count, fp) == px_count;
    fclose(fp);
    if (!ok) unlink(path);   // don't leave a truncated .bin behind
    return ok;
}

// ── Worker ───────────────────────────────────────────────────────────────────
// First accepted JPEG in `dir`, or false when the folder has none.
static bool find_source(const char *dir, char *out, size_t cap)
{
    for (size_t i = 0; i < sizeof(k_sources) / sizeof(k_sources[0]); i++) {
        struct stat st;
        snprintf(out, cap, "%s/%s", dir, k_sources[i]);
        if (stat(out, &st) == 0 && !S_ISDIR(st.st_mode)) return true;
    }
    return false;
}

static void convert_task(void *arg)
{
    (void)arg;
    char src[COVER_PATH_LEN];
    char dst[COVER_PATH_LEN];
    bool ok = false;

    if (find_source(s_dir, src, sizeof(src))) {
        ESP_LOGI(TAG, "converting %s", src);
        char err[96] = "";
        uint16_t *px = jpeg_file_to_rgb565(src, COVER_PX, COVER_PX, err, sizeof(err));
        if (!px) {
            ESP_LOGW(TAG, "%s: %s", src, err[0] ? err : "decode failed");
        } else {
            snprintf(dst, sizeof(dst), "%s/cover.bin", s_dir);
            ok = write_bin(dst, px);
            heap_caps_free(px);
            if (ok) ESP_LOGI(TAG, "wrote %s", dst);
            else    ESP_LOGW(TAG, "cannot write %s", dst);
        }
    }

    // Remember a folder that yielded nothing, so browsing back and forth over
    // an album without artwork doesn't hit the card again and again.
    if (!ok) snprintf(s_tried, sizeof(s_tried), "%s", s_dir);

    s_busy = false;
    if (ok && s_done_cb) s_done_cb();
    vTaskDelete(NULL);
}

void cover_art_request(const char *dir)
{
    if (!dir || !dir[0] || s_busy) return;
    if (strcmp(dir, s_tried) == 0) return;
    if (sdcard_init() != ESP_OK) return;

    snprintf(s_dir, sizeof(s_dir), "%s", dir);
    s_busy = true;
    // Short-lived: one album's worth of work, then the task is gone. libjpeg's
    // own allocations go to PSRAM (jmem_esp), so the stack only carries the
    // decoder's call frames.
    if (xTaskCreate(convert_task, "cover_art", 6144, NULL, 4, NULL) != pdPASS) {
        ESP_LOGW(TAG, "cannot start the converter");
        s_busy = false;
    }
}
