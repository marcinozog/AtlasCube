# Building AtlasCube on Linux

This is the step-by-step path for building the firmware yourself on Linux — for
example to use a hardware variant or pin layout that isn't in the prebuilt
[releases](https://github.com/marcinozog/AtlasCube/releases/latest).

> **Don't need a custom build?** Just download the matching `.bin` from the
> [latest release](https://github.com/marcinozog/AtlasCube/releases/latest) and
> flash it with the [web flasher](https://atlascube.net/flash) or `esptool`. No
> toolchain required. Build from source only when you need a variant/pins that
> aren't shipped.

Building C firmware locally requires the ESP-IDF toolchain (a few GB, one-time).
There is no way around that — but the steps below are the whole story. This is
the same flow the CI uses, so it's well-trodden.

## 1. Install ESP-IDF v5.5.4

ESP-IDF is pinned to **v5.5.4** — `ci/build.py` warns if a different version is
active, and the FreeRTOS patch targets v5.5, so a mismatch can fail the build.

1. Install the system prerequisites (Debian/Ubuntu shown; see the
   [ESP-IDF Linux setup guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/get-started/linux-macos-setup.html)
   for Fedora/Arch package names):

   ```bash
   sudo apt install git wget flex bison gperf python3 python3-pip python3-venv \
        cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
   ```

2. Clone ESP-IDF v5.5.4 and install the ESP32-S3 toolchain:

   ```bash
   mkdir -p ~/esp && cd ~/esp
   git clone -b v5.5.4 --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh esp32s3
   ```

3. Activate the environment **in every new terminal** before building:

   ```bash
   . ~/esp/esp-idf/export.sh
   ```

   This sets `IDF_PATH` and puts the IDF Python and tools on `PATH`. Tip: add an
   alias such as `alias get_idf='. ~/esp/esp-idf/export.sh'` to your `~/.bashrc`
   so you can just type `get_idf`.

## 2. Get the source and build

With the ESP-IDF environment active (step 1.3), clone the repo and build a
variant:

```bash
git clone https://github.com/marcinozog/AtlasCube.git
cd AtlasCube
python ci/build.py co5300        # or ili9341 / st7796 / ssd1322
python ci/build.py               # omit the variant for an interactive menu
```

`ci/build.py` does everything else: it clones ESP-ADF v2.8 (into `./esp-adf`),
selects the variant in `main/include/defines.h`, patches ESP-ADF and ESP-IDF,
compresses the web UI, sets the target to `esp32s3`, builds, and merges a single
flashable image.

> Building for your own board (not a release image)? You can skip straight to
> `scripts/build-flash.py` (next section) — it runs this same ESP-ADF setup on
> its first run and then builds + flashes. Just set your hardware in `defines.h`
> by hand first.

## 3. Flash

The build prints the path to the merged image:

```
build/AtlasCube-<variant>.bin
```

Flash it from offset `0x0` with the [web flasher](https://atlascube.net/flash)
(use the "custom firmware" option) or with esptool:

```bash
esptool.py --chip esp32s3 -p /dev/ttyACM0 write_flash 0x0 build/AtlasCube-<variant>.bin
```

For everyday build & flash during development, use `scripts/build-flash.py` — it
compresses the web UI, builds, and asks how much of the device to overwrite
(firmware only / firmware + web UI / everything). The web UI (`www`) and your
settings (`config`) live in separate partitions, so reflashing code or the UI
keeps your settings; only a factory flash resets them:

```bash
python scripts/build-flash.py -p /dev/ttyACM0
```

Substitute your serial port for `/dev/ttyACM0` (see Troubleshooting for how to
find it). Pass `--scope fw|ui|all` to skip the prompt (or `--scope build` to just
compile, no flash), `--monitor` to open the serial monitor afterwards.

> If you switched the HW variant in `defines.h`, `build-flash.py` detects the stale
> `sdkconfig` and offers to clean it (or pass `--clean` to force it).

## Useful flags (ci/build.py)

`ci/build.py` is the setup/release script (variant switch, ESP-ADF/IDF patches,
merged per-variant image). For build & flash to a board, prefer `build-flash.py` above.

| Flag | Effect |
|---|---|
| `--skip-build` | Set up the variant + patches + web assets, but don't compile. |
| `--adf-path <path>` | Use an existing ESP-ADF checkout instead of cloning into `./esp-adf`. |
| `--no-clean` | Skip the set-target/clean reconfigure (faster rebuilds; dev only). |

`ci/build.py` is idempotent — safe to re-run, including after switching variants.

## Troubleshooting

- **`IDF_PATH is not set`** — you didn't activate the environment in this
  terminal. Run `. ~/esp/esp-idf/export.sh` (or your alias) before
  `ci/build.py`. It has to be re-run in every new shell.
- **A warning about the ESP-IDF / ESP-ADF version** — you installed a version
  other than v5.5.4 / v2.8. Re-clone ESP-IDF at `-b v5.5.4`; the build may fail
  on a mismatched version (the FreeRTOS patch targets v5.5).
- **`region 'iram0_0_seg' overflowed`** — the build ran for plain `esp32`
  instead of `esp32s3`. `ci/build.py` sets the target automatically; if you passed
  `--no-clean` on a fresh checkout, drop it so the target gets configured.
- **Which serial port?** ESP32-S3 over its native USB usually enumerates as
  `/dev/ttyACM0`; through an external USB-UART bridge it's `/dev/ttyUSB0`. Run
  `ls /dev/ttyACM* /dev/ttyUSB*` before and after plugging the board in to spot
  the new device.
- **`Permission denied: '/dev/ttyACM0'`** — your user isn't in the serial group.
  Run `sudo usermod -aG dialout $USER` (on Arch the group is `uucp`), then log
  out and back in. As a one-off you can instead flash with `sudo`.
- **Flashing fails / device not detected** — the firmware drives the native USB
  port and ignores the DTR/RTS auto-reset, so esptool can't put it into download
  mode on its own. Hold the **BOOT** button while plugging in (or while pressing
  RESET), then start the flash. Same trick the web flasher needs.
- **`dfu-util` / `libusb` errors** — install the prerequisites from step 1; the
  `libusb-1.0-0` and `dfu-util` packages are easy to miss and only bite at flash
  time.
