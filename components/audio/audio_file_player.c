#include "audio_file_player.h"
#include "audio_engine.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdint.h>
#include <strings.h>

static const char *TAG = "AUDIO_FILE";

static void (*s_finished_cb)(void) = NULL;


/*
static audio_codec_t detect_codec_from_path(const char *path)
Extension-based detection for local files. Unlike a radio URL there is always a
real extension, so there is no MP3 fallback here — an unknown extension is
reported as UNKNOWN and the caller decides.
*/
static audio_codec_t detect_codec_from_path(const char *path)
{
    if (!path) return AUDIO_CODEC_UNKNOWN;

    if (strcasestr(path, ".flac")) return AUDIO_CODEC_FLAC;
    if (strcasestr(path, ".aac") ||
        strcasestr(path, ".aacp")) return AUDIO_CODEC_AAC;
    if (strcasestr(path, ".wav"))  return AUDIO_CODEC_WAV;
    if (strcasestr(path, ".mp3"))  return AUDIO_CODEC_MP3;

    return AUDIO_CODEC_UNKNOWN;
}


/*
static uint32_t wav_duration_ms(const char *path)
Reads the 44-byte WAV header and computes the clip length from the actual file
size and the PCM byte rate. Returns 0 if the file can't be opened/parsed — the
engine then falls back to the STATE_FINISHED event for end detection.
*/
static uint32_t wav_duration_ms(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    uint8_t h[44];
    size_t got = fread(h, 1, sizeof(h), f);
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fclose(f);

    if (got < sizeof(h) || fsize <= (long)sizeof(h)) return 0;

    uint32_t sr   = h[24] | (h[25] << 8) | (h[26] << 16) | ((uint32_t)h[27] << 24);
    uint16_t ch   = h[22] | (h[23] << 8);
    uint16_t bits = h[34] | (h[35] << 8);
    uint32_t byte_rate = sr * ch * (bits / 8);
    if (byte_rate == 0) return 0;

    uint64_t data_bytes = (uint64_t)(fsize - (long)sizeof(h));
    return (uint32_t)(data_bytes * 1000ULL / byte_rate);
}


/*
static void on_file_end(void)
Engine hook: the current file reached its end (the engine has already torn the
pipeline down). Forward to whoever wants to restore the previous source.
*/
static void on_file_end(void)
{
    if (s_finished_cb) s_finished_cb();
}


void audio_file_player_init(void)
{
    audio_engine_set_file_end_cb(on_file_end);
}


void audio_file_player_set_finished_cb(void (*cb)(void))
{
    s_finished_cb = cb;
}


void audio_file_player_play(const char *path)
{
    if (!path) return;

    audio_codec_t codec = detect_codec_from_path(path);
    if (codec == AUDIO_CODEC_UNKNOWN) {
        ESP_LOGW(TAG, "Unknown file extension, assuming MP3: %s", path);
        codec = AUDIO_CODEC_MP3;
    }

    // Only WAV has a length we can read up-front; for compressed formats the
    // engine relies on the decoder's STATE_FINISHED event.
    uint32_t duration_ms = (codec == AUDIO_CODEC_WAV) ? wav_duration_ms(path) : 0;

    audio_engine_play(AUDIO_SRC_FILE, codec, path, duration_ms);
}
