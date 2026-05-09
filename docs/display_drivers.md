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
display:

```c
static void co5300_rounder_cb(lv_event_t *e)
{
    lv_area_t *a = lv_event_get_param(e);
    a->x1 &= ~1;       // round x1 down to even
    a->y1 &= ~1;       // round y1 down to even
    a->x2 |= 1;        // round x2 up to odd  → width even
    a->y2 |= 1;        // round y2 up to odd  → height even
}

lv_display_add_event_cb(disp, co5300_rounder_cb,
                        LV_EVENT_INVALIDATE_AREA, NULL);
```

Reference implementation: [components/display/drivers/co5300.c](../components/display/drivers/co5300.c).

> Do **not** try to fix this inside `flush_cb` by rounding only the
> CASET/RASET window — LVGL has already rendered the original
> odd-sized area into the buffer, so the panel window and the pixel
> data still won't match.

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
