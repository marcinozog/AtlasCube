/*
 * ts_demux — minimal MPEG-TS → ADTS demultiplexer. See ts_demux.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <stdbool.h>

#include "esp_log.h"
#include "audio_mem.h"

#include "ts_demux.h"

static const char *TAG = "TS_DEMUX";

#define TS_PKT_SIZE 188
#define TS_SYNC     0x47

struct ts_demux {
    uint8_t pkt[TS_PKT_SIZE];
    int     pkt_fill;
    int     pmt_pid;    /* program-map PID from PAT, -1 = unknown */
    int     audio_pid;  /* audio elementary PID from PMT, -1 = unknown */
    bool    pat_done;
    bool    pmt_done;
};

ts_demux_t *ts_demux_create(void)
{
    ts_demux_t *d = audio_calloc(1, sizeof(ts_demux_t));
    if (!d) {
        return NULL;
    }
    d->pmt_pid   = -1;
    d->audio_pid = -1;
    return d;
}

void ts_demux_reset_segment(ts_demux_t *d)
{
    if (d) {
        d->pkt_fill = 0;   /* keep discovered PIDs across segments */
    }
}

bool ts_demux_has_audio(const ts_demux_t *d)
{
    return d && d->audio_pid >= 0;
}

void ts_demux_destroy(ts_demux_t *d)
{
    if (d) {
        audio_free(d);
    }
}

/* Parse a Program Association Table to learn the PMT PID. Assumes the section
 * fits in this one packet (the common case for HLS). */
static void _parse_pat(ts_demux_t *d, const uint8_t *payload, int plen, bool pusi)
{
    if (!pusi || plen < 1) {
        return;
    }
    int ptr = payload[0];
    int base = 1 + ptr;
    if (base + 8 > plen) {
        return;
    }
    const uint8_t *s = payload + base;
    if (s[0] != 0x00) {   /* table_id: PAT */
        return;
    }
    int section_len = ((s[1] & 0x0F) << 8) | s[2];
    int prog_end = 3 + section_len - 4;   /* minus 4-byte CRC */
    if (prog_end > plen - base) {
        prog_end = plen - base;
    }
    for (int i = 8; i + 4 <= prog_end; i += 4) {
        int prog_num = (s[i] << 8) | s[i + 1];
        int pid = ((s[i + 2] & 0x1F) << 8) | s[i + 3];
        if (prog_num != 0) {   /* skip network PID (prog 0) */
            d->pmt_pid = pid;
            d->pat_done = true;
            ESP_LOGD(TAG, "PAT: pmt_pid=0x%x", pid);
            return;
        }
    }
}

/* Pick the most decoder-friendly audio stream type. We feed an ADTS AAC
 * decoder, so 0x0F (AAC ADTS) is ideal; 0x11 (AAC LATM) and MP3 are accepted
 * as a fallback but may not decode cleanly. Higher return = more preferred. */
static int _audio_pref(uint8_t stream_type)
{
    switch (stream_type) {
        case 0x0F: return 3;   /* AAC ADTS */
        case 0x11: return 2;   /* AAC LATM */
        case 0x03:             /* MPEG-1 audio */
        case 0x04: return 1;   /* MPEG-2 audio */
        default:   return 0;
    }
}

/* Parse a Program Map Table to learn the audio elementary PID. Assumes the
 * section fits in this one packet. */
static void _parse_pmt(ts_demux_t *d, const uint8_t *payload, int plen, bool pusi)
{
    if (!pusi || plen < 1) {
        return;
    }
    int ptr = payload[0];
    int base = 1 + ptr;
    if (base + 12 > plen) {
        return;
    }
    const uint8_t *s = payload + base;
    if (s[0] != 0x02) {   /* table_id: PMT */
        return;
    }
    int section_len = ((s[1] & 0x0F) << 8) | s[2];
    int section_end = 3 + section_len - 4;   /* minus CRC */
    if (section_end > plen - base) {
        section_end = plen - base;
    }
    int prog_info_len = ((s[10] & 0x0F) << 8) | s[11];
    int best_pref = 0;
    int i = 12 + prog_info_len;
    while (i + 5 <= section_end) {
        uint8_t stream_type = s[i];
        int epid = ((s[i + 1] & 0x1F) << 8) | s[i + 2];
        int es_info_len = ((s[i + 3] & 0x0F) << 8) | s[i + 4];
        int pref = _audio_pref(stream_type);
        if (pref > best_pref) {
            best_pref = pref;
            d->audio_pid = epid;
        }
        i += 5 + es_info_len;
    }
    if (d->audio_pid >= 0) {
        d->pmt_done = true;
        ESP_LOGD(TAG, "PMT: audio_pid=0x%x (pref=%d)", d->audio_pid, best_pref);
    }
}

/* Process one complete 188-byte packet, appending any audio payload (ADTS) to
 * out. Returns bytes written. */
static int _process_packet(ts_demux_t *d, const uint8_t *pkt, uint8_t *out, int out_cap)
{
    if (pkt[0] != TS_SYNC) {
        return 0;
    }
    if (pkt[1] & 0x80) {   /* transport error indicator */
        return 0;
    }
    bool pusi = pkt[1] & 0x40;
    int  pid  = ((pkt[1] & 0x1F) << 8) | pkt[2];
    int  afc  = (pkt[3] >> 4) & 0x3;

    int off = 4;
    if (afc & 0x2) {                    /* adaptation field present */
        off += 1 + pkt[4];
    }
    if (!(afc & 0x1) || off >= TS_PKT_SIZE) {
        return 0;                       /* no payload */
    }
    const uint8_t *payload = pkt + off;
    int plen = TS_PKT_SIZE - off;

    if (pid == 0 && !d->pat_done) {
        _parse_pat(d, payload, plen, pusi);
        return 0;
    }
    if (d->pmt_pid >= 0 && pid == d->pmt_pid && !d->pmt_done) {
        _parse_pmt(d, payload, plen, pusi);
        return 0;
    }
    if (d->audio_pid < 0 || pid != d->audio_pid) {
        return 0;
    }

    /* Audio PID: on a unit start, skip the PES header to reach the ADTS bytes. */
    if (pusi) {
        if (plen >= 9 && payload[0] == 0x00 && payload[1] == 0x00 && payload[2] == 0x01) {
            int pes_hdr_len = payload[8];
            int skip = 9 + pes_hdr_len;
            if (skip <= plen) {
                payload += skip;
                plen -= skip;
            } else {
                return 0;
            }
        }
    }
    if (plen <= 0) {
        return 0;
    }
    if (plen > out_cap) {
        plen = out_cap;   /* defensive; caller guarantees out_cap >= input */
    }
    memcpy(out, payload, plen);
    return plen;
}

int ts_demux_process(ts_demux_t *d, const uint8_t *in, int in_len,
                     uint8_t *out, int out_cap)
{
    int in_pos = 0;
    int out_len = 0;

    while (in_pos < in_len) {
        if (d->pkt_fill == 0 && in[in_pos] != TS_SYNC) {
            in_pos++;            /* resync to packet boundary */
            continue;
        }
        int need  = TS_PKT_SIZE - d->pkt_fill;
        int avail = in_len - in_pos;
        int n = need < avail ? need : avail;
        memcpy(d->pkt + d->pkt_fill, in + in_pos, n);
        d->pkt_fill += n;
        in_pos += n;

        if (d->pkt_fill == TS_PKT_SIZE) {
            out_len += _process_packet(d, d->pkt, out + out_len, out_cap - out_len);
            d->pkt_fill = 0;
        }
    }
    return out_len;
}
