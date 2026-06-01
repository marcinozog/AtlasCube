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
   important: AtlasCube is pinned to v5.5.4, and `build.py` will warn you if a
   different version is active.
5. Click **Continue** through the remaining screens, then **Start installation**
   and let it finish (this is the heavy, one-time download).

EIM creates a desktop shortcut named **`IDF_v5.5.4_Powershell`** that opens a
shell with ESP-IDF already activated — that's what you use in the next step.

## 2. Get the source and build

1. Open the **ESP-IDF terminal** that EIM created (it has the environment set up;
   `IDF_PATH` and the IDF Python are already on `PATH`).
2. Clone the repo and build a variant:

   ```powershell
   git clone https://github.com/marcinozog/AtlasCube.git
   cd AtlasCube
   python scripts/build.py co5300        # or ili9341 / st7796 / ssd1322
   python scripts/build.py               # omit the variant for an interactive menu
   ```

`build.py` does everything else: it clones ESP-ADF v2.8 (into `./esp-adf`),
selects the variant in `main/include/defines.h`, patches ESP-ADF and ESP-IDF,
compresses the web UI, sets the target to `esp32s3`, builds, and merges a single
flashable image.

## 3. Flash

The build prints the path to the merged image:

```
build/AtlasCube-<variant>.bin
```

Flash it from offset `0x0` with the [web flasher](https://atlascube.net/flash)
(use the "custom firmware" option) or with esptool:

```powershell
esptool.py --chip esp32s3 -p COM5 write_flash 0x0 build/AtlasCube-<variant>.bin
```

(Substitute your serial port for `COM5`.)

## Useful flags

| Flag | Effect |
|---|---|
| `--skip-build` | Set up the variant + patches + web assets, but don't compile. |
| `--no-spiffs` | Don't bundle the web UI (smaller image, no on-device web pages). |
| `--adf-path <path>` | Use an existing ESP-ADF checkout instead of cloning into `./esp-adf`. |
| `--no-clean` | Skip the set-target/clean reconfigure (faster rebuilds; dev only). |

`build.py` is idempotent — safe to re-run, including after switching variants.

## Troubleshooting

- **`IDF_PATH is not set`** — you didn't open the ESP-IDF terminal. Use the
  shortcut EIM created (or run `export.ps1` from your ESP-IDF install) before
  running `build.py`.
- **A warning about the ESP-IDF / ESP-ADF version** — you installed a version
  other than v5.5.4 / v2.8. Reinstall ESP-IDF v5.5.4 via EIM; the build may fail
  on a mismatched version (the FreeRTOS patch targets v5.5).
- **`region 'iram0_0_seg' overflowed`** — this means the build ran for plain
  `esp32` instead of `esp32s3`. `build.py` sets the target automatically; if you
  passed `--no-clean` on a fresh checkout, drop it so the target gets configured.
