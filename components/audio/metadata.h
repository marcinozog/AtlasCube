#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Local-file audio metadata for the SD music player: "now playing" title (from
// the ID3v2 tag) and track length.
//
// Minimal ID3v2 tag reader for the SD music player's "now playing" title.
//
// Reads the TIT2 (title) and TPE1 (artist) text frames from an MP3's ID3v2
// tag and formats them into `out` as "Artist - Title" (or just "Title" when no
// artist frame is present). Only the frame headers are read in full; frame
// bodies are seeked over unless needed, so a large embedded cover (APIC) does
// not get loaded.
//
// Supports ID3v2.3 (plain 32-bit frame sizes) and v2.4 (syncsafe sizes), text
// encodings 0 (ISO-8859-1), 1/2 (UTF-16) and 3 (UTF-8). Returns true when at
// least a title was found and written to `out`; false for non-MP3 files, a
// missing/unsupported tag, or any read error — the caller then falls back to
// the file name.
bool id3_read_title(const char *path, char *out, size_t out_size);

// Exact play length of a local track, in milliseconds. MP3 (CBR via bitrate,
// VBR via the Xing/Info/VBRI frame count) and WAV (PCM header) are supported;
// other formats return 0. Reads only the file's head, so it's cheap to call at
// track start.
uint32_t media_duration_ms(const char *path);
