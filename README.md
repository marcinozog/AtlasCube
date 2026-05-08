# AtlasCube

A hobby project — internet radio and smart clock built on a custom ESP32-S3 board (Atlas Hub). Streams internet radio, shows a clock, manages reminders, and exposes a web UI for configuration. Everything runs on the device with no cloud dependency.

🌐 **[atlascube.net](https://atlascube.net)**

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
    <td align="center"><img src="https://atlascube.net/images/diagram.png" width="200"><br><sub>Diagram with ILI9341</sub></td>
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
- LVGL-based GUI on a 320×240 ILI9341 display
- Screens: clock, playlist, equalizer, settings, Bluetooth, events, WiFi AP
- Rotary encoder navigation (turn + press)
- Configurable layout (widget positions editable via JSON)

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
| Display | ILI9341, 320×240, SPI |
| Input | Rotary encoder with push button |
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

**Atlas Hub board support**

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
| Settings | `/settings.html` |
| Playlist | `/playlist.html` |
| Events | `/events.html` |

WebSocket endpoint: `ws://<device-ip>/ws` — pushes state changes (volume, track, radio state) in real time.

---

## Project docs

Architecture and design notes in [`docs/`](docs/):

- [`audio_pipeline.md`](docs/audio_pipeline.md) — streaming pipeline, DSP, TCP tuning, task affinity
- [`events.md`](docs/events.md) — reminder/event system design
- [`layout_editor.md`](docs/layout_editor.md) — UI layout customization

---

## Roadmap

- **Enclosure** — 3D-printed case currently in design; firmware is developed and tested on the bare development board
- **Touch input** — capacitive touch panel support
- **SD card** — local storage for station logos, music files, and voice notification clips
- **Additional displays** — SSD1322 (256×64 OLED) and ST7796 TFT support in progress
- **Web melody editor** — in-browser tool for composing custom buzzer notification tunes

---

## License

MIT
