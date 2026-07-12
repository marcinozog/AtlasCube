#include "screensaver_photo.h"
#include "ui_screen.h"
#include "ui_profile.h"
#include "sdcard.h"
#include "settings.h"
#include "ntp_service.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

static const char *TAG = "SS_PHOTO";

// Config comes from settings_ex (scrsaver.photo_*); these are fallbacks/bases.
#define PHOTO_DIR_DEFAULT    SD_MOUNT_POINT "/slides"
#define TICK_MS              30
#define MAX_FILES            256

// Reveal pacing baselines (= speed level 3); scaled by the speed setting via spd()
#define LOAD_ROWS_PER_TICK   24
#define TOPDOWN_ROWS         6
#define WIPE_COLS            10
#define DISSOLVE_BLOCK       16
#define DISSOLVE_PER_TICK    14
#define INTERLACE_ROWS       24

// LVGL v9 binary image header (little-endian, 12 bytes) — see scripts/img2lvgl.py
#define LV_BIN_MAGIC   0x19
#define LV_BIN_RGB565  0x12
typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  cf;
    uint16_t flags;
    uint16_t w;
    uint16_t h;
    uint16_t stride;
    uint16_t reserved;
} bin_header_t;

typedef enum {
    FX_TOPDOWN = 0,
    FX_WIPE,
    FX_DISSOLVE,
    FX_INTERLACED,
    FX_COUNT,
} photo_fx_t;

typedef enum { ORDER_SEQUENTIAL = 0, ORDER_RANDOM } photo_order_t;

typedef enum { PH_PICK, PH_LOAD, PH_REVEAL, PH_HOLD, PH_EMPTY } phase_t;

// ── State ─────────────────────────────────────────────────────────────────────
static lv_obj_t   *s_root   = NULL;
static lv_obj_t   *s_canvas = NULL;
static uint16_t   *s_canvas_buf = NULL;   // what is displayed (revealed so far)
static uint16_t   *s_stage_buf  = NULL;   // full target image loaded from SD
static lv_timer_t *s_timer   = NULL;
static int         s_w = 0, s_h = 0;

static char      *s_files[MAX_FILES];
static int        s_file_count = 0;
static int        s_file_idx   = -1;

static phase_t    s_phase = PH_PICK;
static FILE      *s_fp    = NULL;
static int        s_load_row = 0;
static int64_t    s_hold_until = 0;

// Clock overlay (HH:MM) drawn on top of the photo. 4 black copies offset by a
// couple px form an outline behind the white label so it reads on any photo.
#define CLOCK_LAYERS 5             // [0..3] outline, [4] white top
static lv_obj_t  *s_clock[CLOCK_LAYERS] = {0};
static bool       s_clock_on   = true;
static int        s_clock_size = 96;       // 72 / 80 / 96 / 120
static char       s_clock_txt[8] = "";     // last text shown (skip redundant sets)
static int64_t    s_clock_next = 0;        // next clock refresh (esp_timer us)

// Loaded from settings_ex at create()
static char          s_dir[64];
static int64_t       s_hold_ms    = 8000;
static int           s_effect_cfg = 4;            // 0..3 fixed, 4 = random per slide
static int           s_speed      = 3;            // 1 (slow) .. 5 (fast)
static photo_fx_t    s_effect = FX_INTERLACED;    // active effect for the current slide
static photo_order_t s_order  = ORDER_RANDOM;

// Scale a pacing baseline by the speed setting (speed 3 = unchanged), min 1.
static inline int spd(int base)
{
    int v = base * s_speed / 3;
    return v < 1 ? 1 : v;
}

static unsigned s_cfg_gen = 0;   // last applied settings_photo_generation()

// Normalize the configured folder: the VFS mount prefix must be exactly
// SD_MOUNT_POINT (case-sensitive, with a leading slash), while the rest of the
// path FATFS matches case-insensitively — so fix up "SDCARD/SLIDES" & co.
static void normalize_dir(char *dst, size_t dst_sz, const char *src)
{
    while (*src == ' ') src++;
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s%s", (*src == '/') ? "" : "/", src);
    size_t n = strlen(tmp);
    while (n > 1 && (tmp[n - 1] == ' ' || tmp[n - 1] == '/')) tmp[--n] = '\0';

    const size_t mp_len = strlen(SD_MOUNT_POINT);
    if (strncasecmp(tmp, SD_MOUNT_POINT, mp_len) == 0 &&
        (tmp[mp_len] == '\0' || tmp[mp_len] == '/')) {
        snprintf(dst, dst_sz, "%s%s", SD_MOUNT_POINT, tmp + mp_len);
    } else {
        strncpy(dst, tmp, dst_sz - 1);
        dst[dst_sz - 1] = '\0';
    }
}

