# AtlasCube

*English | [Polski](README.pl.md)*

[![Build firmware](https://github.com/marcinozog/AtlasCube/actions/workflows/build.yml/badge.svg)](https://github.com/marcinozog/AtlasCube/actions/workflows/build.yml)

A hobby project — internet radio and smart clock running on a generic dev board (for now) with ESP32-S3 (AtlasCube). Streams internet radio, shows a clock, manages reminders, and exposes a web UI for configuration. Everything runs on the device with no cloud dependency.

🌐 **[atlascube.net](https://atlascube.net)** — project home

<table>
  <tr>
    <td>➡️ <b><a href="https://atlascube.net/demo">Web UI demo</a></b></td>
    <td>Live, in-browser preview of the device web UI — click around without the hardware</td>
  </tr>
  <tr>
    <td>⚡ <b><a href="https://atlascube.net/flash">Flash from browser</a></b></td>
    <td>Install prebuilt firmware over USB from a Chromium browser — no ESP-IDF, no esptool, no CLI</td>
  </tr>
  <tr>
    <td>🔧 <b><a href="#build">Build from source</a></b></td>
    <td>Different display or your own pin layout? Pick the variant and set every GPIO in <a href="main/include/defines.h"><code>main/include/defines.h</code></a>, then build with one command</td>
  </tr>
  <tr>
    <td>📱 <b><a href="https://github.com/marcinozog/AtlasCube-Remote/">Android remote app</a></b></td>
    <td>Phone remote — playback, EQ, events, layout editor (beta)</td>
  </tr>
</table>

---

## Screenshots

### Device screens

<table>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/scr_clock_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_clock_d.jpg" width="200"></a><br><sub>Clock — dark</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_clock_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_clock_l.jpg" width="200"></a><br><sub>Clock — light</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_radio_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_radio_d.jpg" width="200"></a><br><sub>Radio — dark</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_radio_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_radio_l.jpg" width="200"></a><br><sub>Radio — light</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/scr_clock_d_2.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_clock_d_2.jpg" width="200"></a><br><sub>Clock — dark</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_clock_l_2.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_clock_l_2.jpg" width="200"></a><br><sub>Clock — light</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_radio_d_2.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_radio_d_2.jpg" width="200"></a><br><sub>Radio — dark</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_radio_l_2.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_radio_l_2.jpg" width="200"></a><br><sub>Radio — light</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/scr_playlist_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_playlist_d.jpg" width="200"></a><br><sub>Playlist — dark</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_playlist_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_playlist_l.jpg" width="200"></a><br><sub>Playlist — light</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_settings_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_settings_d.jpg" width="200"></a><br><sub>Settings — dark</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_settings_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_settings_l.jpg" width="200"></a><br><sub>Settings — light</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/scr_event_noti_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_event_noti_d.jpg" width="200"></a><br><sub>Event — dark</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_event_noti_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_event_noti_l.jpg" width="200"></a><br><sub>Event — light</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_eq_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_eq_l.jpg" width="200"></a><br><sub>Equalizer</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_bt_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_bt_d.jpg" width="200"></a><br><sub>Bluetooth</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/diagram_ili9341.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/diagram_ili9341.jpg" width="200"></a><br><sub>Diagram with ILI9341</sub></td>
    <td align="center"><a href="https://atlascube.net/images/diagram_co5300.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/diagram_co5300.jpg" width="200"></a><br><sub>Diagram with CO5300</sub></td>
    <td align="center"><a href="https://atlascube.net/images/diagram_ssd1322.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/diagram_ssd1322.jpg" width="200"></a><br><sub>Diagram with SSD1322</sub></td>
  </tr>
</table>

### Web UI

<table>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_index.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_index.png" width="400"></a><br><sub>Radio</sub></td>
    <td align="center"><a href="https://atlascube.net/images/www_bt.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_bt.png" width="400"></a><br><sub>Bluetooth</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_layouts.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_layouts.png" width="400"></a><br><sub>Layouts editor</sub></td>
    <td align="center"><a href="https://atlascube.net/images/www_files_manager.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_files_manager.png" width="400"></a><br><sub>Files manager</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_settings_disp.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_settings_disp.png" width="400"></a><br><sub>Display</sub></td>
    <td align="center"><a href="https://atlascube.net/images/www_settings_mqtt.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_settings_mqtt.png" width="400"></a><br><sub>MQTT</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_settings_ss.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_settings_ss.png" width="400"></a><br><sub>Screensavers</sub></td>
    <td align="center"><a href="https://atlascube.net/images/www_settings_theme.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_settings_theme.png" width="400"></a><br><sub>Theme</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_settings_sleep.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_settings_sleep.png" width="400"></a><br><sub>Dim & Wake</sub></td>
    <td align="center"><a href="https://atlascube.net/images/www_settings_tools.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_settings_tools.png" width="400"></a><br><sub>Tools</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_editor.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_editor.png" width="400"></a><br><sub>Editor</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_playlist.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_playlist.png" width="400"></a><br><sub>Playlist</sub></td>
    <td align="center"><a href="https://atlascube.net/images/www_events.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_events.png" width="400"></a><br><sub>Events</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_eq.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_eq.png" width="400"></a><br><sub>Equalizer</sub></td>
    <td align="center"></td>
  </tr>
</table>

---

## Features

**Audio**
- Internet radio streaming — MP3, AAC, FLAC (via [esp-adf](https://github.com/espressif/esp-adf))
- HLS live streams — plays segmented `.m3u8` playlists alongside plain MP3/AAC/FLAC streams; the MPEG-TS segments are demuxed to ADTS on the fly
- Podcasts — plays a podcast episode as a **finite HTTP stream** (a distinct mode from endless radio: end-of-file is a clean stop, not a reconnect), with the episode title shown on screen, and **resumes mid-episode** via an HTTP `Range` request. The [Android app](https://github.com/marcinozog/AtlasCube-Remote/) is the catalog — it browses/searches feeds (iTunes search, Apple charts, or add-by-URL — all keyless), sends the episode's direct URL, and remembers the playback position; audio streams straight from the CDN to the device, exactly like a radio station
- ICY metadata — station name and now-playing track shown on screen and in the web UI
- 10-band parametric EQ + soft volume (custom DSP element, core 1)
- Playlist — up to 50 stations, stored in SPIFFS
- Bluetooth audio — A2DP sink and HFP hands-free (external QCC5125 module, Bluetooth 5.1); supported codecs: LDAC, aptX HD, aptX LL, aptX, SBC, AAC
- SD card music player — play MP3 / WAV / FLAC / AAC files straight from a microSD folder. Browse subfolders, queue with shuffle and repeat (none / all / one), pause/resume, and auto-advance — all from the `/sd-player.html` web page. A third audio source alongside radio and Bluetooth (one active at a time); shares the EQ and volume with the radio output
- Hardware I2S source switching — a 74HC157D multiplexer routes either the ESP32-S3 or the QCC5125 I2S output to the DAC, selected by a single GPIO
- Automatic retry on stream loss
- Resume on boot — optionally replays the last station after a restart if the radio was playing when it powered off (opt-in, toggled in the Settings web UI)

**UI**
- LVGL-based GUI — supports ILI9341 320×240 (SPI), ST7796U 480×320 (SPI), ILI9488 480×320 (SPI, 18-bit), CO5300 240×296 round AMOLED (QSPI), and SSD1322 256×64 mono OLED (SPI), switched via a single compile-time define
- Screens: home hub (clock face + adaptive controls), radio, SD player, playlist, equalizer, settings, Bluetooth, events, WiFi AP
- Home hub — the default screen: a clock face that adapts to the active source (radio / SD / BT), with a tap overlay to control playback and jump to the playlist / SD browser / BT / settings. It covers all sources from one screen; the per-source screens are optional (hide them from Settings → Display)
- Rotary encoder navigation (turn + press)
- Touch — capacitive CST816D (CO5300 round AMOLED) or FT6336U (ST7796U 480×320) on I2C, or resistive XPT2046 (SPI; shares the LCD bus or a dedicated SPI3, calibrated per UI profile); coexists with the rotary encoder, either input works at any time
- Swipe gestures — horizontal swipes cycle the home ring (home ↔ bt ↔ radio ↔ sd ↔ mqtt, skipping hidden screens); swipe-up opens settings (home) or the source list (radio→playlist, SD→browser); detection runs through LVGL on the existing pointer indev, no per-chip glue
- On-screen controls overlay — tap a screen to bring up the playback controls (play/stop, vol±, prev/next), auto-hides after a short timeout; the home hub's overlay adds source/playlist/sd/settings buttons
- Audio VU meter — an optional radio-screen widget showing a real-time FFT spectrum computed from the live audio output; position it via the layout editor
- Configurable layout (widget positions editable via JSON)
- Screen background — choose a gradient, a solid color, or an **SD wallpaper image** (panel-sized RGB565 `.bin`, shared across all screens), from the Settings web UI
- Custom boot splash logo — drop a panel-sized RGB565 `.bin` on the SD card to replace the built-in logo (auto-fit; falls back to the built-in logo if missing)
- Optional per-source screens in the home ring — show or hide the Radio, SD player and Bluetooth screens from the Settings web UI (the home hub stays, so the ring is never empty)
- 180° flip & colour inversion — rotate the whole screen upside-down (for inverted mounting) or invert panel colours (fixes batches where e.g. yellow shows as blue); both toggle from the Settings web UI and apply live, no restart
- Screensavers — kick in after a configurable idle timeout; choose from clock hands, starfield, fireworks, plasma, Conway's Game of Life, blank (AMOLED-friendly "off"), **Dim** (just lowers the backlight, keeps the current screen), **Dashboard**, or **Photo frame** (see below)

**Dashboard screensaver**
- A user-configurable ambient display that polls any JSON HTTP/HTTPS endpoint and renders a single value
- Configurable from the Settings web UI: **title**, **URL**, **JSON path** (dot/bracket notation, e.g. `rates[0].mid` or `main.temp`), **suffix** (e.g. ` PLN`, `°C`), and **poll interval** (≥ 5 s)
- HTTPS supported out of the box via the ESP-IDF certificate bundle — works with public APIs that need no auth
- Defaults ship with the NBP USD/PLN exchange rate; swap the fields to read pretty much anything serving JSON (weather, crypto, home automation, GitHub stats, …)
- Polling runs in a dedicated FreeRTOS task only while the screensaver is active — no background traffic when another screen is shown

**Photo-frame screensaver**
- Turns the device into a digital photo frame — cycles through images stored on a microSD card
- Images are pre-converted to LVGL's panel-sized RGB565 binary format (by the Android app or the [`scripts/img2lvgl.py`](scripts/img2lvgl.py) helper) and dropped on the card — there is **no JPEG/PNG decoder on the device**, so even large photos cost no extra firmware RAM
- Configurable from the Settings web UI or the Android app: **source folder**, **order** (sequential / random), **seconds per slide**, **reveal effect** and **reveal speed**
- The slow SD load is turned into the transition: each new image **develops over the previous one** with a configurable reveal — **top-down**, **wipe**, **dissolve**, **interlaced** (retro low-res → sharp), or **random per slide**
- Renders into two full-screen PSRAM buffers and repaints only the area that changed each tick, so it stays light even while the radio streams
- Settings apply live — changing the effect/order/timing updates the running slideshow within a slide, no need to leave the screensaver
- Manage slides from anywhere: browse / upload / rename / delete them via the web **SD file manager** (Settings → Tools) or the Android app, which also converts and uploads phone photos in one step
- Requires a microSD card wired to the build's SDMMC pins (1-bit mode)

**Events & reminders**
- Birthdays, namedays, anniversaries, plain reminders, alarms (radio), voice notifications
- Recurring: daily / weekly / monthly / yearly
- On-screen fullscreen notification + buzzer melody at trigger time; melodies are programmable in firmware (web-based editor planned)
- Alarm type — at trigger time starts a configured playlist station (the stream itself is the "ringtone", keeps playing after dismiss until the radio is stopped) instead of the buzzer
- Voice notification type — plays a spoken clip from the microSD card at trigger time, briefly interrupting and then restoring the radio/Bluetooth source. The Android app synthesizes the speech on the phone (TTS) and uploads it; each clip is a single file named after a readable slug of the event title (e.g. `voice/wake-up-call-a3f9c1.wav`), so the card stays browsable in the file manager. Both the web and app editors can preview the clip
- CRUD via web UI

**Connectivity**
- WiFi STA with AP fallback (first-boot setup via 192.168.4.1)
- HTTP server + WebSocket for real-time state sync
- mDNS — reachable at `<hostname>.local` in STA mode (default `atlascube-xxxx` derived from the MAC, editable in Settings → WiFi); advertises an `_http._tcp` service with a TXT record carrying the `.local` name for discovery clients (e.g. the Android app's NsdManager)
- NTP time sync with configurable timezone
- Web UI served from SPIFFS (no internet required after flash)
- MQTT client — remote control of the radio (play/stop/volume/station) plus up to 6 configurable widgets (toggle / slider / label) on a dedicated on-device screen, driving any external MQTT device (Tasmota, zigbee2mqtt, Home Assistant, …); see [MQTT](#mqtt) below
- OTA firmware update — upload a new app image straight from the web UI (Settings → Tools); it streams into the inactive slot, validates, and reboots, with bootloader rollback if the new image won't start. A backup/export button downloads the currently running firmware first. The web UI and your settings live in separate flash partitions, so an OTA app update never overwrites them. See [OTA updates](#ota-updates) below

**Storage**
- Optional microSD card over SDMMC (1-bit mode), wired to the build's SDMMC pins
- Web **SD file manager** (Settings → Tools) — browse folders, create directories, upload, rename, and delete files straight from the browser (LVGL `.bin` images preview inline); the Android app can push files too
- Web **SPIFFS ⇄ SD backup/restore** (Settings → Tools) — a separate dual-pane manager that copies files between the device's SPIFFS and the SD card: back up configs / web UI to the card and restore them later. Client-side, copy-only
- Web **Settings & stations backup** (Settings → Tools) — one-click *Export settings* downloads every user file on the config partition (settings, theme, events, MQTT, layout, station list) as a single `.json`, and *Import settings* restores it. No SD card needed; layout-independent, so a backup survives a partition change (e.g. before a full USB flash that erases user data). Wi-Fi/MQTT passwords are stored separately and are not included
- Backs the photo-frame slides, voice-notification clips, local music for the SD player, plus the optional screen wallpaper and custom boot splash logo; more on-card content (e.g. station logos) is on the roadmap

**Android app** *(beta)*
- Remote control for playback, station switching, and volume
- Modelled after the YoRadio Remote interface, extended with AtlasCube-specific features: event management, equalizer, layout editor, podcast browser (RSS + iTunes/Apple discovery, resume)
- Separate repo with its own README: [AtlasCube-Remote](https://github.com/marcinozog/AtlasCube-Remote/)

---

## Hardware

| Component | Details |
|---|---|
| MCU | ESP32-S3, 240 MHz, dual-core |
| Board | Atlas Hub (custom) |
| Flash | 16 MB |
| PSRAM | OctoSPI, 80 MHz |
| Display | ILI9341 320×240 (SPI), ST7796U 480×320 (SPI), ILI9488 480×320 (SPI, 18-bit), CO5300 240×296 AMOLED (QSPI), or SSD1322 256×64 mono OLED (SPI) — selected at compile time |
| Touch | CST816D or FT6336U capacitive controller (I2C) — gestures detected by LVGL on the standard pointer indev |
| Input | Rotary encoder with push button + capacitive touch (swipes + tap-to-control overlay) |
| I2S mux | 74HC157D — hardware switch between ESP32-S3 and QCC5125 I2S outputs; controlled via GPIO |
| Audio out | I2S DAC / amplifier (fed from 74HC157D output) |
| Bluetooth | QCC5125 external module, Bluetooth 5.1, A2DP + HFP |
| Microphone | Built-in, used for HFP hands-free |
| Buzzer | LEDC PWM tone generator |

---

## Quick start — flash prebuilt firmware

You don't need ESP-IDF or a toolchain to put AtlasCube on the device. Tagged releases publish ready-to-flash images, and there's a one-click installer in the browser.

### Easiest: flash from your browser

Open **[atlascube.net/flash](https://atlascube.net/flash/)** in Chrome / Edge / Opera / Brave, pick your display variant, plug the device in over USB, click Install. Zero CLI, zero install. (Firefox and Safari don't support WebSerial.)

> First flash: hold the **BOOT** button while plugging USB, then release — puts ESP32-S3 into download mode. Required because the running firmware drives native USB-CDC and ignores the auto-reset.

### Or: flash from CLI with esptool

**1. Pick your display variant:**

| File | Display | Touch |
|---|---|---|
| `AtlasCube-ili9341-ft6336u.bin` | ILI9341 320×240 (SPI) | FT6336U |
| `AtlasCube-st7796-ft6336u.bin`  | ST7796U 480×320 (SPI) | FT6336U |
| `AtlasCube-ili9488-ft6336u.bin` | ILI9488 480×320 (SPI, 18-bit) | FT6336U |
| `AtlasCube-co5300-cst816d.bin`  | CO5300 240×296 (QSPI AMOLED) | CST816D |
| `AtlasCube-ssd1322.bin` | SSD1322 256×64 (mono OLED, SPI) | — (encoder) |
| `AtlasCube-ili9341-xpt2046.bin` | ILI9341 320×240 (SPI) | XPT2046 (resistive) — experimental |
| `AtlasCube-st7796-xpt2046.bin`  | ST7796U 480×320 (SPI) | XPT2046 (resistive) — experimental |
| `AtlasCube-ili9488-xpt2046.bin` | ILI9488 480×320 (SPI, 18-bit) | XPT2046 (resistive) — experimental |

The `-xpt2046` variants are not yet hardware-verified — calibration may need tuning.

**2. Download** the matching `.bin` from the [latest Release](https://github.com/marcinozog/AtlasCube/releases/latest).

**3. Flash** with [esptool](https://github.com/espressif/esptool) (one-time `pip install esptool`):

```bash
esptool.py --chip esp32s3 -p <PORT> write_flash 0x0 AtlasCube-<variant>.bin
```

Substitute `<PORT>` with your serial port (`/dev/ttyUSB0`, `COM3`, …).

**4. First boot:** the device starts an AP named `AtlasCube-XXXXXX`. Connect, open `192.168.4.1` and configure Wi-Fi.

That's it — no ESP-IDF, no ESP-ADF, no patches. The rest of this README describes the dev build, needed only when you want to modify the firmware.

---

## Build

**Requirements**

- [ESP-IDF v5.5.4](https://github.com/espressif/esp-idf)
- [ESP-ADF v2.8](https://github.com/espressif/esp-adf)

**One-command build & flash (recommended)**

> Full step-by-step guides: [docs/build-windows.md](docs/build-windows.md) (ESP-IDF Installation Manager) and [docs/build-linux.md](docs/build-linux.md) (`install.sh` + `export.sh`).

`scripts/build-flash.py` is the all-in-one user script. Set your hardware variant in [`main/include/defines.h`](main/include/defines.h), install [ESP-IDF v5.5.4](https://github.com/espressif/esp-idf) (the official installer is the only manual step on Windows), open the ESP-IDF environment, then from the repo root run:

```bash
python scripts/build-flash.py -p COM5
```

On the **first run** it sets up ESP-ADF for you (clone + patches — no separate step), then compresses the web UI, builds, and flashes a connected board, asking how much of the device to overwrite:

| Scope (`--scope`) | Flashes | Keeps |
|---|---|---|
| Everything / factory (`all`) | bootloader + partition table + app + `www` + `config` | — (works on a blank chip; resets settings to defaults) |
| Firmware only (`fw`) | app slot (OTA-style update) | web UI + settings |
| Firmware + Web UI (`ui`) | app + `www` partition | settings |
| Build only (`build`) | nothing — compile + `web/*.gz` | everything |
| Erase all (`erase`) | wipes the whole flash (app + web UI + settings + NVS) | — |

On a fresh or erased chip use **Everything / factory** (`all`) — it's the only scope that also writes the bootloader and partition table, so the chip can boot. `Firmware only` / `Firmware + Web UI` only update the app / web UI and need a bootloader already present (a blank chip would fail with `invalid header`). `all` flashes the full web UI and resets settings to defaults; the device then boots into AP mode for Wi-Fi setup at `192.168.4.1`.

The flash layout splits the old storage partition into `www` (the editable web UI) and `config` (user settings JSON), so reflashing code or the UI never wipes your settings — only a factory flash reseeds defaults. Pass `--scope all|fw|ui|build|erase` to skip the prompt, and `--monitor` to open the serial monitor afterwards.

Run `build-flash.py` with no `--scope` to get an interactive menu; one entry, **Update from git**, runs `git pull --ff-only` to fetch the latest repo (this script included), then asks you to re-run it. Back up your `defines.h` first — it is tracked by git, so a pull can clash with your local HW/pin edits.

To tweak the web UI without flashing at all, edit files live in the browser (the on-device file editor, or the built-in setup page upload) — they write straight to the `www` partition over HTTP.

> Switching the HW variant in `defines.h`? `build-flash.py` detects a stale `sdkconfig` (the old display/touch defines linger because changed `sdkconfig.defaults` aren't re-applied) and offers to delete it for a clean build. Pass `--clean` to force it without the prompt.

**What the first-run setup does**

The setup logic lives in `scripts/env_setup.py` and runs on the first build (shared by both `build-flash.py` and `ci/build.py`). It is idempotent — safe to re-run; `build-flash.py --setup` re-runs just the patches. It does the following:

- Clones ESP-ADF v2.8 into `./esp-adf` if `ADF_PATH` is not already set.
- Initializes ESP-ADF submodules `components/esp-adf-libs` and `components/esp-sr` (pre-compiled libraries not pulled by a plain clone).
- Copies the AtlasCube board definition into `esp-adf/components/audio_board/esp32_s3_atlascube/`.
- Patches `Kconfig.projbuild`, `CMakeLists.txt` and `component.mk` in `esp-adf/components/audio_board/` to register the board.
- Applies the FreeRTOS patch (`idf_v5.5_freertos.patch`) on ESP-IDF — required for `xTaskCreateRestrictedPinnedToCore`, without which the MP3 decoder task fails at runtime (`E AUDIO_THREAD: Not found right xTaskCreateRestrictedPinnedToCore`).

`sdkconfig.defaults` already contains `CONFIG_ESP32_S3_ATLASCUBE_BOARD=y`.

<details>
<summary>Manual steps — what the setup automates, for reference / debugging</summary>

If you'd rather do it by hand (or are debugging the setup):

1. **ESP-ADF submodules:**
   ```bash
   git -C $ADF_PATH submodule update --init components/esp-adf-libs components/esp-sr
   ```
2. **Board sources** — copy or symlink:
   ```bat
   mklink /D %ADF_PATH%\components\audio_board\esp32_s3_atlascube <path-to-repo>\components\audio_board\esp32_s3_atlascube
   ```
3. **Register in `esp-adf/components/audio_board/Kconfig.projbuild`** (inside the `AUDIO_BOARD` choice):
   ```kconfig
   config ESP32_S3_ATLASCUBE_BOARD
       bool "ESP32-S3-AtlasCube"
   ```
4. **Register in `esp-adf/components/audio_board/CMakeLists.txt`** (before `register_component()`):
   ```cmake
   if (CONFIG_ESP32_S3_ATLASCUBE_BOARD)
       message(STATUS "Current board name is " CONFIG_ESP32_S3_ATLASCUBE_BOARD)
       list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_s3_atlascube)
       set(COMPONENT_SRCS
           ./esp32_s3_atlascube/board.c
           ./esp32_s3_atlascube/board_pins_config.c
       )
   endif()
   ```
5. **Register in `esp-adf/components/audio_board/component.mk`** (legacy GNU Make build):
   ```makefile
   ifdef CONFIG_ESP32_S3_ATLASCUBE_BOARD
   COMPONENT_ADD_INCLUDEDIRS += ./esp32_s3_atlascube
   COMPONENT_SRCDIRS += ./esp32_s3_atlascube
   endif
   ```
6. **FreeRTOS patch on ESP-IDF:**
   ```bash
   git -C $IDF_PATH apply --ignore-whitespace $ADF_PATH/idf_patches/idf_v5.5_freertos.patch
   ```

</details>

**Pick the hardware variant**

The active variant lives in [`main/include/defines.h`](main/include/defines.h) — three independent `#define` groups: `DISPLAY_*`, `UI_PROFILE_*`, `TOUCH_*`. Uncomment exactly one in each group; `build-flash.py` reads them as-is. (`ci/build.py <variant>` overwrites them for you — handy in CI.)

After switching the variant by hand, run `idf.py fullclean` so `sdkconfig` is regenerated from the new combination (`build-flash.py --clean` does this for you).

**Pin configuration**

Every GPIO has a compile-time default in [`main/include/defines.h`](main/include/defines.h) (there is no `menuconfig`/Kconfig for pins). You can also remap pins **at runtime — no rebuild** — from the built-in setup page; `defines.h` then just provides the defaults. Edit `defines.h` (and rebuild) when you want to bake new defaults into a binary.

| Peripheral | Defines | Notes |
|---|---|---|
| Display | `LCD_PIN_*` (SPI) / `DISPLAY_PIN_*` (QSPI) | inside the per-driver `#if CONFIG_DISPLAY_*` block |
| Touch (I2C) | `CTP_SCL`, `CTP_SDA`, `CTP_INT`, `CTP_RST` | CST816D / FT6336U; `-1` = unused (`TOUCH_NONE` skips it) |
| Touch (SPI) | `TP_CLK`, `TP_MOSI`, `TP_MISO`, `TP_CS`, `TP_IRQ` | XPT2046 only; `TP_CLK`/`TP_MOSI` = `-1` shares the LCD bus |
| SD card | `SD_PIN_CLK`, `SD_PIN_CMD`, `SD_PIN_D0`, `SD_PIN_CD` | SDMMC 1-bit; CMD/D0 need ~10k pull-ups |
| I2S DAC | `I2S_DATA`, `I2S_BCK`, `I2S_LCK` | also read by ESP-ADF (via the board's `get_i2s_pins`) |
| Bluetooth | `BT_MODULE_TX_PIN`, `BT_MODULE_RX_PIN`, `BT_MOULE_PIN` | QCC5125 UART |
| Encoder | `ENC_CLK_PIN`, `ENC_DT_PIN`, `ENC_BTN_PIN` | turn + press |
| Buzzer | `BUZZER_PIN` | `-1` to disable |

The display pins are grouped per driver, so set your variant first (above) — you only edit the block that matches the active `DISPLAY_*`.

**Runtime pin setup (no rebuild)**

Open `http://<device-ip>/setup` (or `192.168.4.1/setup` in AP mode; also linked from Settings → Tools). The page lets you remap display / touch / SD / I2S / encoder / buzzer / Bluetooth GPIOs and stores them in NVS, overriding the `defines.h` defaults — so one binary fits boards with different wiring. It flags reserved (26–37), strapping (0/3/45/46) and duplicate pins, and blocks saving on hard conflicts. The allowed-GPIO list is colour-coded **in use (red) / free (green)** so a spare pin is obvious at a glance, and the whole map can be **exported/imported as a JSON file** (the file records the driver and firmware version, so an import warns if it was made for a different build). **Power-cycle** the device after saving (a soft restart does not reliably remap GPIO pads). "Reset pins to defaults" clears the overrides. The display *driver* itself is still fixed at build time — pins are configurable, the driver is not.

**Build and flash manually**

Once the variant and patches are in place (`build-flash.py --setup` does just the setup), the usual ESP-IDF flow works:

```bash
idf.py build
idf.py flash
```

> **Editing the board files while iterating with plain `idf.py`** (e.g. the
> VS Code ESP-IDF extension's build button): `idf.py` builds the copy inside your
> ESP-ADF clone, so repo edits to `components/audio_board/esp32_s3_atlascube/`
> won't take effect until you re-run `build-flash.py --setup`. To keep
> edits live, replace the ADF copy with a junction (no admin needed):
>
> ```powershell
> $dest = "$env:ADF_PATH\components\audio_board\esp32_s3_atlascube"
> Remove-Item -Recurse -Force $dest
> New-Item -ItemType Junction -Path $dest -Target "<repo>\components\audio_board\esp32_s3_atlascube"
> ```
>
> The setup detects an existing symlink/junction and leaves it in place.

**Single merged image (CI / release)**

`ci/build.py` is the release entry point that CI runs: it selects the variant from a CLI argument (overwriting `defines.h`), runs the same first-run setup, builds, and produces a merged, distributable `build/AtlasCube-<variant>.bin`. Most users don't need it — `build-flash.py` above covers building & flashing your own board.

```bash
python ci/build.py co5300       # or ili9341 / st7796 / ili9488 / ssd1322
python ci/build.py              # interactive variant menu
```

The merged `.bin` combines bootloader, partition table, app, and both SPIFFS images (`www` + `config`) into one file flashable from offset `0x0` with `esptool` or a web flasher. To build it by hand:

```bash
python spiffs_image/tools/compress_web.py
idf.py build
idf.py merge-bin -o AtlasCube.bin
```

Flash with:

```bash
esptool.py write_flash 0x0 AtlasCube.bin
```

### Custom fonts

Fonts live in [`components/ui/fonts/`](components/ui/fonts/) as LVGL C arrays. The sizes are not a standard — they are chosen per panel. The `_NN` in a name is the `--size` (line height in px); the large `_72/_80/_96` files are **digit-only** (`--range 0x30-0x3A` plus one icon), while the `_NN_pl` files carry the full Polish character set.

To add a new font (e.g. a larger `montserrat_120`):

1. **Generate** the `.c` with [lv_font_conv](https://lvgl.io/tools/fontconverter) (the exact `Opts:` used are in each file's header). For a digit-only clock font:
   ```bash
   lv_font_conv --font Montserrat-Medium.ttf --range 0x30-0x3A \
     --font FontAwesome5-Solid+Brands+Regular.woff --range 0xF0F3 \
     --size 120 --bpp 4 --format lvgl --no-compress -o lv_font_montserrat_120.c
   ```
   Drop the file into [`components/ui/fonts/`](components/ui/fonts/).
2. **Compile** it — add the filename to the source list in [`components/ui/CMakeLists.txt`](components/ui/CMakeLists.txt).
3. **Declare** it — add `LV_FONT_DECLARE(lv_font_montserrat_120);` in [`ui_fonts.h`](components/ui/fonts/ui_fonts.h).
4. **Register** it — append `{ "montserrat_120", &lv_font_montserrat_120 },` to the table in [`ui_fonts.c`](components/ui/fonts/ui_fonts.c). The id then shows up automatically in the web UI font dropdowns and is serialized into the UI profile.

Note: a glyph is shorter than the nominal size (≈72 % of `--size` for digits), so to get a digit `X` px tall pick `--size ≈ X / 0.72`. See [`docs/layout_editor.md`](docs/layout_editor.md#font-registry) for how fonts map to screen fields.

---

## Web UI

Available at the device IP or `<hostname>.local` (STA mode), or `192.168.4.1` (AP mode).

| Page | Path |
|---|---|
| Radio / now playing | `/` |
| SD music player | `/sd-player.html` |
| Settings | `/settings.html` |
| Playlist | `/playlist.html` |
| Events | `/events.html` |
| Equalizer | `/eq.html` |
| Layout editor | `/layout.html` |
| SPIFFS file editor | `/spiffs-editor.html` |
| SD card file editor | `/sd-editor.html` |
| File manager (SPIFFS / SD) | `/manager.html` |
| MQTT widgets | `/mqtt.html` |

WebSocket endpoint: `ws://<device-ip>/ws` — pushes state changes (volume, track, radio state) in real time.

The running firmware version (from `git describe`) is shown in the web UI header, on the Wi-Fi setup page, and — together with the device IP — on the **boot splash** for a few seconds in STA mode (toggleable in Settings → Display). A quick way to confirm what was flashed and how to reach the device.

---

## MQTT

The device runs an MQTT client that connects to a local broker (e.g. Mosquitto) on the LAN. Configure it from **Settings → MQTT** in the web UI: host, port, username/password, client ID, base topic. After saving, the client reconnects on the fly — no reboot needed.

- **Compile-time switch**: `CONFIG_MQTT_ENABLE` (menuconfig → *MQTT configuration*). Default `y`; set `n` to drop the component entirely from the firmware.
- **Connection**: plain TCP (LAN-only, no TLS), QoS 0, automatic reconnect (handled by `esp-mqtt`).
- **Will / online status**: the device publishes `online` (retained) to `<base_topic>/status` on connect, and the broker delivers `offline` (LWT, retained) on unexpected disconnect.
- **Payload style**: plain text on hierarchical topics (Tasmota/HA-style) — easy to use from `mosquitto_pub` and to wire into Home Assistant via `command_topic`/`state_topic` in YAML.

### Topic map

All radio topics use the prefix `<base_topic>/` (default: `atlascube/`). The MQTT `client_id` is a separate broker-level identifier and does not appear in topic names.

| Topic suffix | Direction | Payload | Notes |
|---|---|---|---|
| `cmd/play` | subscribe | any | resumes the currently selected station |
| `cmd/stop` | subscribe | any | |
| `cmd/next` / `cmd/prev` | subscribe | any | wraps around the playlist |
| `cmd/volume` | subscribe | `0`–`100` | clamped |
| `cmd/station` | subscribe | playlist index | 0-based |
| `state/playing` | publish (retain) | `playing` \| `stopped` \| `buffering` \| `error` | |
| `state/volume` | publish (retain) | `0`–`100` | |
| `state/station_index` | publish (retain) | playlist index | |
| `state/station` | publish (retain) | station name | from playlist entry |
| `state/title` | publish (retain) | ICY title | "" when stopped |
| `status` | publish (retain) + LWT | `online` \| `offline` | LWT delivers `offline` if the device drops |

### Widgets screen

A dedicated on-device screen (the MQTT entry in the home ring) hosts **up to 6 user-defined widgets** in a grid. Each slot is configured independently from `/mqtt.html` (linked from Settings → MQTT). Set a slot's *Type* to `None` to disable it.

**Widget types**

- **Toggle** — publishes `ON`/`OFF` on the cmd topic when tapped; visual state follows the state topic (so the UI stays in sync if the device is toggled from HA, a physical button, an automation, …).
- **Slider** — configurable `min` / `max` / `step`; publishes the numeric value on the cmd topic and tracks the state topic.
- **Label** — read-only; displays the latest value from the state topic, with an optional `unit` suffix (`°C`, `%`, …).

**Common fields**

- **Title** — short string shown above the widget.
- **Command topic** — published on user interaction (toggle/slider). Example (Tasmota): `cmnd/livingroom/POWER`. Example (zigbee2mqtt accepts plain text on `/set`): `zigbee2mqtt/<name>/set`.
- **State topic** — subscribed on connect/reconnect; drives the widget's displayed value.
- **JSON path** — when non-empty, extracts a single field from JSON payloads (works on both directions):
  - *Incoming*: e.g. `state` pulls `"ON"` out of zigbee2mqtt's `{"state":"ON", ...}`.
  - *Outgoing*: the cmd publish is wrapped as `{"<path>": <value>}` instead of raw text — handy for devices that expect JSON (zigbee2mqtt `{"brightness":128}`).
  - Empty path = plain-text mode in both directions.
- Plain-text payload parser accepts `ON`/`OFF`/`on`/`off`/`true`/`false`/`1`/`0` for booleans, bare numbers for sliders.

### Examples

Watch everything the device publishes:

```bash
mosquitto_sub -h 192.168.1.10 -v -t 'atlascube/#'
```

Control the radio:

```bash
mosquitto_pub -h 192.168.1.10 -t atlascube/cmd/play
mosquitto_pub -h 192.168.1.10 -t atlascube/cmd/volume  -m 30
mosquitto_pub -h 192.168.1.10 -t atlascube/cmd/station -m 2
```

Minimal Home Assistant YAML:

```yaml
mqtt:
  switch:
    - name: AtlasCube Radio
      command_topic: atlascube/cmd/play
      payload_off:   stopped       # use cmd/stop for off; or split into two switches
      state_topic:   atlascube/state/playing
      payload_on:    playing
  number:
    - name: AtlasCube Volume
      command_topic: atlascube/cmd/volume
      state_topic:   atlascube/state/volume
      min: 0
      max: 100
  sensor:
    - name: AtlasCube Title
      state_topic: atlascube/state/title
```

> HA MQTT Discovery (auto-registration) is not implemented yet — entities are declared manually as above.

**File editor**

`/spiffs-editor.html` is an in-browser editor for the web UI files — HTML/CSS/JS and other text assets. It lists files from the `www` partition, lets you edit them with syntax highlighting, and saves back over HTTP without reflashing (HTML/CSS/JS are re-gzipped on the device). Useful for tweaking layouts or the web UI on a deployed device. The editor can also open the separate `config` partition (the settings JSON) for direct edits — handy for debugging — though normally those files are managed through their own screens (Settings, Events, MQTT, …).

---

## OTA updates

Update the firmware over Wi-Fi from **Settings → Tools** — no USB cable, no esptool. The page shows the running version, takes a firmware image, streams it to the device, and reboots into it. Progress is mirrored on the device screen.

**What it touches:**

| Partition | OTA touches it? | Notes |
|---|---|---|
| app (`ota_0` / `ota_1`) | ✅ writes the **inactive** slot, then switches boot | the only thing OTA writes |
| `www` (web UI) | ❌ untouched | update separately (file editor / setup page / full reflash) |
| `config` (settings) | ❌ untouched | your settings survive |
| bootloader + partition table | ❌ untouched | can't be changed over OTA |
| NVS | ❌ untouched | pin map (GPIO config) survives |

**How it flows:** `0xE9` magic check → `esp_ota_begin` erases the inactive slot → stream + `esp_ota_write` → `esp_ota_end` validates the checksum → `esp_ota_set_boot_partition` → reboot (bootloader rolls back if the new image won't start). The two app partitions live in [`partitions16MB.csv`](partitions16MB.csv); if no inactive slot is present the endpoint returns `501`.

**Which file:** upload the **app-only** image — either `AtlasCube-<variant>-ota.bin` from the [latest Release](https://github.com/marcinozog/AtlasCube/releases/latest) (also linked from [atlascube.net/flash](https://atlascube.net/flash)) or your own `build/atlascube.bin` (~2.3 MB). *Not* the merged `AtlasCube-<variant>.bin`, which also contains the bootloader, partition table and the `www`/`config` partitions and is meant for a full `0x0` USB flash. Make sure the image matches your display variant; flashing a different variant's binary will break the UI.

**Adopting the layout:** switching an existing 16 MB device to the OTA partition layout is a one-time full USB reflash (a partition-table change can't go through OTA itself). After that, every further update is web-only.

**Safety:**
- The device stops playback during the write to free RAM and avoid flash/SPI contention.
- **Backup first:** the *Export running firmware* button (`GET /api/ota/backup`) downloads the active slot as `atlascube-<version>.bin` — a re-flashable snapshot you can upload again to roll back manually.

When a firmware update also ships new web UI, OTA leaves the `www` partition as-is — the device flags this on the **setup page** (`/setup`), which shows a *web UI out of date* banner with a one-click link to the matching `AtlasCube-www.zip` from the latest release. Extract it and upload the files there (include `www_version.txt` to clear the warning). Alternatively, edit/upload via the in-browser file editor (`/spiffs-editor.html`) or do a full `0x0` reflash.

---

## Project docs

Architecture and design notes in [`docs/`](docs/):

- [`audio_pipeline.md`](docs/audio_pipeline.md) — streaming pipeline, DSP, TCP tuning, task affinity
- [`events.md`](docs/events.md) — reminder/event system design
- [`layout_editor.md`](docs/layout_editor.md) — UI layout customization
- [`navigation.md`](docs/navigation.md) — screen map: the home ring, inputs (encoder/touch), how to edit it
- [`display_drivers.md`](docs/display_drivers.md) — display driver gotchas (QSPI AMOLED even-boundary, shared SPI mutex, LVGL buffer vs internal DRAM budget)

---

## Roadmap

- **Enclosure** — 3D-printed case currently in design; firmware is developed and tested on the bare development board
- **More SD-backed content** — the microSD card already powers the photo frame, voice-notification clips, and local music playback; extending it to e.g. station logos
- **Web melody editor** — in-browser tool for composing custom buzzer notification tunes

---

## License

MIT
