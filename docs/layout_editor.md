# Layout editor — UI editing from the web

The editor lets you change LCD screen layout (positions, sizes,
visibility, fonts) from the web UI without recompiling. Changes are
persisted to a JSON file on SPIFFS and loaded on every boot. After
saving, the active screen is rebuilt on the fly — no device restart
needed.

Currently covered sections: **clock**, **bt**, **radio**. Other screens
(playlist, eq, settings, events, wifi) follow the same pattern described
below in *Adding a new section*. Sections are exposed as tabs at the
top of the layout page.

## Components involved

| Component | Role |
|---|---|
| [components/ui/ui_profile.h](../components/ui/ui_profile.h) + [ui_profile.c](../components/ui/ui_profile.c) | `ui_profile_t` (layout struct), const defaults, mutable runtime, JSON I/O |
| [components/ui/fonts/ui_fonts.h](../components/ui/fonts/ui_fonts.h) + [ui_fonts.c](../components/ui/fonts/ui_fonts.c) | Registry id ↔ `lv_font_t*` (font serialization) |
| [components/ui/ui_manager.c](../components/ui/ui_manager.c) | `do_rebuild_active()` — handles `UI_EVT_PROFILE_CHANGED` |
| [components/ui/screens/screen_home.c](../components/ui/screens/screen_home.c) | Reads `ui_profile_get()` in `create()` |
| [components/web/http_server.c](../components/web/http_server.c) | `/api/ui/profile/*` endpoints |
| [spiffs_image/www/layout.html](../spiffs_image/www/layout.html) + [layout.js](../spiffs_image/www/layout.js) | SVG canvas + form page |
| `/spiffs/ui_profile.json` | Persisted overrides — created on first Apply |

## Philosophy

The layout profile (`ui_profile_t`) is a flat struct of `int16_t`
(offsets, sizes), `bool` (visibility flags), and `const lv_font_t *`
(font pointers). Every screen reads these fields in `create()` and
applies them to LVGL widgets. Editing the layout therefore boils down
to:

1. change a field value in the runtime copy,
2. force `destroy()` + `create()` of the active screen,
3. write the overrides to JSON (read back on next boot).

Screens require no changes — provided they already pull their dimensions
from `ui_profile_get()` instead of hardcoding them.

## Data model

`ui_profile_t` defined in [components/ui/ui_profile.h](../components/ui/ui_profile.h).
One section per screen (clock, radio, playlist, bt, settings, eq,
events, wifi), each with a dozen-ish fields:

| Field type | Example | Range |
|---|---|---|
| `int16_t` (position) | `clock_strip_x`, `clock_strip_y` | 0..screen_w/h (top-left origin) |
| `int16_t` (size) | `clock_strip_w`, `clock_strip_h` | 0..screen_w/h |
| `int16_t` (offset from anchor) | `clock_strip_station_x`, `clock_strip_station_y` | X/Y offset from the strip's top-centre |
| `bool` (visibility) | `clock_show_strip`, `clock_show_time` | true/false |
| `const lv_font_t *` | `clock_strip_station_font`, `clock_time_font` | id from registry → pointer |

**The clock section uses absolute LCD pixels (top-left origin)** for all
"free elements" (strip, time label, date label). No edge
anchoring — each element has `_x`, `_y`, `_w`, `_h`. These hard pixel
coords don't auto-scale, so a layout saved for one LCD size is
meaningless on another. The JSON is stamped with `w`/`h` on save, and
`ui_profile_load_from_file()` skips it when they don't match the compiled
`DISPLAY_WIDTH/HEIGHT` — switching LCD size falls back to the
compile-time defaults automatically (no manual reset needed).

Strip station/title are exceptions — they're anchored to the strip's
top-centre (`LV_ALIGN_TOP_MID`): `_x`/`_y` are offsets from that anchor
and `_w` is the label width, which grows symmetrically around the centre.
Legacy JSON with the shared `clock_strip_label_w` still loads (it seeds
both widths, per-label keys override).

Two instances: `k_defaults` (const, compile-time, picked by `#if
UI_PROFILE_MONO_128X64`) and `s_runtime` (mutable, returned by
`ui_profile_get()`). Boot copies defaults → runtime, then
`ui_profile_load_from_file()` patches runtime from JSON.

## Font registry

