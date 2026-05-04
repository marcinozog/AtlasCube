#include "audio_dsp.h"
#include "audio_element.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

static const char *TAG = "AUDIO_DSP";

#define DSP_BANDS       10
#define MAX_CHANNELS     2
#define DSP_Q            1.4142f   // sqrt(2) — octave bands

static const float CENTER_FREQ[DSP_BANDS] = {
    31.0f, 62.0f, 125.0f, 250.0f, 500.0f,
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

/* ---------- biquad peaking filter ---------- */

typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float x1[MAX_CHANNELS], x2[MAX_CHANNELS];
    float y1[MAX_CHANNELS], y2[MAX_CHANNELS];
} biquad_t;

typedef struct {
    /* EQ */
    biquad_t filters[DSP_BANDS];
    int      gain_db[DSP_BANDS];
    int      sample_rate;
    int      channels;
    bool     dirty;         // true = recompute coefficients on next process()
    bool     eq_enabled;    // false = bypass the whole biquad loop

    /* Volume */
    float    volume;        // 0.0 .. 1.0, default 1.0
} dsp_ctx_t;


/* ====================================================================
   compute_coeffs — peaking EQ (Audio EQ Cookbook, R. Bristow-Johnson)
   ==================================================================== */
static void compute_coeffs(biquad_t *f, float freq, float gain_db, float fs)
{
    float A     = powf(10.0f, gain_db / 40.0f);
    float w0    = 2.0f * (float)M_PI * freq / fs;
    float cw    = cosf(w0);
    float sw    = sinf(w0);
    float alpha = sw / (2.0f * DSP_Q);
    float a0    = 1.0f + alpha / A;

    f->b0 = (1.0f + alpha * A) / a0;
    f->b1 = (-2.0f * cw)       / a0;
    f->b2 = (1.0f - alpha * A) / a0;
    f->a1 = (-2.0f * cw)       / a0;
    f->a2 = (1.0f - alpha / A) / a0;
}

static void recompute_all(dsp_ctx_t *ctx)
{
    if (ctx->sample_rate <= 0) return;

    for (int i = 0; i < DSP_BANDS; i++) {
        compute_coeffs(&ctx->filters[i],
                       CENTER_FREQ[i],
                       (float)ctx->gain_db[i],
                       (float)ctx->sample_rate);
        // Reset history — avoid artifacts after coefficient change
        for (int ch = 0; ch < MAX_CHANNELS; ch++) {
            ctx->filters[i].x1[ch] = ctx->filters[i].x2[ch] = 0.0f;
            ctx->filters[i].y1[ch] = ctx->filters[i].y2[ch] = 0.0f;
        }
    }

    ctx->dirty = false;
    ESP_LOGI(TAG, "Coefficients recomputed: %d Hz, %d ch", ctx->sample_rate, ctx->channels);
}


/* ---------- audio element callbacks ---------- */

static esp_err_t dsp_open(audio_element_handle_t self)
{
    return ESP_OK;
}

static esp_err_t dsp_close(audio_element_handle_t self)
{
    return ESP_OK;
}

static int dsp_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int bytes_read = audio_element_input(self, in_buffer, in_len);
    if (bytes_read <= 0) return bytes_read;

    dsp_ctx_t *ctx = (dsp_ctx_t *)audio_element_getdata(self);

    // Lazy coefficient update — no pipeline stop
    if (ctx->dirty) {
        recompute_all(ctx);
    }

    int16_t *samples   = (int16_t *)in_buffer;
    int      n_samples = bytes_read / sizeof(int16_t);
    int      ch        = (ctx->channels > 0) ? ctx->channels : 1;

    /* ---------------------------------------------------------------
       Pass 1: 10-band EQ (sequential biquad filters, Direct Form I)
       Bands with gain == 0 are skipped. The whole section is skipped
       when EQ is disabled by the user.
       --------------------------------------------------------------- */
    if (ctx->eq_enabled)
    for (int b = 0; b < DSP_BANDS; b++) {
        if (ctx->gain_db[b] == 0) continue;

        biquad_t *f = &ctx->filters[b];

        for (int i = 0; i < n_samples; i++) {
            int c = i % ch;

            float x = (float)samples[i];
            float y = f->b0 * x
                    + f->b1 * f->x1[c] + f->b2 * f->x2[c]
                    - f->a1 * f->y1[c] - f->a2 * f->y2[c];

            f->x2[c] = f->x1[c]; f->x1[c] = x;
            f->y2[c] = f->y1[c]; f->y1[c] = y;

            // Hard clip after each band — prevents saturation accumulation
            if (y >  32767.0f) y =  32767.0f;
            if (y < -32768.0f) y = -32768.0f;
            samples[i] = (int16_t)y;
        }
    }

    /* ---------------------------------------------------------------
       Pass 2: Volume — fixed-point int16, one multiplier per sample.
       Skipped entirely when volume == 1.0.
       --------------------------------------------------------------- */
    float vol = ctx->volume;
    if (vol < 1.0f) {
        int32_t gain = (int32_t)(vol * 32768.0f);

        for (int i = 0; i < n_samples; i++) {
            int32_t s = (int32_t)samples[i] * gain >> 15;
            if (s >  32767) s =  32767;
            if (s < -32768) s = -32768;
            samples[i] = (int16_t)s;
        }
    }

    return audio_element_output(self, in_buffer, bytes_read);
}


