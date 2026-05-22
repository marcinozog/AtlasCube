# Changelog

All notable changes to AtlasCube firmware are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

While the major version is `0`, any minor release may introduce breaking changes.

## [Unreleased]

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

[Unreleased]: https://github.com/marcinozog/AtlasCube/compare/v0.6.0...HEAD
[0.6.0]: https://github.com/marcinozog/AtlasCube/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/marcinozog/AtlasCube/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/marcinozog/AtlasCube/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/marcinozog/AtlasCube/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/marcinozog/AtlasCube/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/marcinozog/AtlasCube/releases/tag/v0.1.0