// (Re)load photo config from settings_ex into the local state.
static void load_config(void)
{
    app_settings_t *st = settings_get();
    const char *dir = st->scrsaver.photo_dir[0] ? st->scrsaver.photo_dir : PHOTO_DIR_DEFAULT;
    normalize_dir(s_dir, sizeof(s_dir), dir);
    s_order      = st->scrsaver.photo_order ? ORDER_RANDOM : ORDER_SEQUENTIAL;
    s_hold_ms    = (int64_t)st->scrsaver.photo_hold_s * 1000;
    s_effect_cfg = st->scrsaver.photo_effect;
    s_speed      = st->scrsaver.photo_speed;
    s_clock_on   = st->scrsaver.photo_clock ? true : false;
    s_clock_size = st->scrsaver.photo_clock_size;
}

// ── Clock overlay ──────────────────────────────────────────────────────────────
// The big _72/_80/_96/_120 fonts are digit-only (0x30-0x3A) — which covers the
// "0-9" and ":" of HH:MM exactly.
static const lv_font_t *clock_font(int size)
{
    switch (size) {
    case 72:  return &lv_font_montserrat_72;
    case 80:  return &lv_font_montserrat_80;
    case 120: return &lv_font_montserrat_120;
    default:  return &lv_font_montserrat_96;
    }
}

static void clock_destroy(void)
{
    for (int i = 0; i < CLOCK_LAYERS; i++) {
        if (s_clock[i]) { lv_obj_delete(s_clock[i]); s_clock[i] = NULL; }
    }
    s_clock_txt[0] = '\0';
}

// Build the 5 centred labels on top of the canvas. Hidden until NTP syncs (the
// digit-only font can't render a "--:--" placeholder anyway).
static void clock_build(void)
{
    if (!s_clock_on || !s_root) return;
    static const int OX[4] = { -2,  2, -2,  2 };
    static const int OY[4] = { -2, -2,  2,  2 };
    const lv_font_t *f = clock_font(s_clock_size);
    for (int i = 0; i < CLOCK_LAYERS; i++) {
        lv_obj_t *l = lv_label_create(s_root);
        lv_obj_set_style_text_font(l, f, LV_PART_MAIN);
        bool top = (i == CLOCK_LAYERS - 1);
        lv_obj_set_style_text_color(l, top ? lv_color_white() : lv_color_black(), LV_PART_MAIN);
        lv_obj_align(l, LV_ALIGN_CENTER, top ? 0 : OX[i], top ? 0 : OY[i]);
        lv_label_set_text(l, "");
        lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
        s_clock[i] = l;
    }
    s_clock_txt[0] = '\0';
}

