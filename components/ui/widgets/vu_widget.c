#include "vu_widget.h"
#include "audio_levels.h"   // PCM ring tapped from the DSP element
#include "theme.h"
#include "esp_dsp.h"        // dsps_fft2r_*, dsps_wind_hann_f32
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "VU_WIDGET";

// ── Tunables ────────────────────────────────────────────────────────────────
// N=2048 → 21.5 Hz/bin at 44.1 kHz, window ~46 ms. Shorter window = less visual
// lag behind the audio (the bar reflects ~46 ms of past PCM, not ~93 ms as N=4096
// did). Trade-off: the 31 & 62 Hz half-octave bars partly re-merge (bins 1-2),
// but the bars above ~125 Hz still separate. Bars sit on the EQ's own band
// centres (2 per octave) so meter and equaliser describe the same bands.
#define VU_FFT_N      1024         // FFT window (power of two)
#define VU_BARS_MAX   12           // bar count — bars are log-spaced across the
                                   // spectrum (decoupled from the EQ band count),
                                   // so this can be any value, not just 2*bands.
#define VU_TICK_MS    33           // refresh period (~30 fps; only this small rect redraws)
#define VU_SAMPLE_HZ  44100.0f     // PLAYBACK_SAMPLE_RATE (fixed post-rsp)
#define VU_FREQ_TOP   17000.0f     // clamp the top band below Nyquist so the last
                                   // bar doesn't peak on >16 kHz codec noise
#define VU_DB_FLOOR    0.0f        // peak power(dB) mapped to bar = 0
#define VU_DB_CEIL     36.0f       // peak power(dB) mapped to bar = full height
#define VU_ATTACK      0.95f       // smoothing when a bar rises (fast)
#define VU_DECAY       0.50f       // smoothing when a bar falls (higher → snappier drop)
#define VU_GAMMA       0.6f        // <1 boosts perceived motion at low levels

// FFT runs on its own task pinned to core 0 so the heavy transform never
// blocks the LVGL task's (blocking) display flush — see audio task notes.
#define VU_FFT_CORE    0           // off the LVGL/display core
#define VU_FFT_PRIO    3           // low: yields to audio (ADF) on core 0
#define VU_FFT_STACK   4096        // internal-RAM stack (working buffers stay in PSRAM)

// ── Display objects ──────────────────────────────────────────────────────────
static lv_obj_t   *s_cont = NULL;
static lv_obj_t   *s_bars[VU_BARS_MAX];
static lv_timer_t *s_timer = NULL;
static int         s_h      = 0;   // container inner height (bar baseline)
static int         s_nbars  = 0;   // active bar count (= VU_BARS_MAX, log-spaced)

// ── FFT task (core 0) ────────────────────────────────────────────────────────
static TaskHandle_t      s_fft_task = NULL;
static SemaphoreHandle_t s_fft_done = NULL;  // task → destroy() join handshake
static volatile bool     s_fft_run  = false; // task loop condition

// ── DSP working buffers (LVGL task only; PSRAM to spare internal RAM) ─────────
static int16_t *s_raw = NULL;          // newest mono window (VU_FFT_N)
static float   *s_win = NULL;          // Hann window (VU_FFT_N)
static float   *s_cf  = NULL;          // interleaved complex Re,Im (VU_FFT_N*2)
static float    s_lvl[VU_BARS_MAX];    // smoothed bar levels 0..1
static int      s_bin_lo[VU_BARS_MAX]; // FFT bin range per bar
static int      s_bin_hi[VU_BARS_MAX];
static uint32_t s_last_count = 0;      // audio_levels_count() at last tick
static bool     s_table_ready = false; // one-time FFT twiddle table (global, persists)

// Hz → FFT bin index, clamped to the usable range [1 .. N/2].
static void edge_to_bins(int bar, float f_lo, float f_hi)
{
    const float bin_hz = VU_SAMPLE_HZ / (float)VU_FFT_N;
    int lo = (int)(f_lo / bin_hz);
    int hi = (int)(f_hi / bin_hz);
    if (lo < 1)            lo = 1;
    if (hi <= lo)          hi = lo + 1;
    if (hi > VU_FFT_N / 2) hi = VU_FFT_N / 2;
    if (lo >= hi)          lo = hi - 1;
    s_bin_lo[bar] = lo;
    s_bin_hi[bar] = hi;
}

