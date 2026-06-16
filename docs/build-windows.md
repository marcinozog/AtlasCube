# Building AtlasCube on Windows

This is the step-by-step path for building the firmware yourself on Windows — for
example to use a hardware variant or pin layout that isn't in the prebuilt
[releases](https://github.com/marcinozog/AtlasCube/releases/latest).

> **Don't need a custom build?** Just download the matching `.bin` from the
> [latest release](https://github.com/marcinozog/AtlasCube/releases/latest) and
> flash it with the [web flasher](https://atlascube.net/flash) or `esptool`. No
> toolchain required. Build from source only when you need a variant/pins that
> aren't shipped.

Building C firmware locally requires the ESP-IDF toolchain (a few GB, one-time).
There is no way around that — but the steps below are the whole story.

## 1. Install ESP-IDF v5.5.4

Use the **ESP-IDF Installation Manager (EIM)** — one installer that downloads
ESP-IDF, the toolchain and Python, and creates a ready-to-use terminal shortcut.

1. Download `eim-gui-windows-x64.exe` from the
   [EIM releases page](https://github.com/espressif/idf-im-ui/releases/latest).
2. Run it and choose **Custom installation**. EIM will offer to download any
   missing **Git** and **Python** for you — let it. (On a clean machine or in a
   sandbox you'll usually be missing both, so this is where they get installed.)
3. Select the target chip(s) — you can keep the default **all**, or narrow it to
   just **ESP32-S3**, which is the MCU AtlasCube runs on.
4. When it asks which ESP-IDF version to install, **select `v5.5.4`**. This is
   important: AtlasCube is pinned to v5.5.4, and the build will warn you if a
   different version is active.
5. Click **Continue** through the remaining screens, then **Start installation**
   and let it finish (this is the heavy, one-time download).

EIM creates a desktop shortcut named **`IDF_v5.5.4_Powershell`** that opens a
shell with ESP-IDF already activated — that's what you use in the next step.

## 2. Get the source, pick your variant, build & flash

1. Open the **ESP-IDF terminal** that EIM created (it has the environment set up;
   `IDF_PATH` and the IDF Python are already on `PATH`).
2. Clone the repo:

   ```powershell
   git clone https://github.com/marcinozog/AtlasCube.git
   cd AtlasCube
   ```
3. **Pick your hardware variant** in [`main/include/defines.h`](../main/include/defines.h):
   uncomment exactly one entry in each of the three `#define` groups —
   `DISPLAY_*`, `UI_PROFILE_*`, `TOUCH_*`.
4. Build and flash the connected board over USB:

   ```powershell
   python scripts/build-flash.py -p COM5
   ```

   (Substitute your serial port for `COM5`.)

`scripts/build-flash.py` is the all-in-one user script and does everything: on
its **first run** it clones ESP-ADF v2.8 (into `./esp-adf`) and patches ESP-ADF
and ESP-IDF; then it compresses the web UI, sets the target to `esp32s3`, builds,
and flashes the board — asking how much of the device to overwrite:

| Scope (`--scope`) | Flashes | Keeps |
|---|---|---|
| Everything / factory (`all`) | bootloader + partition table + app + `www` + `config` | — (works on a blank chip; resets settings) |
| Firmware only (`fw`) | app slot (OTA-style update) | web UI + settings |
| Firmware + Web UI (`ui`) | app + `www` partition | settings |
| Build only (`build`) | nothing (compile + `web/*.gz`) | everything |
| Erase all (`erase`) | wipes the whole flash (app + web UI + settings + NVS) | — |

On a fresh or erased chip pick **Everything / factory** (`all`) — it's the only
scope that also writes the bootloader and partition table, so the chip can boot;
`Firmware only` / `Firmware + Web UI` only update the app / web UI and need a
bootloader already present (a blank chip fails with `invalid header`). The web UI
(`www`) and your settings (`config`) live in separate partitions, so reflashing
code or the UI keeps your settings; only a factory flash resets them.

Pass `--scope all|fw|ui|build|erase` to skip the prompt, `--monitor` to open the
serial monitor afterwards, and `--clean` if you switched the HW variant in
`defines.h` (it detects a stale `sdkconfig` and offers to clean it anyway).

> **First flash gotcha (ESP32-S3 native USB):** hold the **BOOT** button while
> plugging in USB, then release, so the chip enters download mode — the running
> firmware drives native USB-CDC and ignores the auto-reset.

## 3. Producing a merged release image (optional)

`scripts/build-flash.py` covers building & flashing your own board. If you want a
single `0x0`-flashable image instead — to distribute, or to flash with the
[web flasher](https://atlascube.net/flash) — use `ci/build.py`, the CI/release
entry point. It picks the variant from a CLI argument (overwriting `defines.h`),
runs the same setup, builds, and merges bootloader + partition table + app +
`www` + `config` into one file:

```powershell
python ci/build.py co5300         # or ili9341 / st7796 / ili9488 / ssd1322
python ci/build.py                # omit the variant for an interactive menu
```

It prints the path to the merged image:

```
build/AtlasCube-<variant>.bin
```

Flash it from offset `0x0` with the web flasher (use the "custom firmware"
option) or with esptool:

```powershell
esptool.py --chip esp32s3 -p COM5 write_flash 0x0 build/AtlasCube-<variant>.bin
```

## Useful flags

`scripts/build-flash.py`:

| Flag | Effect |
|---|---|
| `-p PORT` | Serial port (e.g. `COM5`); auto-detected if omitted. |
| `--scope all\|fw\|ui\|build\|erase` | What to do; skips the interactive prompt. |
| `--monitor` | Open the serial monitor after flashing. |
| `--clean` | Force a clean `sdkconfig` before building (variant switch). |
| `--setup` | Re-run just the ESP-ADF/IDF setup patches. |

`ci/build.py` (CI / release):

| Flag | Effect |
|---|---|
| `--skip-build` | Set up the variant + patches + web assets, but don't compile. |
| `--adf-path <path>` | Use an existing ESP-ADF checkout instead of cloning into `./esp-adf`. |
| `--no-clean` | Skip the set-target/clean reconfigure (faster rebuilds; dev only). |

Both scripts are idempotent — safe to re-run, including after switching variants.

## Troubleshooting

- **`IDF_PATH is not set`** — you didn't open the ESP-IDF terminal. Use the
  shortcut EIM created (or run `export.ps1` from your ESP-IDF install) before
  running `build-flash.py`.
- **A warning about the ESP-IDF / ESP-ADF version** — you installed a version
  other than v5.5.4 / v2.8. Reinstall ESP-IDF v5.5.4 via EIM; the build may fail
  on a mismatched version (the FreeRTOS patch targets v5.5).
- **`region 'iram0_0_seg' overflowed`** — this means the build ran for plain
  `esp32` instead of `esp32s3`. The build sets the target automatically; if it
  lingers, run `build-flash.py --clean` (or `ci/build.py` without `--no-clean`) so
  the target gets reconfigured.
