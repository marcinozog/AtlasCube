# Audio pipeline — design notes

Notes from implementing internet radio streaming on ESP32-S3 with esp-adf.
Captures non-obvious decisions and the empirical reasoning behind them.

## Pipeline

```
http  →  <decoder>  →  rsp (resample)  →  dsp (EQ + volume)  →  i2s
        mp3 / aac / flac      to 44100 Hz                    fixed 44100/16/2
```

All elements run as separate FreeRTOS tasks. Pipeline is rebuilt on every
codec switch (`audio_pipeline_unlink` + `audio_pipeline_link` with new tag),
not on every play.

## Supported codecs

| Format | Container | Status |
|---|---|---|
| MP3 | raw | ✓ |
| AAC | raw / ADTS | ✓ |
| FLAC | raw (`fLaC` magic) | ✓ |
| OGG/Vorbis | OGG | ✓ (rare) |
| **OGG/FLAC** | OGG | ✗ esp-adf prebuilt has no demuxer |
| **OGG/Opus** | OGG | ✗ requires separate opus_decoder |
| HLS / .m3u8 | playlist | ✗ icy_http_stream is not HLS-aware |

`ogg_decoder` in esp-adf-libs is actually `VORBIS_DECODER` — the auto-detect
header (`auto_ogg_dec.h`) is misleading. Many "FLAC Hi-Fi" Icecast streams
serve `audio/ogg` with FLAC payload and **will not work**. Look for streams
with `Content-Type: audio/flac` (raw FLAC).

## Codec detection (hybrid)

1. **URL hint** (zero-cost): `.flac` / `.aac` / `.aacp` / `.mp3` / `/aac` etc.
   Only matches with explicit dot prefix for FLAC — bare "flac" in path can
   be OGG/FLAC (e.g. `juventus_FLAC`).
2. **HTTP probe** (~100-300 ms): if URL has no hint, opens a separate
   `esp_http_client` with `Range: bytes=0-1`, reads `Content-Type` via
   `HTTP_EVENT_ON_HEADER` callback, maps to codec.
3. **Fallback**: MP3 (most common in shoutcast streams without metadata).

`esp_http_client_get_header(client, "Content-Type", ...)` returns NULL on
some configs — the event handler is the reliable path.

## Sample rate

I2S is **fixed at 44100/16/2**. `rsp_filter` (esp-adf) handles upsampling
from anything the decoder produces (22050 for AAC LC, 32000, 48000 for
FLAC, etc.) to playback rate.

Why not dynamic `i2s_stream_set_clk()`:
- Some DAC/codec combos do not generate a stable clock at non-standard
  rates (22050 produced silence/noise on this board).
- `set_clk` does internal PAUSE → reconfig → RESUME. Two consecutive calls
  (one from `MUSIC_INFO`, one from `STATE_RUNNING`) caused `RESUME timeout`.
- Fixed playback rate sidesteps both problems at the cost of a resampler
  pass (~5% CPU).

## Threading model

Three dedicated tasks instead of inline work:

- **`audio_evt`** (5 KB stack) — listens on event iface, dispatches
  `MUSIC_INFO` → `rsp_filter_set_src_info`, dispatches HTTP errors to retry.
- **`audio_retry`** (4 KB stack) — restarts pipeline after stream loss
  (separate task because `audio_pipeline_stop` from inside the event
  listener overflows the event queue → FreeRTOS assert).
- **`audio_play`** (8 KB stack) — runs `audio_player_play` payload
  (HTTP probe + TLS handshake + pipeline rebuild). Without this, calling
  `audio_player_play` from a WebSocket handler caused stack overflow in
  the `httpd` task (~4 KB stack), because `esp_http_client` + TLS uses
  4-6 KB just for the handshake.

Front-end is a binary semaphore + mutex on the URL buffer. Spam-clicking
stations is safe — only the latest URL is honored.

