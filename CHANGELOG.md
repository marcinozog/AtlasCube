# Changelog

All notable changes to AtlasCube firmware are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

While the major version is `0`, any minor release may introduce breaking changes.

## [Unreleased]

## [0.30.0] - 2026-06-25

### Added
- **display:** add 180° screen flip option
- **ui:** wire BT transport in controls overlay + faithful play state
- **ui:** add Clock/Radio nav-ring show/hide toggles
- **ui:** add unified Home hub screen with source-aware overlay
- **settings:** group colour palette into labelled sections
- **theme:** per-theme VU meter colours (vu_bg/vu_bar)

### Changed
- **ui:** tighten settings row height across graphical profiles
- **ui:** remove standalone clock screen, superseded by Home hub

### Fixed
- **ui:** show BT artist/title in Home strip instead of radio station

## [0.29.0] - 2026-06-24

### Added
- **setup:** export/import pin map to/from JSON file

## [0.28.0] - 2026-06-24

### Added
- **setup:** group GPIO pins into labelled sections with descriptions

## [0.27.0] - 2026-06-24

### Added
- **pins:** make I2S pins runtime-configurable too

### Fixed
- **pins:** handle -1 (absent peripheral) without crashing

## [0.26.0] - 2026-06-24

### Added
- **pins:** runtime GPIO pin map configurable via /setup
- **ui:** adjust vu meter
- **eq:** touch support for equalizer
- **eq:** touch support for equalizer band sliders

### Changed
- **ui:** drop unused physical-button inputs

### Fixed
- **ui:** sync controls overlay play/stop with external state changes
- **eq:** don't clip round slider knobs at min/max
- **touch:** keep polling while finger held so swipes aren't fragmented

## [0.25.0] - 2026-06-22

### Added
- **ui:** VU meter on SD player screen + per-screen toggle
- **ui:** real-audio FFT spectrum VU meter on the radio screen
- **audio:** play segmented HLS (.m3u8) radio streams
- **scripts:** add "Update from git" menu option to build-flash.py
- **ui:** toggle to show/hide SD player screen in nav ring
- **ui:** decorative "Fake VU" animated background

### Changed
- **ui:** remove animated "Fake VU" background

### Fixed
- **eq:** narrow EQ slider range to -13..+6 dB
- **mem:** reclaim ~84 KB internal DRAM to stabilize HTTPS radio
- **ui:** VU background follows theme and costs less on CPU1
- **web:** switch to wallpaper clears VU sentinel path
- **audio:** stop socket exhaustion on live HLS streams
- **web:** keep UI loadable while radio HTTPS stream runs
- **web:** drag playlist rows only from the grip handle
- **ntp:** apply saved timezone and servers on boot

## [0.24.0] - 2026-06-18

### Added
- **ui:** controls widget on screen clock
- **ui:** center-anchor clock_widget and BT/radio screen labels
- **ui:** anchored auto-width labels that don't wander on text change
- **ui:** add idle auto-dim screensaver

### Fixed
- **ui:** clock controls resume SD instead of starting radio

## [0.23.0] - 2026-06-17

### Added
- **ui:** custom splash logo from SD
- **web:** preview LVGL .bin images in browser
- **ui:** add SD wallpaper background option
- **web:** add config partition to the file editor
- **web:** replace "back to radio" with bottom toolbar nav on events/settings
- **sd-player:** on-screen control changes pause to stop/play toggle

## [0.22.0] - 2026-06-16

### Added
- **web:** add cross-page toolbar nav to bt.html and sd-player.html
- **events:** timed playback schedules (radio or SD file/folder)
- **audio:** auto-detect stream codec from Content-Type, relink on mismatch

### Changed
- **events:** replace alarm type with playback schedules

### Fixed
- **web:** style "Use this folder" button as primary in events.html

## [0.21.2] - 2026-06-15

### Fixed
- **build:** default to esp32s3 target on a fresh checkout
- **compress-web:** use ASCII arrow so logging doesn't crash on cp1250

## [0.21.1] - 2026-06-15

### Changed
- move playlist.csv from config to www partition

## [0.21.0] - 2026-06-15

### Added
- **build-flash:** add "first flash" option for blank chips
- **build-flash:** group the menu
- **build-flash:** add full chip erase menu option
- **build-flash:** prompt for the serial port when -p is omitted
- **scripts:** build-flash.py self-sets-up ESP-ADF on first run (single user entry) + scripts/README
- **fs:** isolate settings on a config partition (www/config split) + built-in setup page
- **ui:** ui profile for sd player screen
- **ui:** on-device SD music player + file browser (LVGL)
- **audio:** ID3 track titles + resume SD music after voice notification