Fonts are pointers — they can't be serialized directly. So we keep a
`{id_string, lv_font_t*}` table in [ui_fonts.c](../components/ui/fonts/ui_fonts.c):

```c
static const font_entry_t k_fonts[] = {
    { "montserrat_12_pl", &lv_font_montserrat_12_pl },
    { "montserrat_14_pl", &lv_font_montserrat_14_pl },
    // ...
};
```

JSON stores strings (`"montserrat_14_pl"`), `ui_font_by_id()` maps
string → `lv_font_t*`, `ui_font_id()` does the reverse.

**To add a font:** add `LV_FONT_DECLARE(...)` in `ui_fonts.h`, append a
row in `ui_fonts.c`. That's it — no other code needs to change.

## Persistence — `/spiffs/ui_profile.json`

Format: top-level object with one key per screen section, each section
holding the struct field names.

```json
{
  "clock": {
    "clock_strip_x": 0, "clock_strip_y": 178,
    "clock_strip_w": 320, "clock_strip_h": 62,
    "clock_strip_station_w": 296, "clock_strip_title_w": 296,
    "clock_strip_station_x": 0, "clock_strip_station_y": 8,
    "clock_strip_title_x": 0, "clock_strip_title_y": 32,
    "clock_strip_station_font": "montserrat_14_pl",
    "clock_strip_title_font": "montserrat_12_pl",
    "clock_time_x": 40, "clock_time_y": 25,
    "clock_time_font": "montserrat_96",
    "clock_show_time": true,
    "clock_date_x": 80, "clock_date_y": 130,
    "clock_date_font": "montserrat_18_pl",
    "clock_show_date": true,
    "clock_show_strip": true,
    "clock_show_mode_indicator": true,
    "clock_show_event_indicator": true
  }
}
```

Missing fields → values from `k_defaults`. No file → full profile from
defaults. The file is created on first Apply from the web UI; until then
it simply doesn't exist.

## REST API

| Endpoint | Method | Description |
|---|---|---|
| `/api/ui/profile/meta` | GET | `{ screen_w, screen_h, fonts: [...] }` — used by the page to size the canvas and populate font dropdowns |
| `/api/ui/profile/clock` | GET / POST | Clock section runtime / patch. Missing fields → unchanged. Saves file + triggers active-screen rebuild on POST |
| `/api/ui/profile/bt` | GET / POST | BT section, same semantics as clock |
| `/api/ui/profile/radio` | GET / POST | Radio section, same semantics as clock |
| `/api/ui/profile/reset` | POST | Reset full runtime to compile-time defaults, save file, rebuild |

Implementation in [components/web/http_server.c](../components/web/http_server.c).
Patch / dump for a single section live in `ui_profile.c` as
`ui_profile_dump_clock()` / `ui_profile_patch_clock()` — http_server
just shuttles JSON, doesn't know the field names.

## Change flow

```
[browser]                         [esp32]
  Apply ──POST clock JSON──→  api_ui_profile_clock_post_handler
                                ├── ui_profile_patch_clock(json)   → s_runtime updated
                                ├── ui_profile_save_to_file()      → /spiffs/ui_profile.json
                                └── ui_event_send(UI_EVT_PROFILE_CHANGED)
                                                                   ↓ event queue
                                                              [lvgl_task]
                                                              do_rebuild_active()
                                                                ├── screen->destroy()
                                                                ├── lv_obj_clean(scr)
                                                                └── screen->create(scr)  ← reads new s_runtime
```

Thread-safety: HTTP handler runs on the httpd task, LVGL on `lvgl_task`
— that's why the rebuild is pushed via the ui_manager event queue
instead of being called directly.

## Frontend

[layout.html](../spiffs_image/www/layout.html) — form on the right, SVG
canvas on the left. SVG has `viewBox="0 0 screen_w screen_h"`, every
widget is rendered as a `<rect>` with the right CSS class. Visual scale
×2 (CSS) — `viewBox` keeps proportions faithful to the LCD.

[layout.js](../spiffs_image/www/layout.js) — `state.<section>` object per
section holds current values; `state.active` selects which one the form
edits and the SVG draws. Sections registered in the `SECTIONS` map:
`{ title, fields, renderer }`. Tab buttons switch active section.

All sections are pre-fetched on page load so switching tabs is instant
(no roundtrip). Apply POSTs only the active section's data to
`/api/ui/profile/<section>`. Reset POSTs `/api/ui/profile/reset`
which wipes the entire runtime to defaults — re-fetch everything.

