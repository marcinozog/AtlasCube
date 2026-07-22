/*
 * ts_demux — minimal MPEG-TS (188-byte packet) demultiplexer that extracts the
 * audio elementary stream from an HLS .ts segment stream.
 *
 * It is intentionally tiny: it locates the audio PID from PAT/PMT (assuming both
 * sections fit in a single TS packet, which holds for typical HLS streams),
 * strips the TS/adaptation/PES headers and concatenates the elementary payload.
 * The output is the raw elementary byte stream — ADTS AAC (preferred when the
 * program offers several audio streams) or MPEG audio (MP2/MP3) — suitable for
 * esp-adf's decoders, which resync on the frame sync word, so occasional header
 * mishandling is tolerated rather than fatal. ts_demux_codec() reports which
 * codec the PMT declared once the audio PID is known.
 *
 * The demuxer is fed arbitrary byte chunks (recv() boundaries do not need to
 * align to 188) and keeps a partial-packet accumulator across calls.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ts_demux ts_demux_t;

typedef enum {
    TS_DEMUX_CODEC_UNKNOWN = 0,  /*!< Audio PID not identified yet */
    TS_DEMUX_CODEC_AAC,          /*!< stream_type 0x0F (ADTS) or 0x11 (LATM) */
    TS_DEMUX_CODEC_MPEG,         /*!< stream_type 0x03/0x04 — MPEG-1/2 audio (MP2/MP3) */
} ts_demux_codec_t;

/**
 * @brief Create a TS demuxer instance (heap-allocated).
 * @return Handle or NULL on OOM.
 */
ts_demux_t *ts_demux_create(void);

/**
 * @brief Reset packet-assembly state (call at the start of each segment).
 *
 * The discovered PMT/audio PIDs are kept, since all segments of one media
 * playlist share the same program — only the partial-packet accumulator is
 * cleared so a new segment starts on a fresh 188-byte boundary.
 */
void ts_demux_reset_segment(ts_demux_t *d);

/**
 * @brief Feed TS bytes, emit demuxed ADTS bytes.
 *
 * Output never exceeds input (headers are only stripped), so an @p out buffer
 * at least @p in_len bytes large is always sufficient. All of @p in is consumed
 * (any trailing partial packet is buffered internally).
 *
 * @param d        Demuxer handle
 * @param in       Input TS bytes
 * @param in_len   Number of input bytes
 * @param out      Output buffer for ADTS bytes
 * @param out_cap  Capacity of @p out (must be >= in_len)
 * @return Number of ADTS bytes written to @p out
 */
int ts_demux_process(ts_demux_t *d, const uint8_t *in, int in_len,
                     uint8_t *out, int out_cap);

/**
 * @brief Whether the audio PID has been identified from PAT/PMT yet.
 */
bool ts_demux_has_audio(const ts_demux_t *d);

/**
 * @brief Codec of the selected audio stream, per the PMT's stream_type.
 *
 * Valid once ts_demux_has_audio() is true; TS_DEMUX_CODEC_UNKNOWN before that
 * (or for an exotic stream_type the demuxer passed through anyway).
 */
ts_demux_codec_t ts_demux_codec(const ts_demux_t *d);

/**
 * @brief Destroy a demuxer instance.
 */
void ts_demux_destroy(ts_demux_t *d);

#ifdef __cplusplus
}
#endif