### Changed
- **scripts:** split toolchain setup into env_setup.py, move build.py → ci/build.py; build-flash.py is the sole user script
- **ram:** recover internal RAM + drop deferred settings write

### Fixed
- **ui:** show now-playing only for the active source

## [0.20.0] - 2026-06-14

### Added
- **web:** English SD player UI + embedded EQ modal
- **sd:** pause/resume + shuffle/repeat
- **sd:** folder browsing + EQ link on the SD player
- **web:** SD Player link on home + volume on the SD page
- **sd:** folder-queue music player with web UI
- **audio:** WS play_file test hook for SD music playback
- **ws:** accept BT transport commands over plain-text WS

### Changed
- **audio:** split audio_player into engine + net/file source layers
- **web:** rename file tools for clarity, file manager first
- **web:** rename file tools for clarity — spiffs-editor / sd-editor / manager

### Fixed
- **audio:** BT↔SD source exclusivity + httpd stack overflow

### Other
- **ws:** drop plain-text BT transport handlers

## [0.19.0] - 2026-06-14

### Added
- **web:** browsable folder tree in the SPIFFS pane + SD play legend
- **web:** browsable folder tree in the SPIFFS panel
- **web:** show SPIFFS and SD usage in the file manager
- **events:** flat voice clip files, drop the per-clip folder
- **web:** play voice clips from the events list rows
- **web:** inline WAV/MP3 playback in the SD file manager
- **voice:** readable slug folder names, drop text sidecar

### Fixed
- **events:** widen sound path buffer for readable clip folder names
- **web:** label SD card consistently in file manager

## [0.18.0] - 2026-06-13

### Added
- **ui:** boot splash overlay with firmware version + STA IP
- **bt:** per-module AT command descriptor, web sync/transport controls, codec & sample-rate info
- **web:** move layout editor link from Tools to Display tab
- **web:** add icon legend for each file-manager panel
- **web:** add "backup all SPIFFS files" to file manager
- **web:** per-file copy buttons in file manager
- **web:** add dual-pane SPIFFS↔SD file manager with inline editor
- **screensaver:** overlay clock on photo frame

### Fixed
- **web:** stop file-manager editor modal colliding with shared .modal

## [0.17.0] - 2026-06-13

### Added
- **display:** add ILI9488 480x320 driver variant
- **buzzer:** allow disabling buzzer with BUZZER_PIN -1
- **events:** self-describing voice clips — sidecar text + WWW preview

## [0.16.0] - 2026-06-12

### Added
- **events:** voice notifications - playing WAV from SD at a given time
- **screensaver:** apply photo-frame config live
- **web:** add mkdir + rename to SD file manager
- **web:** add New folder (mkdir) to SD file manager
- **sd:** enable LVGL POSIX fs (S:→/sdcard); finalize SD/display pins
- **web:** SD card file manager
- **sd:** add SDMMC 1-bit SD card mount

### Changed
- **screensaver:** partial invalidation + lower LVGL priority

### Fixed
- **mem:** persist WiFi RAM tuning in defaults; revert LVGL to prio 5
- **sd:** move SD to GPIO 12/13/11, display to 39/40
- **web:** correct SD file manager build errors

## [0.15.0] - 2026-06-11

### Added
- **audio:** exclusive audio source auto-switch (Radio ⇄ Bluetooth)
- **clock:** optional IP + <hostname>.local on the clock screen
- **ws:** expose WiFi RSSI in state broadcast
- **settings:** rename WiFi tab to Network
- **mdns:** advertise device as <hostname>.local

### Fixed
- **playlist:** change max entry to 512 on the web
- **bt:** make exclusive auto-switch reliable

### Added
- **bluetooth:** exclusive audio source — starting BT play stops the radio and starting the radio pauses the phone, with automatic source switching (toggleable, default on; in web BT panel and on-device Settings)

## [0.14.0] - 2026-06-10

### Added
- **playlist:** increase max entries
- **radio:** resume playback on boot if it was playing
- **screensaver:** add digital clock screensaver
- **build:** prompt whether to bundle the web UI into storage

### Changed
- **audio_board:** centralize I2S pins in defines.h

### Fixed
- **radio:** play http SHOUTcast streams via raw socket
- **build:** don't clobber a symlinked/junctioned board in ESP-ADF

## [0.13.0] - 2026-06-07