### Drag & drop

Each "free element" (strip, time label, date label) supports:

| Action | Effect |
|---|---|
| Drag rectangle body (`move` cursor) | Changes `_x`, `_y` |
| Drag an orange corner handle | Changes `_x`/`_y` + `_w`/`_h` (per corner) |

4 corner handles (TL, TR, BL, BR) — each one writes a different
combination of fields:
- **TL**: x, y, w, h
- **TR**: y, w, h
- **BL**: x, w, h
- **BR**: w, h

Strip station/title labels are an exception — vertical drag only,
inside the strip, modifies `clock_strip_station_y` / `clock_strip_title_y`.

CSS px → LCD units conversion: `pxPerUnit = svg.clientHeight / screen_h`.
Drag updates the form and vice versa — the form updates the SVG via the
shared `state.clock` object.

### Placeholder text & font preview

Time/date labels are rendered with a placeholder ("88:88",
"Mon 2026-05-01") sized roughly to the font name — `fontHeight()` parses
the `_NN[_pl]` suffix from the font id (e.g. `montserrat_96` → 96 px).
This is just a preview — the actual width on the LCD depends on
LVGL font metrics.

## Adding a new section (e.g. radio)

The `bt` section was added by following exactly this recipe — use it as
a reference (commit history shows the diff).

1. **Schema** in `ui_profile.h` — replace existing offset-from-anchor
   fields with absolute `_x`, `_y`, `_w`, `_h` (or just `_x`, `_y` for
   labels with font-driven size). Declare public helpers
   `ui_profile_dump_radio` / `ui_profile_patch_radio`.

2. **Defaults + JSON helpers** in `ui_profile.c`:
   - Translate existing `LV_ALIGN_X + offset` into absolute coords.
     E.g. for 320x240 with `LV_ALIGN_CENTER, 0, -50` and size 100:
     screen center is (160, 120), so element center is (160, 70),
     top-left is (110, 20).
   - `static void load_radio(const cJSON *obj, ui_profile_t *p)` —
     mirror of fields with `load_i16` / `load_bool` / `load_font`.
   - `static cJSON *dump_radio(const ui_profile_t *p)` — symmetric,
     with `add_*`.
   - `void *ui_profile_dump_radio(void)` + `void ui_profile_patch_radio(const void *)`
     as thin public wrappers.
   - Wire into `ui_profile_load_from_file()` and `ui_profile_save_to_file()`.

3. **Screen rewrite** — replace `lv_obj_align(obj, LV_ALIGN_*, x, y)`
   with `lv_obj_set_pos(obj, p->X_x, p->X_y)`. For sized elements
   (containers, sliders) drop any `_size` field that becomes redundant
   when `_w`/`_h` are explicit.

4. **Endpoint** in `http_server.c` — copy the BT handlers
   (`api_ui_profile_bt_get_handler` / `_post_handler`), s/bt/radio/g,
   register the two URIs.

5. **Frontend** in `layout.js`:
   - Add `RADIO_FIELDS = [...]`.
   - Add a `renderRadio(svg)` function — call `drawFreeElement` for
     each rectangular widget, `drawLabel` for text labels.
   - Register in `SECTIONS`: `radio: { title: 'Radio', fields: RADIO_FIELDS, renderer: renderRadio }`.
   - Add a tab button in `layout.html`: `<button class="section-tab" data-section="radio" onclick="selectSection('radio')">📻 Radio</button>`.

That's it — `selectSection`, `buildForm`, `applyProfile`, `resetProfile`
are all section-agnostic.

## Edge cases

- **No file on first boot** — `ui_profile_load_from_file()` returns
  `ESP_ERR_NOT_FOUND`, runtime stays at defaults. First POST creates
  the file.
- **File saved for a different LCD size** — the file stores `w`/`h`; on
  load, a mismatch with the compiled `DISPLAY_WIDTH/HEIGHT` is logged and
  the overrides are skipped (defaults used), so a stale layout can't
  scatter widgets after a variant switch. A legacy file without `w`/`h`
  counts as a mismatch — re-save once to stamp it.
- **Broken JSON** — `cJSON_Parse` returns `NULL`, fall back to defaults.
  The file is not overwritten on load — the next save will heal it.
- **Unknown font id in JSON** — `ui_font_by_id()` returns `NULL`,
  `load_font` skips that field → defaults stay. No crash.