// Lay VU_BARS_MAX bars log-spaced across the audible band (60 Hz … VU_FREQ_TOP),
// decoupled from the EQ band count so the bar count is free. The low edge starts
// at 60 Hz, not lower: at N=1024 (43 Hz/bin) anything below ~90 Hz collapses onto
// FFT bin 1, so starting lower just made the bottom bars move identically. 60 Hz
// keeps each low bar at least one bin wide; the small speaker can't reproduce
// sub-60 Hz anyway. Top is clamped below the Nyquist noise. Each bar spans one
// equal ratio step, so spacing is even on a log (musical) axis.
static void build_bins(void)
{
    s_nbars = VU_BARS_MAX;

    const float f_lo  = 60.0f;          // see note above (FFT bin resolution)
    const float f_hi  = VU_FREQ_TOP;
    const float ratio = powf(f_hi / f_lo, 1.0f / (float)s_nbars);

    float edge = f_lo;
    for (int b = 0; b < s_nbars; b++) {
        float next = edge * ratio;
        edge_to_bins(b, edge, next);
        edge = next;
    }
}

// Push the current s_lvl[] into bar heights. Only the container area invalidates.
static void render_bars(void)
{
    for (int b = 0; b < s_nbars; b++) {
        int hgt = (int)(powf(s_lvl[b], VU_GAMMA) * (float)s_h + 0.5f);
        if (hgt < 1) hgt = 1;
        if (hgt == lv_obj_get_height(s_bars[b])) continue;
        lv_obj_set_height(s_bars[b], hgt);
        lv_obj_set_y(s_bars[b], s_h - hgt);
    }
}

static void smooth_to(int b, float target)
{
    float k = (target > s_lvl[b]) ? VU_ATTACK : VU_DECAY;
    s_lvl[b] += (target - s_lvl[b]) * k;
}

// One FFT frame → smoothed s_lvl[]. Runs on the core-0 FFT task only: NO LVGL
// calls here (LVGL is not thread-safe). The render timer turns s_lvl[] into bar
// heights on the LVGL task.
static void fft_compute(void)
{
    if (!s_table_ready || !s_cf) return;

    // No new audio since last frame (paused / stopped): decay to rest.
    uint32_t count = audio_levels_count();
    if (count == s_last_count || !audio_levels_snapshot(s_raw, VU_FFT_N)) {
        for (int b = 0; b < s_nbars; b++) smooth_to(b, 0.0f);
        return;
    }
    s_last_count = count;

    // Window into the complex array (imaginary part zeroed), normalise to ±1.
    for (int i = 0; i < VU_FFT_N; i++) {
        s_cf[2 * i]     = (s_raw[i] * (1.0f / 32768.0f)) * s_win[i];
        s_cf[2 * i + 1] = 0.0f;
    }

    dsps_fft2r_fc32(s_cf, VU_FFT_N);
    dsps_bit_rev_fc32(s_cf, VU_FFT_N);

    // Per bar: PEAK power across its bins → dB → normalised 0..1 → smoothed.
    // Peak keeps the meter punchy (transients pop); the wide-band high-frequency
    // bias it used to cause is handled by clamping the top band (VU_FREQ_TOP).
    for (int b = 0; b < s_nbars; b++) {
        float peak = 0.0f;
        for (int k = s_bin_lo[b]; k < s_bin_hi[b]; k++) {
            float re = s_cf[2 * k], im = s_cf[2 * k + 1];
            float p  = re * re + im * im;
            if (p > peak) peak = p;
        }
        float db  = 10.0f * log10f(peak + 1e-9f);   // 10·log10 of peak power
        float lvl = (db - VU_DB_FLOOR) / (VU_DB_CEIL - VU_DB_FLOOR);
        if (lvl < 0.0f) lvl = 0.0f; else if (lvl > 1.0f) lvl = 1.0f;
        smooth_to(b, lvl);
    }
}

// Dedicated FFT task, pinned to core 0 so the 4096-pt transform never blocks the
// LVGL task's blocking display flush. Working buffers live in PSRAM; only this
// ~4 KB stack sits in internal RAM. Yields between frames at the VU refresh rate.
static void fft_task(void *arg)
{
    (void)arg;
    while (s_fft_run) {
        fft_compute();
        vTaskDelay(pdMS_TO_TICKS(VU_TICK_MS));
    }
    xSemaphoreGive(s_fft_done);   // let destroy() free the buffers
    vTaskDelete(NULL);
}

// LVGL-task timer: render-only. Reads the latest s_lvl[] (written by fft_task)
// into bar heights. Benign single-writer/single-reader float races at worst
// flicker one bar by a pixel for one frame.
static void tick_cb(lv_timer_t *t)
{
    (void)t;
    render_bars();
}