### Added
- **ota:** auto-rollback on a failed update
- **ota:** web-push firmware update with on-device progress
- **web:** audio source switch, move device screen to settings
- **ui:** move theme & background toggles to Colors tab, rename it to Theme
- **ui:** configurable clock & now-playing fonts in layout editor

### Fixed
- **ota:** render progress screen before the flash erase storm

## [0.12.0] - 2026-06-02

### Added
- **ui:** dithered gradient screen background

### Fixed
- **radio:** re-anchor curr_index after playlist edit
- **scripts:** guard flash-web.py against stale sdkconfig on variant switch

## [0.11.0] - 2026-06-01

### Added
- **night:** wake/sleep radio actions on the dimming schedule

### Changed
- **ui:** move night mode into its own Sleep & Wake tab

## [0.10.0] - 2026-06-01

### Added
- **ui:** centralize screen navigation into an editable ring table

### Changed
- **scripts:** unify build tooling as Python under scripts/

### Fixed
- **web:** no-cache for mutable data files (playlist/configs)

## [0.9.3] - 2026-05-31

### Fixed
- **build:** set esp32s3 target in build.py; add Windows build guide

## [0.9.2] - 2026-05-31

## [0.9.1] - 2026-05-30

### Fixed
- **ui:** ignore layout JSON saved for a different LCD size

## [0.9.0] - 2026-05-30

### Added
- **ui:** fonts added
- **ui:** auto-fit splash logo to the panel
- **display:** add SSD1322 256x64 mono variant
- **web:** show firmware version in UI and wifi setup

### Changed
- **screensaver:** changed default

### Fixed
- **scripts:** default flash-web.ps1 to flash when no args

## [0.8.1] - 2026-05-30

### Fixed
- **build:** ship SPIFFS web assets in CI/release binaries

## [0.8.0] - 2026-05-29

### Added
- **build:** select flash size per variant via defines.h
- **ui:** event indicator on radio screen + live refresh on add/edit/delete
- **events:** per-alarm volume override

### Fixed
- **wifi:** improve AP compatibility with modern phones
- **ui:** adjust screen radio
- **ui:** controls overlay reads mode from owner, not app_state

## [0.7.0] - 2026-05-28

### Added
- **web:** group editors in Tools tab, rename EQ button to Radio EQ
- **display:** add ST7796U 480x320 driver and UI profile
- **web:** move Web files editor to a new Tools settings tab
- **web:** add Clear button and follow/pause auto-scroll to BT console
- **web:** move Bluetooth panel to dedicated bt.html page
- **ui:** dark/light theme for clockhands and dashboard screensavers
- **ui:** touch support in settings + global idle-timer hook
- **ui:** modified ui profile for ILI9341
- **touch:** FT6336U driver, rebuild touch orientation
- **events:** alarm type with radio stream wake-up
- **display:** night dimming schedule

### Changed
- **config:** pick HW variant only in defines.h

### Fixed
- **display:** shrink ST7796 LVGL buffer to fit internal DRAM budget
- **web:** show Dashboard widget settings only when Dashboard style selected

## [0.6.0] - 2026-05-22

### Added
- **ui:** mqtt screensaver
- **www:** pretty-print and validate JSON in file editor
- **web:** rebuild active screen after MQTT config save
- **mqtt:** mqtt widgets
- **mqtt:** mqtt client
- **ui:** controls widget resets screensaver idle timer

### Changed
- **widget:** changed control widget auto hide time
- **mqtt:** remove client id from base topic

### Fixed
- **mqtt:** wrap widget command payload in JSON when json_path is set
- **ui:** clear scrollable flag for main screens
- **screensaver:** disable screensaver in wifi ap mode

## [0.5.0] - 2026-05-18

### Added
- **ui:** Touch closes the event notification screen
- **ui:** Touch closes the screensaver
- **playlist:** play station on row tap
- **ui:** volume widget and gestures
- **ui:** controls overlay widget
- **touch:** CST816D capacitive driver
- **bluetooth:** audio metadata
- **screensaver:** alarms for dashboard
- **screensaver:** dashboard and blank
- **ui:** add screensaver entry to settings menu

### Changed
- **screensaver:** live update setting for dashboard screensaver
- **screensaver:** remove enable status (0s - OFF)

### Fixed
- **bluetooth:** fixed refresh title

## [0.4.0] - 2026-05-13

### Added
- **ui:** Added screensavers

## [0.3.0] - 2026-05-11

### Added
- **www:** On-device web file editor

