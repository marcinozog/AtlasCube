#include "audio_levels.h"

// Power-of-two ring so the index masks with a single AND. 4096 mono samples is
// ~93 ms at 44.1 kHz — comfortably more than the FFT window the widget pulls
// (≤2048), so the reader always has a full, recent window to work with.
#define RING_SIZE 4096
#define RING_MASK (RING_SIZE - 1)

static int16_t           s_ring[RING_SIZE];
static volatile uint32_t s_widx = 0;   // total mono samples ever written (wraps in days)

void audio_levels_push(const int16_t *samples, int n_samples, int channels)
{
    if (!samples || n_samples <= 0) return;

    uint32_t w = s_widx;
    if (channels == 2) {
        // Interleaved L,R → average to mono.
        for (int i = 0; i + 1 < n_samples; i += 2) {
            int32_t m = ((int32_t)samples[i] + (int32_t)samples[i + 1]) >> 1;
            s_ring[w & RING_MASK] = (int16_t)m;
            w++;
        }
    } else {
        for (int i = 0; i < n_samples; i++) {
            s_ring[w & RING_MASK] = samples[i];
            w++;
        }
    }
    s_widx = w;   // publish the new write position last
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
