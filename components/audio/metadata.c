#include "metadata.h"
#include "esp_log.h"
#include "trace.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

static const char *TAG = "META";

#define ID3_MAX_TEXT 128   // per-frame text cap (title/artist), incl. NUL


/*
static uint32_t syncsafe(const uint8_t b[4])
Decode a 28-bit syncsafe integer (7 usable bits per byte) — the ID3v2 tag size
and v2.4 frame sizes use this encoding so the bytes never collide with an MP3
frame sync.
*/
static uint32_t syncsafe(const uint8_t b[4])
{
    return ((uint32_t)(b[0] & 0x7f) << 21) | ((uint32_t)(b[1] & 0x7f) << 14) |
           ((uint32_t)(b[2] & 0x7f) << 7)  |  (uint32_t)(b[3] & 0x7f);
}


/*
static void append_utf8_cp(char **p, char *end, uint32_t cp)
Append one Unicode code point to *p as UTF-8, advancing *p but never past end
(leaving room for the trailing NUL is the caller's job).
*/
static void append_utf8_cp(char **p, char *end, uint32_t cp)
{
    char *o = *p;
    if (cp < 0x80) {
        if (o + 1 > end) return;
        *o++ = (char)cp;
    } else if (cp < 0x800) {
        if (o + 2 > end) return;
        *o++ = (char)(0xC0 | (cp >> 6));
        *o++ = (char)(0x80 | (cp & 0x3F));
    } else {
        if (o + 3 > end) return;
        *o++ = (char)(0xE0 | (cp >> 12));
        *o++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *o++ = (char)(0x80 | (cp & 0x3F));
    }
    *p = o;
}


/*
static void decode_text(uint8_t enc, const uint8_t *in, size_t n, char *out, size_t out_size)
Convert an ID3 text-frame body to a NUL-terminated UTF-8 string in `out`:
  enc 0 = ISO-8859-1 (each byte → its Latin-1 code point)
  enc 1 = UTF-16 with BOM   enc 2 = UTF-16BE   enc 3 = UTF-8 (copied through)
Only the Basic Multilingual Plane is handled for UTF-16 (no surrogate pairs);
that covers the practical case for music tags.
*/
static void decode_text(uint8_t enc, const uint8_t *in, size_t n,
                        char *out, size_t out_size)
{
    if (out_size == 0) return;
    char *p = out, *end = out + out_size - 1;   // reserve the NUL

    if (enc == 1 || enc == 2) {
        bool be = (enc == 2);
        size_t i = 0;
        if (enc == 1 && n >= 2) {               // consume the byte-order mark
            if (in[0] == 0xFF && in[1] == 0xFE)      { be = false; i = 2; }
            else if (in[0] == 0xFE && in[1] == 0xFF) { be = true;  i = 2; }
        }
        for (; i + 1 < n; i += 2) {
            uint32_t cp = be ? ((uint32_t)in[i] << 8 | in[i + 1])
                             : ((uint32_t)in[i + 1] << 8 | in[i]);
            if (cp == 0) break;                  // NUL terminator inside frame
            append_utf8_cp(&p, end, cp);
        }
    } else if (enc == 3) {                        // UTF-8: copy until NUL/end
        for (size_t i = 0; i < n && in[i]; i++) {
            if (p >= end) break;
            *p++ = (char)in[i];
        }
    } else {                                      // ISO-8859-1
        for (size_t i = 0; i < n && in[i]; i++)
            append_utf8_cp(&p, end, in[i]);
    }
    *p = 0;
}


