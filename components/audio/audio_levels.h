#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Lock-free mono sample ring, fed from the audio DSP element on core 1 and read
// by the VU spectrum widget on the LVGL task (also core 1). The writer only ever
// advances a single 32-bit index and the reader copies a recent window — a torn
// sample across a preemption boundary is visually irrelevant for a meter, so no
// mutex: aligned 32-bit index loads/stores are atomic enough on the ESP32-S3.
//
// The FFT itself is deliberately NOT done here — the widget pulls a window from
// this ring and transforms it on the LVGL task, keeping the audio hot path to a
// cheap copy (the decode/rsp/dsp tasks are glitch-sensitive).

// Append PCM to the ring, downmixing to mono. `samples` is interleaved int16,
// `n_samples` the total int16 count (frames * channels), `channels` is 1 or 2.
void audio_levels_push(const int16_t *samples, int n_samples, int channels);

// Copy the `count` most recent mono samples, oldest-first, into `out`. Returns
// false until the ring holds at least `count` samples of history.
bool audio_levels_snapshot(int16_t *out, int count);

// Total mono samples ever written. A reader that sees this unchanged between
// ticks knows no new audio arrived (paused / stopped) and can decay its meter
// instead of re-transforming a stale window.
uint32_t audio_levels_count(void);

// Latest per-channel linear RMS (0..1), computed over the most recent pushed
// chunk. Feeds the needle VU, which wants a level per channel rather than a
// spectrum. Mono input reports the same value on both. Each value is a single
// aligned 32-bit float store on the writer side, so reads need no lock; use
// audio_levels_count() to detect a stalled (paused) stream.
void audio_levels_get_stereo(float *l, float *r);

#ifdef __cplusplus
}
#endif
