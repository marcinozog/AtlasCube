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
void audio_engine_set_eq_10(int *bands);
void audio_engine_set_eq_enabled(bool enabled);

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