bool id3_read_title(const char *path, char *out, size_t out_size)
{
    if (!path || !out || out_size == 0) return false;
    if (!strcasestr(path, ".mp3")) return false;   // ID3v2 only probed for MP3

    FILE *f = fopen(path, "rb");
    if (!f) return false;

    bool ok = false;
    char title[ID3_MAX_TEXT]  = {0};
    char artist[ID3_MAX_TEXT] = {0};

    uint8_t hdr[10];
    if (fread(hdr, 1, 10, f) != 10) goto done;
    if (memcmp(hdr, "ID3", 3) != 0) goto done;     // no ID3v2 tag at the start

    uint8_t ver = hdr[3];                          // 3 = v2.3, 4 = v2.4
    if (ver != 3 && ver != 4) goto done;           // v2.2 frame layout differs
    if (hdr[5] & 0x80) goto done;                  // unsynchronisation: skip parsing
    if (hdr[5] & 0x40) goto done;                  // extended header: skip parsing

    uint32_t tag_size = syncsafe(&hdr[6]);
    long tag_end = 10 + (long)tag_size;            // first byte past the tag

    // Walk the frames: 4-byte ID, 4-byte size, 2-byte flags, then the body.
    while (ftell(f) + 10 <= tag_end) {
        uint8_t fh[10];
        if (fread(fh, 1, 10, f) != 10) break;
        if (fh[0] == 0) break;                     // padding → end of frames

        uint32_t fsize = (ver == 4)
            ? syncsafe(&fh[4])
            : ((uint32_t)fh[4] << 24 | (uint32_t)fh[5] << 16 |
               (uint32_t)fh[6] << 8  | fh[7]);
        if (fsize == 0 || ftell(f) + (long)fsize > tag_end) break;

        bool is_title  = !memcmp(fh, "TIT2", 4);
        bool is_artist = !memcmp(fh, "TPE1", 4);
        // fh[9] holds the per-frame format flags (compression / encryption /
        // grouping / data-length indicator); if any are set the body isn't plain
        // text, so skip the frame and fall back to the file name.
        if ((is_title || is_artist) && fsize >= 1 && fh[9] == 0) {
            uint8_t enc = (uint8_t)fgetc(f);
            uint32_t blen = fsize - 1;
            uint8_t buf[ID3_MAX_TEXT];
            uint32_t rd = blen < sizeof(buf) ? blen : sizeof(buf);
            if (fread(buf, 1, rd, f) != rd) break;
            if (rd < blen) fseek(f, blen - rd, SEEK_CUR);   // skip the overflow
            decode_text(enc, buf, rd, is_title ? title : artist,
                        is_title ? sizeof(title) : sizeof(artist));
            if (title[0] && artist[0]) break;      // have both → stop scanning
        } else {
            fseek(f, fsize, SEEK_CUR);             // not interesting → skip body
        }
    }

    if (title[0]) {
        if (artist[0]) snprintf(out, out_size, "%s - %s", artist, title);
        else           snprintf(out, out_size, "%s", title);
        ok = true;
    }

done:
    fclose(f);
    if (ok) TRACE(TRACE_AUDIO, TAG, "%s → \"%s\"", path, out);
    return ok;
}


// ── Embedded cover art ───────────────────────────────────────────────────────

/*
static bool apic_jpeg_span(FILE *f, uint32_t fsize, uint8_t *type,
                           uint32_t *offset, uint32_t *len)
Walk the variable-length head of an APIC frame body — text encoding byte, MIME
string, picture type byte, description string — and report where the image bytes
that follow start and how many there are. The description's terminator depends on the encoding
byte: one NUL for Latin-1/UTF-8, two for the UTF-16 encodings. Only image/jpeg
is accepted (the old v2.2-style 3-character "JPG" spelling included, since some
taggers still write it into v2.3 frames). The file position on entry is the
first byte of the frame body; on return it is undefined — the caller seeks.
*/
static bool apic_jpeg_span(FILE *f, uint32_t fsize, uint8_t *type,
                           uint32_t *offset, uint32_t *len)
{
    const long body = ftell(f);
    if (fsize < 4) return false;

    int enc = fgetc(f);
    if (enc < 0) return false;

    // MIME: NUL-terminated ASCII, read with a cap so a corrupt frame can't walk
    // the whole file.
    char mime[32];
    uint32_t n = 0;
    for (;;) {
        int c = fgetc(f);
        if (c < 0) return false;
        if ((uint32_t)(ftell(f) - body) > fsize) return false;
        if (c == 0) break;
        if (n < sizeof(mime) - 1) mime[n++] = (char)c;
    }
    mime[n] = 0;
    if (strcasecmp(mime, "image/jpeg") != 0 &&
        strcasecmp(mime, "image/jpg")  != 0 &&
        strcasecmp(mime, "JPG")        != 0) return false;

    int pt = fgetc(f);
    if (pt < 0) return false;
    *type = (uint8_t)pt;

    // Description, terminated by one NUL (enc 0/3) or two (the UTF-16 encs).
    const bool wide = (enc == 1 || enc == 2);
    for (;;) {
        int c = fgetc(f);
        if (c < 0) return false;
        if ((uint32_t)(ftell(f) - body) > fsize) return false;
        if (c != 0) continue;
        if (!wide) break;
        int c2 = fgetc(f);
        if (c2 < 0) return false;
        if (c2 == 0) break;
    }

    const long start = ftell(f);
    const uint32_t used = (uint32_t)(start - body);
    if (used >= fsize) return false;

    *offset = (uint32_t)start;
    *len    = fsize - used;
    return true;
}


