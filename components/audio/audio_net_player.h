#pragma once

// Internet-radio source (ICY/HTTP) for the shared audio engine. Owns URL codec
// detection, ICY StreamTitle parsing and the stream-loss retry policy. The
// pipeline, decoders and output tail live in audio_engine.

// Registers the net layer's hooks (metadata / info / error) with the engine and
// starts the retry task. Call once at boot, after audio_engine_init.
void audio_net_player_init(void);

// Async: start playing a radio stream URL.
void audio_net_player_play(const char *url);
