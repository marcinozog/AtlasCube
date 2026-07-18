#include "audio_levels.h"
#include <math.h>

// Power-of-two ring so the index masks with a single AND. 4096 mono samples is
// ~93 ms at 44.1 kHz — comfortably more than the FFT window the widget pulls
// (≤2048), so the reader always has a full, recent window to work with.
#define RING_SIZE 4096
#define RING_MASK (RING_SIZE - 1)

static int16_t           s_ring[RING_SIZE];
static volatile uint32_t s_widx = 0;   // total mono samples ever written (wraps in days)
static volatile float    s_rms_l = 0.0f;  // per-chunk linear RMS 0..1 (needle VU)
static volatile float    s_rms_r = 0.0f;

void audio_levels_push(const int16_t *samples, int n_samples, int channels)
{
    if (!samples || n_samples <= 0) return;

    uint32_t w = s_widx;
    // Sum of squares per channel rides along the existing mono-mix loop: two
    // extra MACs per frame on the S3's FPU, cheap enough for the audio hot path.
    float sq_l = 0.0f, sq_r = 0.0f;
    int frames = 0;
    if (channels == 2) {
        // Interleaved L,R → average to mono.
        for (int i = 0; i + 1 < n_samples; i += 2) {
            float fl = (float)samples[i], fr = (float)samples[i + 1];
            sq_l += fl * fl;
            sq_r += fr * fr;
            int32_t m = ((int32_t)samples[i] + (int32_t)samples[i + 1]) >> 1;
            s_ring[w & RING_MASK] = (int16_t)m;
            w++;
        }
        frames = n_samples / 2;
    } else {
        for (int i = 0; i < n_samples; i++) {
            float f = (float)samples[i];
            sq_l += f * f;
            s_ring[w & RING_MASK] = samples[i];
            w++;
        }
        sq_r = sq_l;
        frames = n_samples;
    }
    if (frames > 0) {
        s_rms_l = sqrtf(sq_l / (float)frames) * (1.0f / 32768.0f);
        s_rms_r = sqrtf(sq_r / (float)frames) * (1.0f / 32768.0f);
    }
    s_widx = w;   // publish the new write position last
}

void audio_levels_get_stereo(float *l, float *r)
{
    if (l) *l = s_rms_l;
    if (r) *r = s_rms_r;
}

bool audio_levels_snapshot(int16_t *out, int count)
{
    if (!out || count <= 0 || count > RING_SIZE) return false;

    uint32_t w = s_widx;
    if (w < (uint32_t)count) return false;   // not enough history yet

    uint32_t start = w - (uint32_t)count;
    for (int i = 0; i < count; i++) {
        out[i] = s_ring[(start + (uint32_t)i) & RING_MASK];
    }
    return true;
}

uint32_t audio_levels_count(void)
{
    return s_widx;
}