bool id3_find_cover(const char *path, uint32_t *offset, uint32_t *len)
{
    if (!path || !offset || !len) return false;
    if (!strcasestr(path, ".mp3")) return false;

    FILE *f = fopen(path, "rb");
    if (!f) return false;

    bool ok = false;

    uint8_t hdr[10];
    if (fread(hdr, 1, 10, f) != 10) goto done;
    if (memcmp(hdr, "ID3", 3) != 0) goto done;

    uint8_t ver = hdr[3];
    if (ver != 3 && ver != 4) goto done;
    if (hdr[5] & 0x80) goto done;   // unsynchronisation would corrupt the image bytes
    if (hdr[5] & 0x40) goto done;   // extended header

    long tag_end = 10 + (long)syncsafe(&hdr[6]);

    while (ftell(f) + 10 <= tag_end) {
        uint8_t fh[10];
        if (fread(fh, 1, 10, f) != 10) break;
        if (fh[0] == 0) break;                       // padding → end of frames

        uint32_t fsize = (ver == 4)
            ? syncsafe(&fh[4])
            : ((uint32_t)fh[4] << 24 | (uint32_t)fh[5] << 16 |
               (uint32_t)fh[6] << 8  | fh[7]);
        if (fsize == 0 || ftell(f) + (long)fsize > tag_end) break;

        const long next = ftell(f) + (long)fsize;
        // fh[9] set means compressed/encrypted/length-prefixed body — not a
        // plain JPEG, so leave it alone.
        if (!memcmp(fh, "APIC", 4) && fh[9] == 0) {
            uint8_t type = 0;
            uint32_t off = 0, n = 0;
            if (apic_jpeg_span(f, fsize, &type, &off, &n)) {
                // Keep the first usable picture, but a front cover outranks it
                // and ends the search.
                if (!ok || type == 0x03) {
                    *offset = off;
                    *len    = n;
                    ok      = true;
                }
                if (type == 0x03) break;
            }
        }
        fseek(f, next, SEEK_SET);
    }

done:
    fclose(f);
    if (ok) TRACE(TRACE_AUDIO, TAG, "%s → cover %u B at %u", path,
                  (unsigned)*len, (unsigned)*offset);
    return ok;
}


// ── Track length ─────────────────────────────────────────────────────────────

/*
static uint32_t wav_duration_ms(FILE *f, long fsize)
Length of a PCM WAV from its 44-byte canonical header: data bytes / byte rate.
The engine has its own copy for the end-of-file timer; this one serves the SD
player's on-screen counter. Returns 0 on a short/unparseable header.
*/
static uint32_t wav_duration_ms(FILE *f, long fsize)
{
    uint8_t h[44];
    fseek(f, 0, SEEK_SET);
    if (fread(h, 1, sizeof(h), f) < sizeof(h) || fsize <= (long)sizeof(h))
        return 0;

    uint32_t sr   = h[24] | (h[25] << 8) | (h[26] << 16) | ((uint32_t)h[27] << 24);
    uint16_t ch   = h[22] | (h[23] << 8);
    uint16_t bits = h[34] | (h[35] << 8);
    uint32_t byte_rate = sr * ch * (bits / 8);
    if (byte_rate == 0) return 0;

    uint64_t data_bytes = (uint64_t)(fsize - (long)sizeof(h));
    return (uint32_t)(data_bytes * 1000ULL / byte_rate);
}


