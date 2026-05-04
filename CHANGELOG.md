# Changelog

All notable changes to AtlasCube firmware are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

While the major version is `0`, any minor release may introduce breaking changes.

## [Unreleased]

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

[Unreleased]: https://github.com/marcinozog/AtlasCube/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/marcinozog/AtlasCube/releases/tag/v0.1.0
