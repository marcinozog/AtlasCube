#pragma once

// Local-file source (SD/FAT) for the shared audio engine. Owns codec detection
// from the file extension and WAV length probing. The pipeline, decoders and
// output tail live in audio_engine.

// Registers the file-end hook with the engine. Call once at boot, after
// audio_engine_init.
void audio_file_player_init(void);

// Plays a local audio file (mp3/wav/flac/aac) from the mounted SD/FAT
// filesystem, e.g. "/sdcard/music/track.mp3" or
// "/sdcard/voice/wake-up-a3f9c1.wav". Unlike a radio stream the file has a real
// end: when it finishes the pipeline is torn down and the finished callback (if
// set) is invoked — the radio retry logic does NOT kick in. Async.
void audio_file_player_play(const char *path);

// Registered by the layer that knows how to restore the previous audio source
// (radio_service). Called from an internal task when a file started via
// audio_file_player_play reaches its end. NULL to detach.
void audio_file_player_set_finished_cb(void (*cb)(void));