// MPEG audio bitrate tables (kbps), indexed by the 4-bit bitrate field. Only the
// Layer III rows are needed for MP3; index 0 (free) and 15 (bad) map to 0.
static const int s_br_mpeg1_l3[16] =
    {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
static const int s_br_mpeg2_l3[16] =
    {0, 8,16,24,32,40,48,56,64, 80, 96,112,128,144,160,0};

// Sample rates (Hz), indexed by [version][srate field]. Version field: 0=MPEG2.5,
// 1=reserved, 2=MPEG2, 3=MPEG1.
static const int s_srate[4][3] = {
    {11025, 12000,  8000},   // MPEG2.5
    {    0,     0,     0},    // reserved
    {22050, 24000, 16000},   // MPEG2
    {44100, 48000, 32000},   // MPEG1
};

/*
static uint32_t mp3_duration_ms(FILE *f, long fsize)
Exact play length of an MP3. Skips a leading ID3v2 tag, parses the first valid
MPEG frame header, then:
  - if the frame carries a Xing/Info (or VBRI) header → uses its frame count for
    an exact VBR length: frames * samples_per_frame / sample_rate;
  - otherwise assumes CBR → (audio bytes) * 8 / bitrate.
Returns 0 if no valid frame is found (e.g. free-format or a non-MP3 payload).
*/
static uint32_t mp3_duration_ms(FILE *f, long fsize)
{
    // Skip a leading ID3v2 tag so the frame scan starts at real audio.
    long audio_start = 0;
    uint8_t tag[10];
    fseek(f, 0, SEEK_SET);
    if (fread(tag, 1, 10, f) == 10 && !memcmp(tag, "ID3", 3)) {
        audio_start = 10 + (long)syncsafe(&tag[6]);
        if (tag[5] & 0x10) audio_start += 10;   // v2.4 footer present
    }

    // Read a window big enough to hold the header + Xing/VBRI region, and tolerate
    // a little junk/padding before the first sync.
    uint8_t buf[1024];
    if (fseek(f, audio_start, SEEK_SET) != 0) return 0;
    size_t n = fread(buf, 1, sizeof(buf), f);
    if (n < 40) return 0;

    for (size_t i = 0; i + 4 <= n; i++) {
        if (buf[i] != 0xFF || (buf[i + 1] & 0xE0) != 0xE0) continue;

        int ver   = (buf[i + 1] >> 3) & 0x03;   // 0=2.5, 1=reserved, 2=2, 3=1
        int layer = (buf[i + 1] >> 1) & 0x03;   // 1=LayerIII, 2=II, 3=I
        int bri   = (buf[i + 2] >> 4) & 0x0F;
        int sri   = (buf[i + 2] >> 2) & 0x03;
        if (ver == 1 || layer != 1 || bri == 0 || bri == 15 || sri == 3)
            continue;                            // not a valid Layer III header

        int bitrate = (ver == 3) ? s_br_mpeg1_l3[bri] : s_br_mpeg2_l3[bri];
        int srate   = s_srate[ver][sri];
        if (bitrate == 0 || srate == 0) continue;

        int spf = (ver == 3) ? 1152 : 576;       // samples/frame, Layer III

        // Xing/Info sits after the side info; VBRI at a fixed offset. Its position
        // depends on MPEG version and mono vs. stereo (channel_mode == 3 → mono).
        int mono = ((buf[i + 3] >> 6) & 0x03) == 3;
        size_t xoff = i + ((ver == 3) ? (mono ? 21 : 36) : (mono ? 13 : 21));

        uint32_t frames = 0;
        if (xoff + 12 <= n &&
            (!memcmp(&buf[xoff], "Xing", 4) || !memcmp(&buf[xoff], "Info", 4))) {
            uint32_t flags = ((uint32_t)buf[xoff+4] << 24) | ((uint32_t)buf[xoff+5] << 16) |
                             ((uint32_t)buf[xoff+6] << 8)  |  buf[xoff+7];
            if (flags & 0x01)                     // frame-count field present
                frames = ((uint32_t)buf[xoff+8] << 24) | ((uint32_t)buf[xoff+9] << 16) |
                         ((uint32_t)buf[xoff+10] << 8) |  buf[xoff+11];
        } else if (i + 36 + 18 <= n && !memcmp(&buf[i + 36], "VBRI", 4)) {
            size_t v = i + 36;                    // VBRI: frame count at offset +14
            frames = ((uint32_t)buf[v+14] << 24) | ((uint32_t)buf[v+15] << 16) |
                     ((uint32_t)buf[v+16] << 8)  |  buf[v+17];
        }

        if (frames > 0)                           // VBR: exact from the frame count
            return (uint32_t)((uint64_t)frames * spf * 1000ULL / (uint32_t)srate);

        // CBR: derive from the audio byte span and the constant bitrate.
        uint64_t audio_bytes = (uint64_t)(fsize - audio_start);
        return (uint32_t)(audio_bytes * 8ULL / (uint32_t)bitrate);   // kbps → ms
    }

    return 0;
}


uint32_t media_duration_ms(const char *path)
{
    if (!path) return 0;

    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);

    uint32_t ms = 0;
    if      (strcasestr(path, ".mp3")) ms = mp3_duration_ms(f, fsize);
    else if (strcasestr(path, ".wav")) ms = wav_duration_ms(f, fsize);
    // FLAC/AAC: no cheap exact length here → 0 (the UI then omits the total).

    fclose(f);
    return ms;
}