### Changed
- **ui:** single source of truth for display dimensions
- **layout:** Clock, mode, events widgets change to absolute pos
- **www:** Added radio layout to editor
- **display:** Modiefied CO5300 layout

## [0.2.0] - 2026-05-09

### Added
- **display:** Added CO5300 display driver
- **playlist:** Added fav stations and import/export

### Other
- Add diagram image to README table

## [0.1.0] - 2026-05-04

Initial public snapshot pushed to GitHub.

### Added
- Audio pipeline (player, playlist, internet radio service).
- Wi-Fi manager with STA + AP fallback.
- HTTP and WebSocket servers.
- Display + UI manager with profile loading from filesystem.
- Bluetooth, NTP, encoder, buzzer, events service.
- SPIFFS-based settings and assets.
- Firmware version derived from `git describe` and logged at boot.

[Unreleased]: https://github.com/marcinozog/AtlasCube/compare/v0.30.0...HEAD
[0.30.0]: https://github.com/marcinozog/AtlasCube/compare/v0.29.0...v0.30.0
[0.29.0]: https://github.com/marcinozog/AtlasCube/compare/v0.28.0...v0.29.0
[0.28.0]: https://github.com/marcinozog/AtlasCube/compare/v0.27.0...v0.28.0
[0.27.0]: https://github.com/marcinozog/AtlasCube/compare/v0.26.0...v0.27.0
[0.26.0]: https://github.com/marcinozog/AtlasCube/compare/v0.25.0...v0.26.0
[0.25.0]: https://github.com/marcinozog/AtlasCube/compare/v0.24.0...v0.25.0
[0.24.0]: https://github.com/marcinozog/AtlasCube/compare/v0.23.0...v0.24.0
[0.23.0]: https://github.com/marcinozog/AtlasCube/compare/v0.22.0...v0.23.0
[0.22.0]: https://github.com/marcinozog/AtlasCube/compare/v0.21.2...v0.22.0
[0.21.2]: https://github.com/marcinozog/AtlasCube/compare/v0.21.1...v0.21.2
[0.21.1]: https://github.com/marcinozog/AtlasCube/compare/v0.21.0...v0.21.1
[0.21.0]: https://github.com/marcinozog/AtlasCube/compare/v0.20.0...v0.21.0
[0.20.0]: https://github.com/marcinozog/AtlasCube/compare/v0.19.0...v0.20.0
[0.19.0]: https://github.com/marcinozog/AtlasCube/compare/v0.18.0...v0.19.0
[0.18.0]: https://github.com/marcinozog/AtlasCube/compare/v0.17.0...v0.18.0
[0.17.0]: https://github.com/marcinozog/AtlasCube/compare/v0.16.0...v0.17.0
[0.16.0]: https://github.com/marcinozog/AtlasCube/compare/v0.15.0...v0.16.0
[0.15.0]: https://github.com/marcinozog/AtlasCube/compare/v0.14.0...v0.15.0
[0.14.0]: https://github.com/marcinozog/AtlasCube/compare/v0.13.0...v0.14.0
[0.13.0]: https://github.com/marcinozog/AtlasCube/compare/v0.12.0...v0.13.0
[0.12.0]: https://github.com/marcinozog/AtlasCube/compare/v0.11.0...v0.12.0
[0.11.0]: https://github.com/marcinozog/AtlasCube/compare/v0.10.0...v0.11.0
[0.10.0]: https://github.com/marcinozog/AtlasCube/compare/v0.9.3...v0.10.0
[0.9.3]: https://github.com/marcinozog/AtlasCube/compare/v0.9.2...v0.9.3
[0.9.2]: https://github.com/marcinozog/AtlasCube/compare/v0.9.1...v0.9.2
[0.9.1]: https://github.com/marcinozog/AtlasCube/compare/v0.9.0...v0.9.1
[0.9.0]: https://github.com/marcinozog/AtlasCube/compare/v0.8.1...v0.9.0
[0.8.1]: https://github.com/marcinozog/AtlasCube/compare/v0.8.0...v0.8.1
[0.8.0]: https://github.com/marcinozog/AtlasCube/compare/v0.7.0...v0.8.0
[0.7.0]: https://github.com/marcinozog/AtlasCube/compare/v0.6.0...v0.7.0
[0.6.0]: https://github.com/marcinozog/AtlasCube/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/marcinozog/AtlasCube/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/marcinozog/AtlasCube/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/marcinozog/AtlasCube/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/marcinozog/AtlasCube/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/marcinozog/AtlasCube/releases/tag/v0.1.0
