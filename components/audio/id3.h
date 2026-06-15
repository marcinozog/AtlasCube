#pragma once

#include <stdbool.h>
#include <stddef.h>

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
