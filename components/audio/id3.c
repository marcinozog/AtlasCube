#include "id3.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

static const char *TAG = "ID3";

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
    if (ok) ESP_LOGD(TAG, "%s → \"%s\"", path, out);
    return ok;
}