static void clock_update(void)
{
    if (!s_clock_on || !s_clock[0]) return;

    if (!ntp_service_is_synced()) {
        if (!lv_obj_has_flag(s_clock[0], LV_OBJ_FLAG_HIDDEN))
            for (int i = 0; i < CLOCK_LAYERS; i++) lv_obj_add_flag(s_clock[i], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);

    bool hidden = lv_obj_has_flag(s_clock[0], LV_OBJ_FLAG_HIDDEN);
    if (!hidden && strcmp(buf, s_clock_txt) == 0) return;   // nothing changed

    strcpy(s_clock_txt, buf);
    for (int i = 0; i < CLOCK_LAYERS; i++) {
        lv_label_set_text(s_clock[i], buf);
        lv_obj_clear_flag(s_clock[i], LV_OBJ_FLAG_HIDDEN);
    }
}

// reveal progress
static int        s_rev = 0;       // generic counter (rows / cols revealed)
static int        s_pass = 0;      // interlace pass index
static int       *s_perm = NULL;   // dissolve block order
static int        s_perm_n = 0;    // number of blocks
static int        s_perm_i = 0;    // next block to reveal

static uint32_t   s_rng = 0x1234abcd;

static inline uint32_t xrand(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

// ── Directory scan ────────────────────────────────────────────────────────────
static void free_files(void)
{
    for (int i = 0; i < s_file_count; i++) free(s_files[i]);
    s_file_count = 0;
}

static void scan_dir(void)
{
    free_files();
    DIR *d = opendir(s_dir);
    if (!d) {
        ESP_LOGW(TAG, "Cannot open %s", s_dir);
        return;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL && s_file_count < MAX_FILES) {
        if (e->d_name[0] == '.') continue;
        size_t n = strlen(e->d_name);
        if (n < 5 || strcasecmp(e->d_name + n - 4, ".bin") != 0) continue;
        s_files[s_file_count] = strdup(e->d_name);
        if (s_files[s_file_count]) s_file_count++;
    }
    closedir(d);
    ESP_LOGI(TAG, "Found %d slide(s) in %s", s_file_count, s_dir);
}

static const char *next_file(void)
{
    if (s_file_count == 0) return NULL;
    int idx;
    if (s_order == ORDER_RANDOM) {
        idx = (int)(xrand() % (uint32_t)s_file_count);
        if (s_file_count > 1 && idx == s_file_idx) idx = (idx + 1) % s_file_count;
    } else {
        idx = (s_file_idx + 1) % s_file_count;
        if (idx == 0) scan_dir();   // re-scan once per full loop to pick up new uploads
    }
    s_file_idx = idx;
    return s_file_count ? s_files[idx] : NULL;
}

// ── Phase: pick + open + validate header ─────────────────────────────────────
static bool open_next(void)
{
    for (int tries = 0; tries < s_file_count + 1; tries++) {
        const char *name = next_file();
        if (!name) return false;

        char path[sizeof(s_dir) + 256];
        snprintf(path, sizeof(path), "%s/%s", s_dir, name);
        FILE *fp = fopen(path, "rb");
        if (!fp) { ESP_LOGW(TAG, "open %s failed", path); continue; }

        bin_header_t h;
        if (fread(&h, sizeof(h), 1, fp) != 1 ||
            h.magic != LV_BIN_MAGIC || h.cf != LV_BIN_RGB565 ||
            h.w != s_w || h.h != s_h) {
            ESP_LOGW(TAG, "skip %s (bad header or size %ux%u)", name, h.w, h.h);
            fclose(fp);
            continue;
        }
        // Pick the effect for this slide (fixed, or random when configured to 4).
        s_effect = (s_effect_cfg >= 0 && s_effect_cfg <= 3)
                       ? (photo_fx_t)s_effect_cfg
                       : (photo_fx_t)(xrand() % FX_COUNT);
        s_fp = fp;
        s_load_row = 0;
        return true;
    }
    return false;
}

// ── Effect setup + steppers (copy stage → canvas) ────────────────────────────
static inline void copy_rows(int y0, int rows)
{
    if (y0 >= s_h) return;
    if (y0 + rows > s_h) rows = s_h - y0;
    memcpy(s_canvas_buf + (size_t)y0 * s_w,
           s_stage_buf  + (size_t)y0 * s_w,
           (size_t)rows * s_w * 2);
}

// Mark only the changed rectangle dirty (canvas sits at 0,0 so its pixel coords
// equal screen coords). Per-tick partial invalidation instead of full-screen —
// over a reveal LVGL repaints ~one screenful total, not one per tick.
static inline void inval(int x1, int y1, int x2, int y2)
{
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= s_w) x2 = s_w - 1;
    if (y2 >= s_h) y2 = s_h - 1;
    if (x2 < x1 || y2 < y1) return;
    lv_area_t a = { .x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2 };
    lv_obj_invalidate_area(s_canvas, &a);
}

static void reveal_begin(void)
{
    s_rev = 0;
    s_pass = 0;
    s_perm_i = 0;

    if (s_effect == FX_DISSOLVE && s_perm) {
        // Fisher-Yates shuffle of the block order
        for (int i = s_perm_n - 1; i > 0; i--) {
            int j = (int)(xrand() % (uint32_t)(i + 1));
            int t = s_perm[i]; s_perm[i] = s_perm[j]; s_perm[j] = t;
        }
    }
}

// One reveal step; returns true when the slide is fully shown.
static bool reveal_step(void)
{
    switch (s_effect) {
    case FX_TOPDOWN: {
        int rows = spd(TOPDOWN_ROWS);
        int y0 = s_rev;
        copy_rows(s_rev, rows);
        s_rev += rows;
        inval(0, y0, s_w - 1, y0 + rows - 1);
        return s_rev >= s_h;
    }
    case FX_WIPE: {
        int cols = spd(WIPE_COLS);
        if (s_rev + cols > s_w) cols = s_w - s_rev;
        int x0 = s_rev;
        for (int y = 0; y < s_h; y++) {
            memcpy(s_canvas_buf + (size_t)y * s_w + s_rev,
                   s_stage_buf  + (size_t)y * s_w + s_rev,
                   (size_t)cols * 2);
        }
        s_rev += cols;
        inval(x0, 0, x0 + cols - 1, s_h - 1);
        return s_rev >= s_w;
    }
    case FX_DISSOLVE: {
        int bx = (s_w + DISSOLVE_BLOCK - 1) / DISSOLVE_BLOCK;
        int per = spd(DISSOLVE_PER_TICK);
        for (int k = 0; k < per && s_perm_i < s_perm_n; k++, s_perm_i++) {
            int b = s_perm[s_perm_i];
            int gx = (b % bx) * DISSOLVE_BLOCK;
            int gy = (b / bx) * DISSOLVE_BLOCK;
            int bw = (gx + DISSOLVE_BLOCK > s_w) ? s_w - gx : DISSOLVE_BLOCK;
            int bh = (gy + DISSOLVE_BLOCK > s_h) ? s_h - gy : DISSOLVE_BLOCK;
            for (int y = gy; y < gy + bh; y++) {
                memcpy(s_canvas_buf + (size_t)y * s_w + gx,
                       s_stage_buf  + (size_t)y * s_w + gx,
                       (size_t)bw * 2);
            }
            inval(gx, gy, gx + bw - 1, gy + bh - 1);
        }
        return s_perm_i >= s_perm_n;
    }
    case FX_INTERLACED:
    default: {
        // Passes with shrinking stride: each pass copies one source row and
        // duplicates it down to fill its stride → blocky preview that sharpens.
        static const int strides[] = { 8, 4, 2, 1 };
        int stride = strides[s_pass];
        int budget = spd(INTERLACE_ROWS);
        int y0 = s_rev;
        int yend = s_rev;
        int done_rows = 0;
        while (done_rows < budget && s_rev < s_h) {
            for (int dy = 0; dy < stride && s_rev + dy < s_h; dy++) {
                memcpy(s_canvas_buf + (size_t)(s_rev + dy) * s_w,
                       s_stage_buf  + (size_t)s_rev * s_w,
                       (size_t)s_w * 2);
                yend = s_rev + dy + 1;
            }
            s_rev += stride;
            done_rows += stride;
        }
        inval(0, y0, s_w - 1, yend - 1);
        if (s_rev >= s_h) {
            s_pass++;
            s_rev = 0;
            if (s_pass >= (int)(sizeof(strides) / sizeof(strides[0]))) return true;
        }
        return false;
    }
    }
}

// ── Timer: the state machine ─────────────────────────────────────────────────
static void tick(lv_timer_t *t)
{
    (void)t;
    if (!s_canvas_buf || !s_stage_buf) return;

    // Config changed in settings? Apply it live and restart the current slide.
    if (settings_photo_generation() != s_cfg_gen) {
        s_cfg_gen = settings_photo_generation();
        char old_dir[sizeof(s_dir)];
        strncpy(old_dir, s_dir, sizeof(old_dir) - 1);
        old_dir[sizeof(old_dir) - 1] = '\0';
        load_config();
        if (s_fp) { fclose(s_fp); s_fp = NULL; }
        if (strcmp(old_dir, s_dir) != 0) scan_dir();
        clock_destroy();            // toggle/size may have changed — rebuild
        clock_build();
        s_clock_next = 0;
        s_phase = PH_PICK;
    }

    // Refresh the clock overlay once a second (no-op when text is unchanged).
    int64_t nowus = esp_timer_get_time();
    if (nowus >= s_clock_next) {
        s_clock_next = nowus + 1000000;
        clock_update();
    }

    switch (s_phase) {
    case PH_PICK:
        if (open_next()) {
            s_phase = PH_LOAD;
        } else {
            // No slides — paint black once and wait before re-scanning.
            memset(s_canvas_buf, 0, (size_t)s_w * s_h * 2);
            lv_obj_invalidate(s_canvas);
            s_hold_until = esp_timer_get_time() + 2000 * 1000;
            s_phase = PH_EMPTY;
        }
        break;

    case PH_LOAD: {
        int rows = LOAD_ROWS_PER_TICK;
        if (s_load_row + rows > s_h) rows = s_h - s_load_row;
        size_t got = fread(s_stage_buf + (size_t)s_load_row * s_w,
                           (size_t)s_w * 2, rows, s_fp);
        s_load_row += (int)got;
        if (got < (size_t)rows || s_load_row >= s_h) {
            fclose(s_fp);
            s_fp = NULL;
            reveal_begin();
            s_phase = PH_REVEAL;
        }
        break;
    }

    case PH_REVEAL:
        // reveal_step() invalidates only the region it changed this tick.
        if (reveal_step()) {
            s_hold_until = esp_timer_get_time() + s_hold_ms * 1000;
            s_phase = PH_HOLD;
        }
        break;

    case PH_HOLD:
        if (esp_timer_get_time() >= s_hold_until) s_phase = PH_PICK;
        break;

    case PH_EMPTY:
        if (esp_timer_get_time() >= s_hold_until) { scan_dir(); s_phase = PH_PICK; }
        break;
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
static void photo_create(lv_obj_t *parent)
{
    s_w = DISPLAY_WIDTH;
    s_h = DISPLAY_HEIGHT;
    s_phase = PH_PICK;
    s_file_idx = -1;
    s_fp = NULL;
    s_rng ^= (uint32_t)esp_timer_get_time();

    // Pull config from settings_ex
    load_config();
    s_cfg_gen = settings_photo_generation();

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, s_w, s_h);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    size_t sz = (size_t)s_w * s_h * 2;
    s_canvas_buf = heap_caps_aligned_alloc(4, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_stage_buf  = heap_caps_aligned_alloc(4, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_canvas_buf || !s_stage_buf) {
        ESP_LOGE(TAG, "PSRAM alloc failed (2x%u bytes)", (unsigned)sz);
        return;
    }
    memset(s_canvas_buf, 0, sz);

    // Dissolve block permutation (built once, reshuffled per slide)
    int bx = (s_w + DISSOLVE_BLOCK - 1) / DISSOLVE_BLOCK;
    int by = (s_h + DISSOLVE_BLOCK - 1) / DISSOLVE_BLOCK;
    s_perm_n = bx * by;
    s_perm = malloc((size_t)s_perm_n * sizeof(int));
    if (s_perm) for (int i = 0; i < s_perm_n; i++) s_perm[i] = i;

    s_canvas = lv_canvas_create(s_root);
    lv_canvas_set_buffer(s_canvas, s_canvas_buf, s_w, s_h, LV_COLOR_FORMAT_RGB565);
    lv_obj_align(s_canvas, LV_ALIGN_TOP_LEFT, 0, 0);

    // Clock labels are created after the canvas so they sit on top of it.
    s_clock_next = 0;
    clock_build();

    if (sdcard_init() != ESP_OK) ESP_LOGW(TAG, "SD not available — no slides will load");
    scan_dir();

    s_timer = lv_timer_create(tick, TICK_MS, NULL);
    ESP_LOGI(TAG, "Created (2 canvases %dx%d in PSRAM = %u B)", s_w, s_h, (unsigned)(sz * 2));
}

static void photo_destroy(void)
{
    if (s_timer)  { lv_timer_delete(s_timer);  s_timer  = NULL; }
    if (s_fp)     { fclose(s_fp);              s_fp     = NULL; }
    if (s_canvas) { lv_obj_delete(s_canvas);   s_canvas = NULL; }
    if (s_root)   { lv_obj_delete(s_root);     s_root   = NULL; }
    // Clock labels were children of s_root — already freed by its deletion.
    for (int i = 0; i < CLOCK_LAYERS; i++) s_clock[i] = NULL;
    if (s_canvas_buf) { heap_caps_free(s_canvas_buf); s_canvas_buf = NULL; }
    if (s_stage_buf)  { heap_caps_free(s_stage_buf);  s_stage_buf  = NULL; }
    if (s_perm)   { free(s_perm); s_perm = NULL; }
    free_files();
    ESP_LOGI(TAG, "Destroyed");
}

const ui_screen_t screensaver_photo = {
    .create      = photo_create,
    .destroy     = photo_destroy,
    .apply_theme = NULL,
    .on_event    = NULL,
    .on_input    = NULL,
    .name        = "ss_photo",
};
