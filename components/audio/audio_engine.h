#pragma once

#include <stdbool.h>
#include <stdint.h>

// Audio source for the single, shared pipeline. Radio streams come from the
// ICY/HTTP reader, local files from the fatfs reader. The two are mutually
// exclusive — they share the rsp -> dsp -> i2s output tail, the DSP (EQ +
// volume) and the worker tasks, so there is one engine, not two players.
typedef enum {
    AUDIO_SRC_HTTP = 0,
    AUDIO_SRC_FILE,
} audio_src_t;

typedef enum {
    AUDIO_CODEC_UNKNOWN = -1,
    AUDIO_CODEC_MP3 = 0,
    AUDIO_CODEC_AAC,
    AUDIO_CODEC_FLAC,
    AUDIO_CODEC_WAV,
} audio_codec_t;

// Builds the pipeline, elements and worker tasks. Call once at boot, before
// audio_net_player_init / audio_file_player_init (which only register hooks).
void audio_engine_init(void);

// Async: relink the pipeline to <src> -> <codec> -> rsp -> dsp -> i2s, set the
// URI and run. Returns immediately; the heavy work happens on the audio task.
// file_duration_ms > 0 arms the deterministic end-of-file timer (WAV, whose
// length we can read from the header); 0 means "unknown" → end is detected from
// the decoder's STATE_FINISHED event (MP3/FLAC/AAC files and radio streams).
void audio_engine_play(audio_src_t src, audio_codec_t codec,
                       const char *uri, uint32_t file_duration_ms);

// Built-in channel-test signal shape.
typedef enum {
    AUDIO_TEST_PINK = 0,   // pink noise (default; gentle broadband)
    AUDIO_TEST_WHITE,      // white noise (flat broadband hiss)
    AUDIO_TEST_SINE,       // pure sine tone (fixed ~1 kHz)
} audio_test_signal_t;

// Play a short built-in stereo channel-test signal: <ms_per_channel> on the left
// channel, a brief gap, then the same on the right. Needs no SD card or network
// — it lets a user verify the I2S wiring / amp / speakers "just work", handy
// after a runtime pin-map change. Async: relinks to a raw PCM source, runs on
// its own task, and tears down to silence when finished (the previous playback
// is stopped, not resumed). Runs through the DSP, so the current volume applies.
// ms_per_channel is clamped to [200, 5000]. No-op if a test is already running.
void audio_engine_play_test_tone(uint32_t ms_per_channel, audio_test_signal_t signal);

// Set the byte offset for the NEXT HTTP play, sent as a Range header to resume a
// podcast mid-file. Call before audio_engine_play; 0 = play from the start. Only
// affects AUDIO_SRC_HTTP. Persists across an internal codec-relink of that play.
void audio_engine_set_http_offset(uint32_t bytes);

// Synchronous teardown (stop/terminate/reset). Safe from any normal task, but
// NOT from the event-listener task — stopping the pipeline from inside the
// listener overflows the event queue (FreeRTOS assert).
void audio_engine_stop(void);

// Async teardown on the audio task. Safe from small/latency-sensitive callers.
void audio_engine_request_stop(void);

// Pause / resume the running pipeline in place (keeps the position; no teardown).
void audio_engine_pause(void);
void audio_engine_resume(void);
bool audio_engine_is_paused(void);

// Restart the currently-linked pipeline in place (stop/terminate/reset/run)
// without relinking. Used by the radio retry policy. Same event-task caveat as
// audio_engine_stop.
void audio_engine_restart_current(void);

// Drop the "playing" state without tearing the pipeline down. The radio
// give-up path uses this from the event-listener task, where a synchronous
// stop() is unsafe (event-queue overflow → FreeRTOS assert).
void audio_engine_mark_stopped(void);

bool audio_engine_is_playing(void);

void audio_engine_set_volume(int volume);
// Volume taper. true (default) = the 0..100 position is raised to a power before
// it becomes DSP gain, so the quiet end of the travel is usable; false = gain
// tracks the number linearly, which is far louder at low settings. Applies live
// — the current volume is re-pushed at the new curve. Bluetooth is unaffected
// (the module scales in hardware, outside the DSP).
void audio_engine_set_volume_log(bool logarithmic);
void audio_engine_set_eq_10(int *bands);
void audio_engine_set_eq_enabled(bool enabled);
// Mono downmix (L+R averaged into both channels) in the DSP. Applies to every
// pipeline source; the Bluetooth module feeds the amp on its own I2S and is not
// affected. Suspended for the duration of the channel-test tone.
void audio_engine_set_mono(bool mono);

// --- Domain hooks, registered by the net/file layers ------------------------
// ICY StreamTitle metadata from the radio reader (net layer).
void audio_engine_set_meta_cb(void (*cb)(const char *icy_meta));
// A decoder reported MUSIC_INFO → playback is healthy (net cancels retry).
void audio_engine_set_info_cb(void (*cb)(void));
// The HTTP reader reported a status. The argument is the AEL_STATUS_* code; the
// net layer decides whether to retry (a radio stream has no legitimate "end").
void audio_engine_set_error_cb(void (*cb)(int status));
// A local file reached its end (timer or STATE_FINISHED). The pipeline has
// already been stopped when this fires; the file layer restores the prev source.
void audio_engine_set_file_end_cb(void (*cb)(void));