/* ====================================================================
   audio_dsp_init
   ==================================================================== */
audio_element_handle_t audio_dsp_init(int sample_rate, int channels)
{
    dsp_ctx_t *ctx = calloc(1, sizeof(dsp_ctx_t));
    if (!ctx) return NULL;

    ctx->sample_rate = (sample_rate > 0) ? sample_rate : 44100;
    ctx->channels    = (channels   > 0) ? channels    : 2;
    ctx->volume      = 1.0f;
    ctx->dirty       = false;
    ctx->eq_enabled  = true;

    memset(ctx->gain_db, 0, sizeof(ctx->gain_db));
    recompute_all(ctx);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open         = dsp_open;
    cfg.close        = dsp_close;
    cfg.process      = dsp_process;
    cfg.buffer_len   = 2048;
    cfg.out_rb_size  = 8 * 1024;
    cfg.task_stack   = 4096;
    cfg.task_prio    = 5;
    cfg.tag          = "dsp";
    // Stack in internal SRAM — biquad/floating point in PSRAM caused
    // micro-glitches on 256 kbps streams (PSRAM ~5× slower).
    cfg.stack_in_ext = false;
    // Core 1 — offloads core 0 (Wi-Fi/lwIP/HTTP) at higher bitrates.
    cfg.task_core    = 1;

    audio_element_handle_t el = audio_element_init(&cfg);
    if (!el) { free(ctx); return NULL; }

    audio_element_setdata(el, ctx);
    ESP_LOGI(TAG, "DSP initialized: %d Hz, %d ch", ctx->sample_rate, ctx->channels);
    return el;
}


/* ====================================================================
   audio_dsp_set_gains
   Call from any task — no reopen, no playback interruption.
   ==================================================================== */
void audio_dsp_set_gains(audio_element_handle_t el, int *gains_db)
{
    if (!el) return;
    dsp_ctx_t *ctx = (dsp_ctx_t *)audio_element_getdata(el);
    if (!ctx) return;

    for (int i = 0; i < DSP_BANDS; i++) {
        ctx->gain_db[i] = gains_db[i];
    }
    ctx->dirty = true;
}


/* ====================================================================
   audio_dsp_set_volume
   ==================================================================== */
void audio_dsp_set_volume(audio_element_handle_t el, float volume)
{
    if (!el) return;
    dsp_ctx_t *ctx = (dsp_ctx_t *)audio_element_getdata(el);
    if (!ctx) return;

    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    ctx->volume = volume;
}


/* ====================================================================
   audio_dsp_set_eq_enabled
   ==================================================================== */
void audio_dsp_set_eq_enabled(audio_element_handle_t el, bool enabled)
{
    if (!el) return;
    dsp_ctx_t *ctx = (dsp_ctx_t *)audio_element_getdata(el);
    if (!ctx) return;
    ctx->eq_enabled = enabled;
    ESP_LOGI(TAG, "EQ %s", enabled ? "enabled" : "disabled (bypass)");
}


/* ====================================================================
   audio_dsp_set_sample_rate
   Call from audio_event_task when decoder emits MUSIC_INFO.
   ==================================================================== */
void audio_dsp_set_sample_rate(audio_element_handle_t el, int sample_rate, int channels)
{
    if (!el) return;
    dsp_ctx_t *ctx = (dsp_ctx_t *)audio_element_getdata(el);
    if (!ctx) return;

    if (ctx->sample_rate == sample_rate && ctx->channels == channels) return;

    ctx->sample_rate = sample_rate;
    ctx->channels    = channels;
    ctx->dirty       = true;
}