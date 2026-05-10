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
