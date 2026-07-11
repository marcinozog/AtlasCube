# libjpeg (IJG jpeg-9f, decode-only)

Vendored subset of the Independent JPEG Group's libjpeg, release **9f**
(<http://www.ijg.org/>), holding just the decompression side of the library —
the encoder (`jc*.c`), the cjpeg/djpeg applications and the alternative memory
backends are not included.

**Why it exists:** the net_wallpaper feature decodes photos fetched from the
internet, and most services (NASA APOD, picsum, anything run through mozjpeg)
serve **progressive** JPEGs. Every Espressif decoder (`esp_new_jpeg`, tjpgd /
`esp_jpeg`, the ESP32-P4 hardware codec) is baseline-only; libjpeg is the
canonical portable decoder that handles progressive.

Layout:

- `jpeg-9f/` — pristine upstream sources plus a hand-written `jconfig.h`
  (standard ANSI answers, see `jconfig.txt` upstream). Do not edit; a future
  upgrade should be a clean re-drop of the new release.
- `jmem_esp.c` — the one ESP-specific file: the `jmemsys.h` backend (replaces
  upstream `jmemnobs.c`), routing all library allocations to PSRAM. A
  progressive decode buffers the whole image's DCT coefficients (~2.5 MB for a
  ~1 MP photo), which must never come out of internal DRAM.

License: the IJG license (see `jpeg-9f/README.ijg`), free for commercial and
non-commercial use. Its acknowledgment requirement:

> This software is based in part on the work of the Independent JPEG Group.
