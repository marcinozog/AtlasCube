# Changelog

All notable changes to AtlasCube firmware are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

While the major version is `0`, any minor release may introduce breaking changes.

## [Unreleased]

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

[Unreleased]: https://github.com/marcinozog/AtlasCube/compare/v0.17.0...HEAD
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
