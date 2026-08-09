#pragma once

// Turns a music folder's plain JPEG artwork into the cover.bin the SD player
// reads, so an album that already ships a cover.jpg needs nothing uploaded.
//
// The conversion (libjpeg decode + crop to a 240x240 RGB565 square) is far too
// slow for the LVGL task, so it runs on a short-lived worker task and the
// result is cached on the card next to the music: an album is converted once,
// every later play of it is a plain file read.

// Build <dir>/cover.bin from the first accepted JPEG in `dir` ("cover.jpg",
// "cover.jpeg", "folder.jpg", "front.jpg"). No-op when a conversion is already
// running, or when this folder was already tried and had nothing usable — the
// caller may therefore ask on every folder change without thinking about it.
void cover_art_request(const char *dir);

// Fired on the worker task once a cover.bin has been written, so the UI can
// reload the artwork of the folder it is showing.
void cover_art_set_done_cb(void (*cb)(void));
