#include "vu_widget.h"
#include "audio_levels.h"   // PCM ring tapped from the DSP element
#include "theme.h"
#include "esp_dsp.h"        // dsps_fft2r_*, dsps_wind_hann_f32
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Shared EQ band centres, defined in audio_dsp.c (declared in audio_dsp.h).
// Forward-declared here to avoid pulling the esp-adf audio_element.h chain into
// the UI for a single accessor.
int audio_dsp_band_freqs(const float **freqs);

static const char *TAG = "VU_WIDGET";

// ── Tunables ────────────────────────────────────────────────────────────────
// N=1024 → 43 Hz/bin at 44.1 kHz, window ~23 ms. A longer window resolves the
// sub-60 Hz EQ octaves better but smears transients (time/frequency trade-off),
// so 1024 is the sweet spot: snappy, and the ~125 Hz-and-up bands (the audible
// ones on this hardware) still line up with the EQ. Bars sit on the EQ's own
// band centres (2 per octave) so meter and equaliser describe the same bands.
#define VU_FFT_N      1024         // FFT window (power of two)
#define VU_BARS_MAX   20           // static ceiling = 2 * DSP_BANDS (10)
#define VU_TICK_MS    50           // refresh period (~20 fps; only this small rect redraws)
#define VU_SAMPLE_HZ  44100.0f     // PLAYBACK_SAMPLE_RATE (fixed post-rsp)
#define VU_FREQ_TOP   17000.0f     // clamp the top band below Nyquist so the last
                                   // bar doesn't peak on >16 kHz codec noise
#define VU_DB_FLOOR   (-18.0f)     // peak power(dB) mapped to bar = 0
#define VU_DB_CEIL     44.0f       // peak power(dB) mapped to bar = full height
#define VU_ATTACK      0.8f        // smoothing when a bar rises (fast)
#define VU_DECAY       0.22f       // smoothing when a bar falls (slow → VU feel)

#define VU_SQRT2       1.41421356f // octave half-width (centres are octave-spaced)

// ── Display objects ──────────────────────────────────────────────────────────
static lv_obj_t   *s_cont = NULL;
static lv_obj_t   *s_bars[VU_BARS_MAX];
static lv_timer_t *s_timer = NULL;
static int         s_h      = 0;   // container inner height (bar baseline)
static int         s_nbars  = 0;   // active bar count (2 per EQ band)

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

// Lay 2 bars on each EQ octave band: the lower and upper half-octave around the
// band centre. Boundary between the two sub-bars sits exactly on the centre.
static void build_bins(void)
{
    const float *fc = NULL;
    int bands = audio_dsp_band_freqs(&fc);
    int bars  = bands * 2;
    if (bars > VU_BARS_MAX) bars = VU_BARS_MAX;
    s_nbars = bars;

    for (int b = 0; b < bands && (2 * b + 1) < VU_BARS_MAX; b++) {
        float lo  = fc[b] / VU_SQRT2;
        float mid = fc[b];
        float hi  = fc[b] * VU_SQRT2;
        if (mid > VU_FREQ_TOP) mid = VU_FREQ_TOP;   // keep the top bar narrow,
        if (hi  > VU_FREQ_TOP) hi  = VU_FREQ_TOP;    // away from the Nyquist noise
        edge_to_bins(2 * b,     lo,  mid);
        edge_to_bins(2 * b + 1, mid, hi);
    }
}

// Push the current s_lvl[] into bar heights. Only the container area invalidates.
static void render_bars(void)
{
    for (int b = 0; b < s_nbars; b++) {
        int hgt = (int)(s_lvl[b] * (float)s_h + 0.5f);
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

static void tick_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_table_ready || !s_cf) return;

    // No new audio since last tick (paused / stopped): decay to rest.
    uint32_t count = audio_levels_count();
    if (count == s_last_count || !audio_levels_snapshot(s_raw, VU_FFT_N)) {
        for (int b = 0; b < s_nbars; b++) smooth_to(b, 0.0f);
        render_bars();
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
    s_timer = lv_timer_create(tick_cb, VU_TICK_MS, NULL);
    ESP_LOGI(TAG, "Created (%dx%d, %d bars, N=%d)", w, h, s_nbars, VU_FFT_N);
}

void vu_widget_destroy(void)
{
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