Event listener is bound **per-element**, not via `audio_pipeline_set_listener`,
because the latter is silently dropped by `audio_pipeline_unlink`. Per-element
binding survives unlink/link cycles.

### Task affinity

Audio elements are pinned to **core 1**, network/IO stays on **core 0**:

| Element | Core | Reason |
|---|---|---|
| http reader | 0 | Lives next to Wi-Fi/lwIP — same chain |
| mp3/aac/flac decoder | 1 | CPU-heavy, must not contend with Wi-Fi |
| rsp_filter | 1 | Same — FIR resample is FP-heavy |
| dsp (EQ + volume) | 1 | Same — biquads are FP-heavy |
| i2s writer | 0 | Lives next to DMA peripheral |

Without this split, all audio tasks defaulted to core 0 and competed with
Wi-Fi RX for cycles. Symptoms: 256 kbps streams crackled even when total
CPU was only ~30%. After moving decoder/rsp/dsp to core 1 the load
redistributed (typical: core 0 ~12%, core 1 ~50% with EQ on) and the
crackling went away.

## DSP (EQ + volume)

10-band peaking biquad EQ + soft volume, single audio_element.

- Bands with `gain_db == 0` are skipped per-block (cheap default).
- `eq_enabled` flag bypasses the entire EQ loop. Volume runs regardless.
  Toggle exposed in Settings UI.
- DSP element stack lives in **internal SRAM** (`stack_in_ext = false`) —
  with PSRAM stack, biquad floating-point math caused micro-glitches at
  ≥256 kbps streams (PSRAM is ~5× slower for random access).

### Why not esp-dsp?

Tested `dsps_biquad_sf32` (Xtensa-optimized) against the naive C loop:

| Test | esp-dsp ON | esp-dsp OFF (naive) | Δ |
|---|---|---|---|
| MP3 128 kbps HTTPS | 40% | 33% | **+7%** |
| MP3 128 kbps HTTP | 35% | 28% | **+7%** |
| AAC 22050 HTTP | 35% | 29% | **+6%** |
| MP3 256 kbps HTTP | 41% | 34% | **+7%** |

esp-dsp was consistently **slower** because:

1. Stereo path `dsps_biquad_sf32` maps to `_ae32` (LX6 ESP32) on S3, not
   the LX7-specific `_aes3` (which exists only for mono `dsps_biquad_f32`).
2. The naive loop converts `int16 → float → int16` inline, in a single pass
   per band. esp-dsp requires a separate float buffer + three passes
   (convert in, biquad, convert out). The extra memory traffic eats the
   SIMD savings.
3. gcc on LX7 with `-O2` already vectorizes the FPU-heavy inner loop well.

Moving the float buffer from PSRAM to internal SRAM made no difference —
the bottleneck is the extra passes, not buffer location.

## Buffers

| Buffer | Size | Location | Reasoning |
|---|---|---|---|
| HTTP `out_rb_size` | 256 KB | PSRAM (auto fallback) | ~8 s at 256 kbps; absorbs Wi-Fi jitter on weak signal |
| Decoder `out_rb_size` | 32 KB | PSRAM | Standard esp-adf pattern |
| `rsp_filter` `out_rb_size` | 8 KB | PSRAM | Just enough for one resample block |
| `dsp` `out_rb_size` | 8 KB | PSRAM | Same as rsp |
| `i2s` `out_rb_size` | 8 KB | PSRAM | DMA descriptors handle the rest |
| DSP task stack | 4 KB | **internal SRAM** | PSRAM stack caused glitches |
| RSP task stack | 4 KB | **internal SRAM** | Same reason |

## TCP tuning (sdkconfig)

Default lwIP settings caused **playback to stutter for the first ~5 s**
of every 256 kbps stream, even with a 256 KB ringbuffer and excellent RSSI:

| Setting | Default | Tuned | Why |
|---|---|---|---|
| `CONFIG_LWIP_TCP_WND_DEFAULT` | 5760 | 32768 | TCP slow-start saturated 5.7 KB window after one MSS, then waited a full RTT. 32 KB = ~1 s at 256 kbps, lets the receiver pull at line rate from packet 1 |
| `CONFIG_LWIP_TCP_RCV_BUF_DEFAULT` | 5760 | 32768 | Mirrors the window — lwIP needs both |
| `CONFIG_LWIP_TCP_RECVMBOX_SIZE` | 6 | 32 | More mbox slots = fewer dropped pbufs under burst |

After tuning, 256 kbps streams play smoothly from the first second.
Without this even strong WiFi (-52 dBm) and idle CPU did not help —
TCP slow-start is a protocol-level limit, no amount of buffering past it
solves the warm-up phase.

## Retry / error handling

- `audio_evt` listens for `AEL_STATUS_ERROR_OPEN` (HTTP failed to open) and
  stream-lost events (`INPUT_DONE` / `STATE_FINISHED` / `ERROR_INPUT` /
  `ERROR_PROCESS`). Internet radio has no "end", so any of these = anomaly.
- Notifies `audio_retry` task. First retry waits 3 s (TLS stack needs time
  to release heap), subsequent retries 2 s. Max 5 attempts then
  `RADIO_STATE_ERROR`.
- If `MUSIC_INFO` arrives between scheduling retry and retry firing, the
  counter resets — retry is skipped because esp-adf may have recovered the
  HTTP element internally.

## What does not work / known limitations

- **OGG container streams** (FLAC, Opus). No esp-adf-libs prebuilt support
  for OGG/FLAC, and OGG/Opus requires `opus_decoder` which we do not link.
  Detected via Content-Type and logged as `OGG container not supported`.
- **HLS** (`.m3u8`). `icy_http_stream` does not parse playlists. Most
  Radio Paradise FLAC endpoints are HLS.
- **Sample rate above ~48 kHz** untested. FLAC streams are usually 44100.
- **Pipeline takes ~300 ms to start** producing audio after `play()` —
  inherent to esp-adf element warm-up + decoder sync.

## Settings persistence

Audio settings stored in `/spiffs/settings.json`:

```json
"audio": {
  "volume": 30,
  "eq": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
  "eq_enabled": true
}
```

Older config files without `eq_enabled` default to `true` on load.

## Performance reference

Measured on ESP32-S3 with this pipeline, MP3 streams.

**Before task affinity split** (everything on core 0):

| Configuration | Core 0 | Core 1 |
|---|---|---|
| MP3 128 kbps, EQ on | 58% | 3% |
| MP3 128 kbps, EQ off | 31% | 4% |
| MP3 256 kbps, EQ on | 58% | 4% |
| MP3 256 kbps, EQ off | 31% | 4% |

**After moving decoder + rsp + dsp to core 1:**

| Configuration | Core 0 | Core 1 |
|---|---|---|
| MP3 256 kbps, EQ on | 10% | 50% |
| MP3 256 kbps, EQ off | 13% | 26% |

Same total work, redistributed away from the Wi-Fi/lwIP path. Bitrate has
no impact on decoder CPU — work is per-sample-output (44100 × 2 ch),
not per-bit-input. EQ on/off is the dominant factor (~24% of audio core).

## Diagnostics on device

The Settings screen bottom bar shows live system stats updated every
second:

```
Free:5800K Int:55K Blk:24K CPU:13/26% RSSI:-52
```

- **Free** — total free heap (PSRAM + internal)
- **Int** — free internal SRAM (the constrained one)
- **Blk** — largest contiguous internal block (TLS handshake needs ~6 KB)
- **CPU:N/M%** — usage per core, derived from `IDLE0` / `IDLE1` task runtime
  counters via `uxTaskGetSystemState` (requires
  `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y` and
  `CONFIG_FREERTOS_USE_TRACE_FACILITY=y`)
- **RSSI** — Wi-Fi signal in dBm, `--` when STA disconnected.
  Reference: -50 great, -65 marginal for 256 kbps, below -75 expect drops
