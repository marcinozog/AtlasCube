# Display drivers — design notes and gotchas

Two non-obvious rules learned the hard way while bringing up the CO5300
QSPI AMOLED panel. Both are preventive guidance for any future panel
driver added under [components/display/drivers/](../components/display/drivers/).

## 1. QSPI AMOLED panels need even CASET/RASET boundaries

**Symptom we hit:** the CO5300 rendered the first frame of every screen
correctly, but every *partial* redraw afterwards was shifted/garbled —
e.g. the clock label corrupted on minute tick, the brightness slider
trail corrupted as the knob moved. Switching screens (which triggers a
full-area redraw) always restored a clean image until the next partial
update.

**Why it happens:** the panel internally rounds CASET/RASET to even
pixel boundaries. LVGL partial redraws can land on odd `x1/y1`
coordinates, so the panel's drawing window ends up shifted by one pixel
relative to the framebuffer LVGL is sending — pixels land in the wrong
column/row.

**Fix:** snap invalidated areas to even bounds *before* LVGL renders
into the buffer, via an `LV_EVENT_INVALIDATE_AREA` hook on the
display. Round Y to even, and **force X to the full display width** —
see the next section for why narrow X ranges produce a different
artifact:

```c
static void co5300_rounder_cb(lv_event_t *e)
{
    lv_area_t *a = lv_event_get_param(e);
    a->x1 = 0;                       // full width — see "left-edge artifact"
    a->x2 = DISPLAY_WIDTH - 1;
    a->y1 &= ~1;                     // round y1 down to even
    a->y2 |= 1;                      // round y2 up to odd → height even
}

lv_display_add_event_cb(disp, co5300_rounder_cb,
                        LV_EVENT_INVALIDATE_AREA, NULL);
```

Reference implementation: [components/display/drivers/co5300.c](../components/display/drivers/co5300.c).

> Do **not** try to fix this inside `flush_cb` by rounding only the
> CASET/RASET window — LVGL has already rendered the original
> odd-sized area into the buffer, so the panel window and the pixel
> data still won't match.

### 1a. Why X is forced to full width, not just rounded to even

**Symptom we hit:** with the rounder above doing `x1 &= ~1; x2 |= 1;`
(symmetric with Y), light themes showed a thin black vertical artifact
at the visual left edge of every widget that refreshed at an odd
original `x1` (clock label at `x=49`, vol label at `x=95`, etc.). The
artifact **accumulated** on the panel — each refresh repainted the
flawed pixel without overwriting prior bad ones, so over time the
column thickened into a dotted vertical streak through the UI.

**Why it happens:** when the original `x1` is odd, the rounder expands
the dirty area by one pixel to the left (e.g. `x1=49` → `x1=48`).
LVGL 9.2's SW rasterizer does **not** reliably paint that extra column
during partial render. The DMA buffer starts zeroed (`heap_caps_malloc`
with `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`), so the unpainted pixel
stays `0x0000`. After the byte-swap in `flush_cb`, `bswap16(0x0000)` is
still `0x0000`, which the panel renders as a black pixel. With
`MADCTL=0xC0` (MX/MY mirror) that pixel maps to the visual left edge
of the widget.

**What we tried that did NOT work:**
- Undoing the byte-swap after `spi_device_transmit` so the buffer
  stays in LE between refreshes. Ruled out the "double byte-swap of
  unpainted pixel" theory — artifact came right back. Confirms LVGL
  truly skips painting the column, it isn't a stale-byte-order issue.

**What works:** force `x1=0, x2=DISPLAY_WIDTH-1` in the rounder. With
the dirty area always covering the full row, every pixel in the buffer
is part of the original (un-expanded) area and gets painted. On
240-px-wide panels the bandwidth cost is ~480 bytes per row, which is
negligible compared to the QSPI burst overhead.

## 2. Mutex around shared SPI transaction state

The CO5300 driver uses a single global `spi_transaction_ext_t` and
toggles CS by hand via GPIO. LVGL flushes from `lvgl_task`, while
`display_set_backlight()` (a DCS command over the same SPI bus) can be
called from the web/HTTP task, settings handler, or any other task
issuing brightness changes.

Without serialization, two tasks can race on:

- the global transaction struct (one task overwrites cmd/addr/tx_buffer
  while the other has a transaction in flight),
- the manual CS line (one task's `CS_HIGH()` cuts the other's
  transaction mid-byte).

**Fix:** wrap every transaction in a recursive mutex (recursive so a
multi-step sequence like CASET+RASET+RAMWR+bulk can hold one outer
lock while the inner writers re-take it):

