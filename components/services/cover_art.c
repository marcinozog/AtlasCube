#include "cover_art.h"
#include "metadata.h"     // id3_find_cover — where the APIC image sits in an MP3
#include "sdcard.h"
#include "ui_profile.h"   // the on-screen cover size to decode straight into
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "jpeg_rgb565.h"  // shared decode-to-RGB565 (components/libjpeg)
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TAG = "COVER_ART";

// Size of the cover.bin written for a folder. Matches what the web uploader
// writes, so both routes produce the same file and the widget rescales either
// to whatever the layout asks for. Embedded artwork skips the file entirely and
// is decoded straight into the widget's size.
#define COVER_PX       240

// Fixed, short list of accepted spellings — the names album downloads actually
// use. Anything else is set from the web UI, which writes cover.bin directly.
static const char *k_sources[] = { "cover.jpg", "cover.jpeg", "folder.jpg", "front.jpg" };

#define COVER_DIR_LEN  192           // matches app_state's sd_dir
#define COVER_PATH_LEN (COVER_DIR_LEN + 144)
#define COVER_JPEG_MAX (2 * 1024 * 1024)   // sanity cap on an embedded image

typedef struct {
    char dir[COVER_DIR_LEN];
    char track[COVER_PATH_LEN];
} cover_req_t;

static QueueHandle_t s_queue;                // one slot, overwritten by the newest request
static char          s_failed[COVER_DIR_LEN];// folder whose file source would not convert
static void        (*s_done_cb)(void);

// Embedded artwork waiting for the UI. Touched from the worker task and from
// the LVGL task, so the handoff runs under the spinlock — it is a pointer swap,
// nothing more.
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static uint16_t    *s_emb_buf;
static int          s_emb_w, s_emb_h;
static cover_emb_t  s_emb_state;

// Worker-task state: what was last handed over, so an album's tracks don't each
// pay for decoding the same picture. 0 = nothing embedded is on screen.
static uint32_t     s_emb_sig;
static char         s_emb_dir[COVER_DIR_LEN];

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

// ── Folder artwork (cover.jpg → cover.bin) ───────────────────────────────────
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

// Returns true when a cover.bin was written (the UI has to reload).
static bool convert_folder_art(const char *dir)
{
    char path[COVER_PATH_LEN];
    if (strcmp(dir, s_failed) == 0) return false;
    if (!find_source(dir, path, sizeof(path))) return false;

    char err[96] = "";
    ESP_LOGI(TAG, "converting %s", path);
    uint16_t *px = jpeg_file_to_rgb565(path, COVER_PX, COVER_PX, err, sizeof(err));
    if (!px) {
        ESP_LOGW(TAG, "%s: %s", path, err[0] ? err : "decode failed");
        // A source that would not decode will not fix itself, and retrying
        // costs a full decode — so remember it. A folder with NO source is not
        // remembered: that is four stat() calls to re-check, and dropping a
        // cover.jpg into a folder has to work without a reboot.
        snprintf(s_failed, sizeof(s_failed), "%s", dir);
        return false;
    }

    snprintf(path, sizeof(path), "%s/cover.bin", dir);
    bool ok = write_bin(path, px);
    heap_caps_free(px);
    if (ok) ESP_LOGI(TAG, "wrote %s", path);
    else {
        ESP_LOGW(TAG, "cannot write %s", path);
        snprintf(s_failed, sizeof(s_failed), "%s", dir);
    }
    return ok;
}

// ── Embedded artwork (APIC → PSRAM) ──────────────────────────────────────────
/*
static uint32_t cover_signature(FILE *f, uint32_t offset, uint32_t len)
Cheap identity of an embedded image: its length mixed with the head and tail of
its bytes. Every track of an album normally carries the SAME picture, so this
turns "decode the cover again" into "read 512 bytes and compare" — the decode
only happens when the artwork genuinely differs, which is what a compilation
with per-track covers needs. Returns 0 only on a read error (0 never matches a
stored signature).
*/
static uint32_t cover_signature(FILE *f, uint32_t offset, uint32_t len)
{
    uint8_t sample[256];
    uint32_t hash = 2166136261u ^ len;        // FNV-1a seeded with the length

    for (int part = 0; part < 2; part++) {
        const uint32_t n = len < sizeof(sample) ? len : (uint32_t)sizeof(sample);
        const uint32_t at = part ? (offset + len - n) : offset;
        if (fseek(f, (long)at, SEEK_SET) != 0) return 0;
        if (fread(sample, 1, n, f) != n) return 0;
        for (uint32_t i = 0; i < n; i++) {
            hash ^= sample[i];
            hash *= 16777619u;
        }
        if (len <= sizeof(sample)) break;      // head and tail are the same bytes
    }
    return hash ? hash : 1;
}

// Publish embedded artwork (or its absence) for the UI to collect.
static void publish_embedded(uint16_t *buf, int w, int h, cover_emb_t state)
{
    taskENTER_CRITICAL(&s_mux);
    uint16_t *stale = s_emb_buf;               // a result the UI never collected
    s_emb_buf   = buf;
    s_emb_w     = w;
    s_emb_h     = h;
    s_emb_state = state;
    taskEXIT_CRITICAL(&s_mux);
    if (stale) heap_caps_free(stale);
}

