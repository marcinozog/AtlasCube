#pragma once

#include <stdint.h>

// Album art for the SD player, from two sources the caller does not have to
// tell apart:
//
//   * a plain JPEG lying in the music folder ("cover.jpg", "cover.jpeg",
//     "folder.jpg", "front.jpg") — converted once into the folder's cover.bin,
//     which every later play just reads;
//   * the APIC frame of the playing MP3 — decoded per track and kept in PSRAM
//     only, so nothing is written next to somebody's music. A folder's
//     cover.bin outranks it: an artwork file is a deliberate choice, an
//     embedded tag is whatever the ripper put there.
//
// The work (libjpeg + a resample) is far too slow for the LVGL task, so it runs
// on a worker task created on first use; requests are coalesced, so skipping
// through tracks costs one conversion, not one per skip.

typedef enum {
    COVER_EMB_NOTHING = 0,   // nothing new — keep showing what is on screen
    COVER_EMB_NEW,           // fresh artwork, ownership passes to the caller
    COVER_EMB_CLEAR,         // this track has none — drop what is shown
} cover_emb_t;

// Look at `dir` (and `track`, the full path of the file playing from it, which
// may be NULL) and produce whatever artwork they hold. Cheap and safe to call
// on every track change from any task: it only queues the request.
void cover_art_request(const char *dir, const char *track);

// Collect the result of the embedded-artwork path after a done callback. On
// COVER_EMB_NEW the caller owns *buf (an RGB565 PSRAM buffer, *w x *h) and
// frees it. Artwork that turned out identical to what is already on screen is
// never reported — the decode is skipped entirely for it.
cover_emb_t cover_art_take_embedded(uint16_t **buf, int *w, int *h);

// Fired on the worker task when something changed: a folder's cover.bin was
// written, or embedded artwork is waiting in cover_art_take_embedded().
void cover_art_set_done_cb(void (*cb)(void));