- **Field missing in JSON** — see above, defaults stay.
- **Drag past screen edge** — JS clamps `clamp(v, 0, screen_h)`. Backend
  doesn't validate (values are `int16_t`, they fit), so wonky sizes can
  produce a weird LCD render — Reset to defaults always rescues you.
- **POST + reboot race** — POST writes the file *before* sending the
  rebuild event. If the device restarts immediately after a POST, the
  LCD state will still be consistent with the file (read back on boot).

## Per-screen wallpapers

Each hub section carries a `<section>_wallpaper` source field
(`clock_wallpaper`, `radio_wallpaper`, `sd_wallpaper`, `bt_wallpaper`),
resolved per screen in `ui_background_apply()`:

- `""` / `"none"` (default) — **General**: the gradient/solid theme
  background, replaced by the internet wallpaper when one is fetched,
- `"net"` — **Internet**: the fetched wallpaper, pinned to this screen,
- anything else — **SD**: an fopen path to a panel-sized RGB565 `.bin` on
  SD, which outranks the internet wallpaper.

An internet-fetched wallpaper (`/api/wallpaper/fetch`) lives only in
PSRAM and replaces the General background on every screen at once — one
fetch shown everywhere — until the next reboot or an explicit background
change (`net_wallpaper_dismiss()`). A screen set to an SD file keeps it;
a screen set to Internet always shows the fetched image. Explicit
per-screen choices are **not** gated by `display.wallpaper_on`; that
switch only controls the global SD wallpaper, which now applies solely to
screens without a hub section (screensavers etc.). Each distinct SD file
is cached in its own PSRAM slot, so navigating between screens never
re-reads the SD card.

Typical use: the BT screen has no audio signal inside the ESP (external
BT module), so a wallpaper designed around VU meters can be replaced or
disabled there.

In the editor, the wallpaper picker above the canvas edits the **active
tab's** screen: *Choose from SD…* assigns an SD file, *Internet* shows
the fetched wallpaper, *General* is the gradient/solid (or internet)
default. The preview shows the effective wallpaper of the active tab;
when the device is currently displaying an internet-fetched wallpaper
(its pixels aren't available to the editor), the canvas shows a "net
wallpaper" placeholder instead (`GET /api/wallpaper/status` reports
`active`).

## Per-wallpaper layout presets

Different wallpapers usually need different widget placement (hotspots,
now-playing, VU meter…). The editor can therefore snapshot the **active
section** into a preset file on the SD card, named after that screen's
effective wallpaper:

```
/wallpapers/layouts/<width>x<height>/<wallpaper-basename>.json
```

e.g. wallpaper `/sdcard/wallpapers/sunset.bin` on a 480×320 panel → preset
`/wallpapers/layouts/480x320/sunset.json`. Format: `{ w, h, wallpaper,
sections: { clock?, bt?, radio?, sd? } }` — sections are optional; Save
merges the active section into the existing file, so one wallpaper can
accumulate layouts for several screens. The resolution directory keeps
independent presets for the same wallpaper on different LCD variants.

This is a **frontend-only** feature — no firmware involvement. Save
uploads the editor's current state via `POST /api/sd/file` (parent
directories are auto-created). Load fetches the file and POSTs the
active section to the existing `/api/ui/profile/<section>` endpoint, so
the layout is applied live and persisted to `ui_profile.json` exactly
like a manual Apply — and never touches the other screens' layouts. When
you pick a wallpaper for a screen in the editor, it checks for a preset
with that screen's section and offers to apply it.

The `w`/`h` stamp is checked against both the active panel and the resolution
directory. A mismatched file is refused on load (same cross-LCD guard as
`ui_profile.json`). Presets from the old flat `/wallpapers/layouts/*.json`
format are intentionally not loaded.

Note: **Save** snapshots what the editor currently shows — including
tweaks not yet applied to the device. Load-then-Save round-trips
losslessly. Save also pins the screen's **effective wallpaper path** into
the stored section (even when the screen was inheriting the global
default), so loading the preset later re-applies both the layout and its
wallpaper regardless of what the global default is by then.

## Limits

- Maximum JSON size: 16 KB in `ui_profile_load_from_file()`, 4 KB in
  POST body — plenty for the full profile (~150 int16/bool/string fields).
- `max_uri_handlers = 24` in httpd (6 layout endpoints + the rest of the API).