// Returns true when something changed and the UI has to be told.
static bool extract_embedded(const char *track)
{
    uint32_t offset = 0, len = 0;
    if (!track || !track[0] || !id3_find_cover(track, &offset, &len) ||
        len < 4 || len > COVER_JPEG_MAX) {
        // Nothing in this track. Only worth telling the UI if it is currently
        // showing artwork from the previous one.
        if (!s_emb_sig) return false;
        s_emb_sig = 0;
        publish_embedded(NULL, 0, 0, COVER_EMB_CLEAR);
        return true;
    }

    FILE *f = fopen(track, "rb");
    if (!f) return false;

    const uint32_t sig = cover_signature(f, offset, len);
    if (sig && sig == s_emb_sig) { fclose(f); return false; }   // same picture as on screen

    uint8_t *jpg = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    if (!jpg) { fclose(f); return false; }
    bool read_ok = fseek(f, (long)offset, SEEK_SET) == 0 &&
                   fread(jpg, 1, len, f) == len;
    fclose(f);
    if (!read_ok) { heap_caps_free(jpg); return false; }

    const int size = ui_profile_get()->sd_cover_size;
    char err[96] = "";
    uint16_t *px = jpeg_mem_to_rgb565(jpg, len, size, size, err, sizeof(err));
    heap_caps_free(jpg);
    if (!px) {
        ESP_LOGW(TAG, "%s: embedded %s", track, err[0] ? err : "decode failed");
        s_emb_sig = sig;   // a picture that won't decode won't decode next track either
        return false;
    }

    ESP_LOGI(TAG, "embedded cover from %s (%u B)", track, (unsigned)len);
    s_emb_sig = sig;
    publish_embedded(px, size, size, COVER_EMB_NEW);
    return true;
}

cover_emb_t cover_art_take_embedded(uint16_t **buf, int *w, int *h)
{
    taskENTER_CRITICAL(&s_mux);
    cover_emb_t state = s_emb_state;
    if (buf) *buf = s_emb_buf;
    if (w)   *w   = s_emb_w;
    if (h)   *h   = s_emb_h;
    s_emb_buf   = NULL;
    s_emb_state = COVER_EMB_NOTHING;
    taskEXIT_CRITICAL(&s_mux);
    return state;
}

// ── Worker ───────────────────────────────────────────────────────────────────
static void cover_task(void *arg)
{
    (void)arg;
    cover_req_t req;
    for (;;) {
        xQueueReceive(s_queue, &req, portMAX_DELAY);

        // The widget drops its picture whenever the folder changes, so artwork
        // handed over for the previous one is no longer on screen: forget its
        // signature, or an album revisited later would be skipped as "already
        // showing this" and stay blank.
        if (strcmp(req.dir, s_emb_dir) != 0) {
            snprintf(s_emb_dir, sizeof(s_emb_dir), "%s", req.dir);
            s_emb_sig = 0;
        }

        // A folder that already has its artwork file needs nothing further, and
        // that file outranks whatever the tags hold.
        char bin[COVER_PATH_LEN];
        struct stat st;
        snprintf(bin, sizeof(bin), "%s/cover.bin", req.dir);
        if (stat(bin, &st) == 0) continue;

        bool changed = convert_folder_art(req.dir);
        if (!changed) changed = extract_embedded(req.track);
        if (changed && s_done_cb) s_done_cb();
    }
}

void cover_art_request(const char *dir, const char *track)
{
    if (!dir || !dir[0]) return;
    if (sdcard_init() != ESP_OK) return;

    // Started on first use, so a device whose layout has no cover widget never
    // pays for the task. Playback and the SD screen can ask at the same moment,
    // hence the guarded publish — one of them creates it, the other finds it.
    if (!s_queue) {
        QueueHandle_t q = xQueueCreate(1, sizeof(cover_req_t));
        if (!q) return;
        bool mine = false;
        taskENTER_CRITICAL(&s_mux);
        if (!s_queue) { s_queue = q; mine = true; }
        taskEXIT_CRITICAL(&s_mux);
        if (!mine) {
            vQueueDelete(q);                       // lost the race, use the winner's
        } else {
            // Priority 4 keeps it below the audio pipeline: album art must never
            // compete with playback. libjpeg's own allocations go to PSRAM
            // (jmem_esp), so the stack only carries the decoder's call frames.
            if (xTaskCreate(cover_task, "cover_art", 6144, NULL, 4, NULL) != pdPASS) {
                ESP_LOGW(TAG, "cannot start the converter");
                taskENTER_CRITICAL(&s_mux);
                s_queue = NULL;
                taskEXIT_CRITICAL(&s_mux);
                vQueueDelete(q);
                return;
            }
        }
    }

    cover_req_t req;
    snprintf(req.dir, sizeof(req.dir), "%s", dir);
    snprintf(req.track, sizeof(req.track), "%s", track ? track : "");
    // Newest request wins: skipping through five tracks must cost one decode,
    // not five queued ones.
    xQueueOverwrite(s_queue, &req);
}
