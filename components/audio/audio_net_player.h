#pragma once

#include <stdbool.h>
#include <stdint.h>

// Internet-radio source (ICY/HTTP) for the shared audio engine. Owns URL codec
// detection, ICY StreamTitle parsing and the stream-loss retry policy. The
// pipeline, decoders and output tail live in audio_engine.

// Registers the net layer's hooks (metadata / info / error) with the engine and
// starts the retry task. Call once at boot, after audio_engine_init.
void audio_net_player_init(void);

// Async: start playing an HTTP stream URL. finite=false for endless radio
// (an end-of-stream is a loss → retry). finite=true for a podcast episode
// (EOF is a clean finish → engine marked stopped, no retry). offset_bytes>0
// resumes a finite stream mid-file via a Range request (0 = from the start).
void audio_net_player_play(const char *url, bool finite, uint32_t offset_bytes);
