# AtlasCube

[![Build firmware](https://github.com/marcinozog/AtlasCube/actions/workflows/build.yml/badge.svg)](https://github.com/marcinozog/AtlasCube/actions/workflows/build.yml)

A hobby project — internet radio and smart clock built on a custom ESP32-S3 board (AtlasCube). Streams internet radio, shows a clock, manages reminders, and exposes a web UI for configuration. Everything runs on the device with no cloud dependency.

🌐 **[atlascube.net](https://atlascube.net)**

➡️ **[Web interface demo](https://atlascube.net/demo)** — a live, in-browser preview of the device web UI. The demo mirrors what runs on the ESP32-S3: playlist, settings, events, equalizer, layout editor, and file editor are all interactive, backed by a mock state so you can click around without owning the hardware.

⚡ **[Flash firmware from browser](https://atlascube.net/flash)** — install prebuilt firmware directly over USB from Chromium-based browsers (Chrome / Edge / Opera / Brave) via WebSerial. No ESP-IDF, no esptool, no CLI.

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
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/diagram_co5300.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/diagram_co5300.jpg" width="200"></a><br><sub>Diagram with CO5300</sub></td>
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
- ICY metadata — station name and now-playing track shown on screen and in the web UI
- 10-band parametric EQ + soft volume (custom DSP element, core 1)
- Playlist — up to 50 stations, stored in SPIFFS
- Bluetooth audio — A2DP sink and HFP hands-free (external QCC5125 module, Bluetooth 5.1); supported codecs: LDAC, aptX HD, aptX LL, aptX, SBC, AAC
- Hardware I2S source switching — a 74HC157D multiplexer routes either the ESP32-S3 or the QCC5125 I2S output to the DAC, selected by a single GPIO
- Automatic retry on stream loss

**UI**
- LVGL-based GUI — supports ILI9341 320×240 (SPI), ST7796U 480×320 (SPI), and CO5300 240×296 round AMOLED (QSPI), switched via a single compile-time define
- Screens: clock, playlist, equalizer, settings, Bluetooth, events, WiFi AP
- Rotary encoder navigation (turn + press)
- Capacitive touch — CST816D (CO5300 round AMOLED) or FT6336U (ST7796U 480×320), both on I2C; coexists with the rotary encoder, either input works at any time
- Swipe gestures — horizontal swipes navigate between clock ↔ radio ↔ bt; swipe-up opens settings (clock) or playlist (radio); detection runs through LVGL on the existing pointer indev, no per-chip glue
- On-screen controls overlay — tap a media screen to bring up a 5-button cross (play/pause, vol±, prev/next), auto-hides after a short timeout
- Configurable layout (widget positions editable via JSON)
- Screensavers — kick in after a configurable idle timeout; choose from clock hands, starfield, fireworks, plasma, Conway's Game of Life, blank (AMOLED-friendly "off"), or **Dashboard** (see below)

**Dashboard screensaver**
- A user-configurable ambient display that polls any JSON HTTP/HTTPS endpoint and renders a single value
- Configurable from the Settings web UI: **title**, **URL**, **JSON path** (dot/bracket notation, e.g. `rates[0].mid` or `main.temp`), **suffix** (e.g. ` PLN`, `°C`), and **poll interval** (≥ 5 s)
- HTTPS supported out of the box via the ESP-IDF certificate bundle — works with public APIs that need no auth
- Defaults ship with the NBP USD/PLN exchange rate; swap the fields to read pretty much anything serving JSON (weather, crypto, home automation, GitHub stats, …)
- Polling runs in a dedicated FreeRTOS task only while the screensaver is active — no background traffic when another screen is shown

**Events & reminders**
- Birthdays, namedays, anniversaries, plain reminders
- Recurring: daily / weekly / monthly / yearly
- On-screen fullscreen notification + buzzer melody at trigger time; melodies are programmable in firmware (web-based editor planned)
- CRUD via web UI

**Connectivity**
- WiFi STA with AP fallback (first-boot setup via 192.168.4.1)
- HTTP server + WebSocket for real-time state sync
- NTP time sync with configurable timezone
- Web UI served from SPIFFS (no internet required after flash)
- MQTT client — remote control of the radio (play/stop/volume/station) plus up to 6 configurable widgets (toggle / slider / label) on a dedicated on-device screen, driving any external MQTT device (Tasmota, zigbee2mqtt, Home Assistant, …); see [MQTT](#mqtt) below

**Android app** *(in development)*
- Remote control for playback, station switching, and volume
- Modelled after the YoRadio Remote interface, extended with AtlasCube-specific features: event management, equalizer, layout editor

---

## Hardware

| Component | Details |
|---|---|
| MCU | ESP32-S3, 240 MHz, dual-core |
| Board | Atlas Hub (custom) |
| Flash | 8 MB |
| PSRAM | OctoSPI, 80 MHz |
| Display | ILI9341 320×240 (SPI), ST7796U 480×320 (SPI), or CO5300 240×296 AMOLED (QSPI) — selected at compile time |
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
| `AtlasCube-ili9341.bin` | ILI9341 320×240 (SPI) | FT6336U |
| `AtlasCube-st7796.bin`  | ST7796U 480×320 (SPI) | FT6336U |
| `AtlasCube-co5300.bin`  | CO5300 240×296 (QSPI AMOLED) | CST816D |

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

**One-shot setup**

After cloning ESP-IDF and ESP-ADF, run the bundled setup script:

```bash
ADF_PATH=<path-to-esp-adf> IDF_PATH=<path-to-esp-idf> bash scripts/patch-esp-adf.sh
```

The script is idempotent (safe to re-run) and does the following:

- Initializes ESP-ADF submodules `components/esp-adf-libs` and `components/esp-sr` (pre-compiled libraries not pulled by a plain clone).
- Copies the AtlasCube board definition into `esp-adf/components/audio_board/esp32_s3_atlascube/`.
- Patches `Kconfig.projbuild`, `CMakeLists.txt` and `component.mk` in `esp-adf/components/audio_board/` to register the board.
- Applies the FreeRTOS patch (`idf_v5.5_freertos.patch`) on ESP-IDF — required for `xTaskCreateRestrictedPinnedToCore`, without which the MP3 decoder task fails at runtime (`E AUDIO_THREAD: Not found right xTaskCreateRestrictedPinnedToCore`).

`sdkconfig.defaults` already contains `CONFIG_ESP32_S3_ATLASCUBE_BOARD=y`.

<details>
<summary>What the script does — manual steps, for reference</summary>

If you'd rather do it by hand (or are debugging the script):

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

The active variant lives in [`main/include/defines.h`](main/include/defines.h) — three independent `#define` groups: `DISPLAY_*`, `UI_PROFILE_*`, `TOUCH_*`. Edit by hand, or use the helper:

```bash
bash scripts/select-variant.sh ili9341   # or st7796 / co5300
```

After switching the variant, run `idf.py fullclean` so `sdkconfig` is regenerated from the new combination.

**Build and flash**

```bash
idf.py build
idf.py flash
```

**Flash web UI (SPIFFS)**

```bash
idf.py spiffs_create_partition_image storage spiffs_image/web
idf.py flash
```

**Single merged image (for distribution)**

Combines bootloader, partition table, app, and SPIFFS into one file that can be flashed from offset `0x0` with `esptool` or a web flasher:

```bash
idf.py merge-bin -o AtlasCube.bin
```

Flash with:

```bash
esptool.py write_flash 0x0 AtlasCube.bin
```

---

## Web UI

Available at the device IP (STA mode) or `192.168.4.1` (AP mode).

| Page | Path |
|---|---|
| Radio / now playing | `/` |
| Settings | `/settings.html` |
| Playlist | `/playlist.html` |
| Events | `/events.html` |
| Equalizer | `/eq.html` |
| Layout editor | `/layout.html` |
| File editor | `/editor.html` |
| MQTT widgets | `/mqtt.html` |

WebSocket endpoint: `ws://<device-ip>/ws` — pushes state changes (volume, track, radio state) in real time.

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

A dedicated on-device screen (swipe right from the clock) hosts **up to 6 user-defined widgets** in a grid. Each slot is configured independently from `/mqtt.html` (linked from Settings → MQTT). Set a slot's *Type* to `None` to disable it.

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

`/editor.html` is an in-browser editor for files stored in SPIFFS — JSON configs (layouts, playlist, events), HTML/CSS/JS of the web UI itself, and any other text assets on the device. Lists files from the storage partition, lets you edit them with syntax highlighting, and saves back over HTTP without reflashing. Useful for tweaking layouts or web UI on a deployed device.

---

## Project docs

Architecture and design notes in [`docs/`](docs/):

- [`audio_pipeline.md`](docs/audio_pipeline.md) — streaming pipeline, DSP, TCP tuning, task affinity
- [`events.md`](docs/events.md) — reminder/event system design
- [`layout_editor.md`](docs/layout_editor.md) — UI layout customization
- [`display_drivers.md`](docs/display_drivers.md) — display driver gotchas (QSPI AMOLED even-boundary, shared SPI mutex, LVGL buffer vs internal DRAM budget)

---

## Roadmap

- **Enclosure** — 3D-printed case currently in design; firmware is developed and tested on the bare development board
- **SD card** — local storage for station logos, music files, and voice notification clips
- **Additional displays** — SSD1322 (256×64 OLED) support in progress (ILI9341 320×240, ST7796U 480×320, and CO5300 240×296 already supported)
- **Web melody editor** — in-browser tool for composing custom buzzer notification tunes

---

## License

MIT