void vu_widget_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    if (s_cont) return;
    const ui_theme_colors_t *th = theme_get();

    // Working buffers in PSRAM — kept out of the tight internal-RAM budget
    // (TLS/LVGL). The FFT runs ~20 fps on the LVGL task, so the slower PSRAM
    // access is irrelevant.
    s_raw = heap_caps_malloc(VU_FFT_N * sizeof(int16_t),   MALLOC_CAP_SPIRAM);
    s_win = heap_caps_malloc(VU_FFT_N * sizeof(float),     MALLOC_CAP_SPIRAM);
    s_cf  = heap_caps_malloc(VU_FFT_N * 2 * sizeof(float), MALLOC_CAP_SPIRAM);
    if (!s_raw || !s_win || !s_cf) {
        ESP_LOGE(TAG, "buffer alloc failed");
        free(s_raw); free(s_win); free(s_cf);
        s_raw = NULL; s_win = NULL; s_cf = NULL;
        return;
    }

    // FFT twiddle table once (global, persists across recreate); Hann + bins each
    // create (buffers are freshly allocated).
    if (!s_table_ready) {
        esp_err_t e = dsps_fft2r_init_fc32(NULL, VU_FFT_N);
        if (e != ESP_OK && e != ESP_ERR_DSP_REINITIALIZED) {
            ESP_LOGE(TAG, "fft init failed: 0x%x", e);
            free(s_raw); free(s_win); free(s_cf);
            s_raw = NULL; s_win = NULL; s_cf = NULL;
            return;
        }
        s_table_ready = true;
    }
    dsps_wind_hann_f32(s_win, VU_FFT_N);
    build_bins();

    s_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(s_cont);
    lv_obj_set_size(s_cont, w, h);
    lv_obj_set_pos(s_cont, x, y);
    lv_obj_set_style_bg_color(s_cont, lv_color_hex(th->bg_primary), 0);
    lv_obj_set_style_bg_opa(s_cont, LV_OPA_COVER, 0);   // opaque → no full-screen recomposite
    lv_obj_clear_flag(s_cont, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    s_h = h;

    const int slot  = w / s_nbars;
    const int bar_w = (slot * 7) / 10;
    const int pad   = (w - slot * s_nbars) / 2;   // centre the row

    for (int b = 0; b < s_nbars; b++) {
        lv_obj_t *bar = lv_obj_create(s_cont);
        lv_obj_remove_style_all(bar);
        lv_obj_set_width(bar, bar_w);
        lv_obj_set_height(bar, 1);
        lv_obj_set_style_radius(bar, 2, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(th->accent), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(bar, pad + b * slot, s_h - 1);
        s_bars[b] = bar;
        s_lvl[b]  = 0.0f;
    }

    s_last_count = audio_levels_count();

    // Spin up the FFT task before the render timer; if it fails to start the
    // render timer still runs and bars simply stay at rest.
    s_fft_done = xSemaphoreCreateBinary();
    s_fft_run  = true;
    if (!s_fft_done ||
        xTaskCreatePinnedToCore(fft_task, "vu_fft", VU_FFT_STACK, NULL,
                                VU_FFT_PRIO, &s_fft_task, VU_FFT_CORE) != pdPASS) {
        ESP_LOGE(TAG, "fft task create failed");
        s_fft_run  = false;
        s_fft_task = NULL;
        if (s_fft_done) { vSemaphoreDelete(s_fft_done); s_fft_done = NULL; }
    }

    s_timer = lv_timer_create(tick_cb, VU_TICK_MS, NULL);
    ESP_LOGI(TAG, "Created (%dx%d, %d bars, N=%d, core %d)",
             w, h, s_nbars, VU_FFT_N, VU_FFT_CORE);
}

void vu_widget_destroy(void)
{
    // Stop the FFT task and wait for it to exit BEFORE freeing its buffers,
    // otherwise the task could touch freed PSRAM mid-frame (use-after-free).
    if (s_fft_task) {
        s_fft_run = false;
        xSemaphoreTake(s_fft_done, portMAX_DELAY);
        s_fft_task = NULL;
    }
    if (s_fft_done) { vSemaphoreDelete(s_fft_done); s_fft_done = NULL; }

    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_cont)  { lv_obj_delete(s_cont);  s_cont  = NULL; }  // frees the bars too
    free(s_raw); free(s_win); free(s_cf);
    s_raw = NULL; s_win = NULL; s_cf = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

void vu_widget_apply_theme(void)
{
    if (!s_cont) return;
    const ui_theme_colors_t *th = theme_get();
    lv_obj_set_style_bg_color(s_cont, lv_color_hex(th->bg_primary), 0);
    for (int b = 0; b < s_nbars; b++)
        lv_obj_set_style_bg_color(s_bars[b], lv_color_hex(th->accent), 0);
}