```c
static SemaphoreHandle_t s_spi_mtx;     // xSemaphoreCreateRecursiveMutex()
#define SPI_LOCK()    xSemaphoreTakeRecursive(s_spi_mtx, portMAX_DELAY)
#define SPI_UNLOCK()  xSemaphoreGiveRecursive(s_spi_mtx)
```

Reference implementation: [components/display/drivers/co5300.c](../components/display/drivers/co5300.c).

> The ILI9341 driver does **not** need this — its backlight is on a
> separate LEDC PWM channel, not on SPI, so there is only one task
> (LVGL) ever touching the bus.

## 3. LVGL partial buffer vs internal DRAM budget

**Symptom we hit:** right after switching the active profile from
`UI_PROFILE_320x240` (ILI9341) to `UI_PROFILE_480x320` (ST7796U), radio
playback stopped starting. The ESP-ADF pipeline reported:

```
E AUDIO_THREAD: Error creating task dsp
E AUDIO_THREAD: Error creating task i2s
I AUDIO_PIPELINE: ... Dram largest free: 1984 Bytes
```

**Why it happens:** the SPI LCD drivers allocate the LVGL partial
buffer as DMA-capable internal RAM:

```c
buf = heap_caps_malloc(DISPLAY_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t),
                       MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
```

With `LVGL_BUF_LINES = 40` (carried over from the 320-px ILI9341
driver), a 480-px panel pulls **480 × 40 × 2 B = 38 400 B** out of
internal DRAM in one contiguous block. ESP-ADF's `dsp` and `i2s` audio
elements need their task stacks in internal RAM too — once the LVGL
buffer, Wi-Fi/lwIP, and the rest of the firmware have taken their
share, the largest free internal block is too small for an audio task
stack and `audio_thread_create` fails.

**Fix:** size `LVGL_BUF_LINES` per panel so the buffer stays around
~20 kB. For 480-px panels that means `20` (≈19.2 kB) rather than `40`.
LVGL renders partials fine at that height; no visible difference, ~19 kB
of internal DRAM freed.

```c
// components/display/drivers/st7796.c
#define LVGL_BUF_LINES 20   // 480 * 20 * 2 = 19 200 B in DMA+INTERNAL
```

> If you ever port a wider panel (≥ 640 px), drop `LVGL_BUF_LINES`
> further (10–16). The hard signal is `Dram largest free` in the
> pipeline MEM log — if it's < a few kB, audio task creation will fail
> intermittently even if the buffer alloc itself succeeded.

## 4. SSD1322 — 4-bit grayscale, not RGB

The SSD1322 (256×64 OLED, `UI_PROFILE_MONO_256X64`) is the odd one out:
the other drivers push RGB565 straight to the panel, but the SSD1322 is a
16-level grayscale controller. Three consequences:

- **No backlight pin.** It's a self-emissive OLED, so there is no LEDC PWM
  channel. `display_set_backlight()` writes the contrast-current register
  (`0xC1`) instead — same 0–100 API, mapped to 0x00–0xFF.
- **2 pixels per byte.** GDDRAM packs two 4-bit pixels into each byte (high
  nibble = left pixel). LVGL still renders RGB565 (`LV_COLOR_DEPTH=16`), so
  `flush_cb` converts each pixel to luma (`Rec.601` weighting) and packs the
  whole frame into a `DISPLAY_WIDTH*DISPLAY_HEIGHT/2`-byte buffer.
- **Column address = 4 pixels.** One column address spans 4 horizontal
  pixels, and a 256-wide panel is centered in the 480-px GDDRAM → column
  offset `0x1C` (`SSD1322_COL_OFFSET`). Nudge it by ±1 if the image is
  shifted horizontally; toggle bit 4 of the `0xA0` re-map byte if it's
  mirrored.

Because the column granularity is 4 px, partial flushes would need X snapped
to multiples of 4 (cf. the CASET/RASET rule in §1). Rather than carry a
rounder, the driver renders **full-screen in PSRAM** (`MALLOC_CAP_SPIRAM`,
`LV_DISPLAY_RENDER_MODE_FULL`) and pushes the whole 8 KB packed frame each
flush. The panel is tiny, so the full-frame cost is negligible, and keeping
the render buffer in PSRAM sidesteps the internal-DRAM budget from §3 — only
the 8 KB DMA scratch buffer lives in internal RAM.

Reference implementation: [components/display/drivers/ssd1322.c](../components/display/drivers/ssd1322.c).
