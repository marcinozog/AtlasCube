# AtlasCube

A hobby project — internet radio and smart clock built on a custom ESP32-S3 board (AtlasCube). Streams internet radio, shows a clock, manages reminders, and exposes a web UI for configuration. Everything runs on the device with no cloud dependency.

🌐 **[atlascube.net](https://atlascube.net)**

➡️ **[Web interface demo](https://atlascube.net/demo)** — a live, in-browser preview of the device web UI. The demo mirrors what runs on the ESP32-S3: playlist, settings, events, equalizer, layout editor, and file editor are all interactive, backed by a mock state so you can click around without owning the hardware.

---

## Screenshots

### Device screens

<table>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/scr_clock_d.jpg" width="200"><br><sub>Clock — dark</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_clock_l.jpg" width="200"><br><sub>Clock — light</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_radio_d.jpg" width="200"><br><sub>Radio — dark</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_radio_l.jpg" width="200"><br><sub>Radio — light</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/scr_clock_d_2.jpg" width="200"><br><sub>Clock — dark</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_clock_l_2.jpg" width="200"><br><sub>Clock — light</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_radio_d_2.jpg" width="200"><br><sub>Radio — dark</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_radio_l_2.jpg" width="200"><br><sub>Radio — light</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/scr_playlist_d.jpg" width="200"><br><sub>Playlist — dark</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_playlist_l.jpg" width="200"><br><sub>Playlist — light</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_settings_d.jpg" width="200"><br><sub>Settings — dark</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_settings_l.jpg" width="200"><br><sub>Settings — light</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/scr_event_noti_d.jpg" width="200"><br><sub>Event — dark</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_event_noti_l.jpg" width="200"><br><sub>Event — light</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_eq_l.jpg" width="200"><br><sub>Equalizer</sub></td>
    <td align="center"><img src="https://atlascube.net/images/scr_bt_d.jpg" width="200"><br><sub>Bluetooth</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/diagram_ili9341.jpg" width="200"><br><sub>Diagram with ILI9341</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/diagram_co5300.jpg" width="200"><br><sub>Diagram with CO5300</sub></td>
  </tr>
</table>

### Web UI

<table>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/www_index.png" width="400"><br><sub>Radio</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/www_layouts.png" width="400"><br><sub>Layouts editor</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/www_settings_1.png" width="400"><br><sub>Settings</sub></td>
    <td align="center"><img src="https://atlascube.net/images/www_settings_2.png" width="400"><br><sub>Settings (cont.)</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/www_editor.png" width="400"><br><sub>Editor</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/www_playlist.png" width="400"><br><sub>Playlist</sub></td>
    <td align="center"><img src="https://atlascube.net/images/www_events.png" width="400"><br><sub>Events</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://atlascube.net/images/www_eq.png" width="400"><br><sub>Equalizer</sub></td>
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
- LVGL-based GUI — supports ILI9341 320×240 (SPI) and CO5300 240×296 round AMOLED (QSPI), switched via a single compile-time define
- Screens: clock, playlist, equalizer, settings, Bluetooth, events, WiFi AP
- Rotary encoder navigation (turn + press)
- Capacitive touch (CST816D) — coexists with the rotary encoder; either input works at any time
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
- MQTT client — remote control of the radio (play/stop/volume/station) and a configurable on-device toggle widget that drives any external MQTT switch (Tasmota, zigbee2mqtt, etc.); see [MQTT](#mqtt) below

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
| Display | ILI9341 320×240 (SPI) or CO5300 240×296 AMOLED (QSPI) — selected at compile time |
| Touch | CST816D capacitive controller (I2C) — gestures detected by LVGL on the standard pointer indev |
| Input | Rotary encoder with push button + capacitive touch (swipes + tap-to-control overlay) |
| I2S mux | 74HC157D — hardware switch between ESP32-S3 and QCC5125 I2S outputs; controlled via GPIO |
| Audio out | I2S DAC / amplifier (fed from 74HC157D output) |
| Bluetooth | QCC5125 external module, Bluetooth 5.1, A2DP + HFP |
| Microphone | Built-in, used for HFP hands-free |
| Buzzer | LEDC PWM tone generator |

---

## Build

**Requirements**

- [ESP-IDF v5.5.4](https://github.com/espressif/esp-idf)
- [ESP-ADF v2.8](https://github.com/espressif/esp-adf)
- Custom AtlasCube board support (symlink into `esp-adf/components/audio_board/`)

**ESP-ADF submodules (required)**

After cloning ESP-ADF, initialize the required submodules — they contain pre-compiled libraries and are not populated by a plain clone:

```bash
git -C $ADF_PATH submodule update --init components/esp-adf-libs components/esp-sr
```

**AtlasCube board support**

ESP-ADF requires a board definition to compile — it uses it to initialize the audio codec, I2S pins, PA enable, and other hardware specifics. Every supported board in `esp-adf/components/audio_board/` is a separate subdirectory with `board.c`, `board.h`, and `board_pins_config.c`. The `esp32_s3_atlascube` component in this repo follows that same structure and was modelled after the existing ESP32-S3 board definitions shipped with ESP-ADF.

1. Add the board symlink:

```bat
mklink /D %ADF_PATH%\components\audio_board\esp32_s3_atlascube <path-to-repo>\components\audio_board\esp32_s3_atlascube
```

2. Register the board in `esp-adf/components/audio_board/Kconfig.projbuild`:

```kconfig
config ESP32_S3_ATLASCUBE_BOARD
    bool "ESP32-S3-AtlasCube"
```

3. Register the board in `esp-adf/components/audio_board/CMakeLists.txt`:

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

4. Register the board in `esp-adf/components/audio_board/component.mk`:

```makefile
ifdef CONFIG_ESP32_S3_ATLASCUBE_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./esp32_s3_atlascube
COMPONENT_SRCDIRS += ./esp32_s3_atlascube
endif
```

5. `sdkconfig.defaults` already contains the board selection:

```
CONFIG_ESP32_S3_ATLASCUBE_BOARD=y
```

**ESP-IDF FreeRTOS patch (required)**

ESP-ADF needs `xTaskCreateRestrictedPinnedToCore` which is not present in stock ESP-IDF v5.5.x. Apply the bundled patch once after installing ESP-IDF:

```bash
git -C $IDF_PATH apply --ignore-whitespace $ADF_PATH/idf_patches/idf_v5.5_freertos.patch
```

Without this patch the firmware boots but the MP3 decoder task fails to start at runtime:
`E AUDIO_THREAD: Not found right xTaskCreateRestrictedPinnedToCore`

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

### External toggle widget

The MQTT settings tab also configures **one user-defined switch** that appears as a dedicated on-device screen (swipe right from the clock). It targets any external MQTT switch.

- **Command topic** — published on tap with payload `ON`/`OFF`. Example (Tasmota): `cmnd/livingroom/POWER`. Example (zigbee2mqtt accepts plain text on `/set`): `zigbee2mqtt/<name>/set`.
- **State topic** — subscribed; updates the on-screen switch to reflect the device's real state, so the UI stays in sync if the device is toggled from elsewhere (HA, physical button, automation). Payload parser accepts:
  - plain text: `ON`/`OFF`/`on`/`off`/`true`/`false`/`1`/`0`
  - JSON with a `"state"` key (zigbee2mqtt's default state topic format)
- **Label** — short string shown above the switch.

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
- [`display_drivers.md`](docs/display_drivers.md) — display driver gotchas (QSPI AMOLED even-boundary, shared SPI mutex)

---

## Roadmap

- **Enclosure** — 3D-printed case currently in design; firmware is developed and tested on the bare development board
- **SD card** — local storage for station logos, music files, and voice notification clips
- **Additional displays** — SSD1322 (256×64 OLED) and ST7796 TFT support in progress (ILI9341 and CO5300 240×296 already supported)
- **Web melody editor** — in-browser tool for composing custom buzzer notification tunes

---

## License

MIT
