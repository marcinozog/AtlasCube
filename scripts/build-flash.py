#!/usr/bin/env python3
"""
AtlasCube — build & flash. This is the script for end users.

Configure your hardware ONCE in main/include/defines.h (display driver, touch,
panel profile), then run this — there's nothing else to set up first. On the
first run it clones and patches ESP-ADF for you; afterwards it compresses the web
UI, builds, and asks how much of the device to overwrite:

  1) Everything (factory)     — bootloader + partition table + app + www + config
                                (default settings JSON). A full image: use on a
                                fresh or erased chip. This RESETS saved settings to
                                defaults.
  2) Firmware only            — app slot only, flashed over USB. Keeps the web UI
                                and your settings. Needs a bootloader already on
                                the chip (i.e. flashed before via option 1).
  3) Firmware + Web UI        — app + www partition, flashed over USB. Keeps your
                                settings.

The choices map to the flash layout: the app, the editable web UI (`www`), and
the user settings (`config`) live in separate partitions, so you can reflash code
without losing the UI, and reflash the UI without losing settings.

Tip: to tweak the web UI without flashing at all, edit files live in the browser
(the on-device file editor / the built-in setup page upload) — they write to the
www partition over HTTP.

It also guards against a stale sdkconfig: ESP-IDF only applies sdkconfig.defaults
(incl. the auto-generated sdkconfig.variant) when sdkconfig doesn't yet exist, so
switching the HW variant in defines.h leaves the OLD CONFIG_DISPLAY_*/TOUCH_*
baked into sdkconfig. The result is a half-switched binary (e.g. new UI_PROFILE
header but old display driver). Before flashing we compare defines.h against
sdkconfig and, on a mismatch, delete sdkconfig so the next reconfigure rebuilds
it cleanly (prompted; auto when non-interactive; forced with --clean).

Run inside an ESP-IDF environment (the "ESP-IDF PowerShell", or after export.ps1):
    python scripts/build-flash.py                    # ask what to do, then build (+flash)
    python scripts/build-flash.py -p COM5            # explicit serial port
    python scripts/build-flash.py --scope fw         # skip the prompt (fw|ui|all|build)
    python scripts/build-flash.py --scope build      # compile only, don't flash
    python scripts/build-flash.py --scope all --monitor
    python scripts/build-flash.py --clean            # force a clean sdkconfig first

For CI / multi-variant release images, use ci/build.py instead.
"""

import argparse
import re
import sys
from pathlib import Path

# Toolchain setup (clone + patch ESP-ADF/IDF) and shared helpers live in
# scripts/env_setup.py — also used by ci/build.py.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from env_setup import (REPO_ROOT, BOARD_NAME, die, warn, run,  # noqa: E402
                       resolve_idf, resolve_adf, patch_adf)

DEFINES_H = REPO_ROOT / "main" / "include" / "defines.h"
SDKCONFIG = REPO_ROOT / "sdkconfig"
WWW_IMAGE = REPO_ROOT / "build" / "www.bin"


def active_define(text, prefix):
    """Return the single uncommented `#define <prefix>NAME` member, or None."""
    m = re.search(rf"^[ \t]*#define[ \t]+({re.escape(prefix)}\w+)[ \t]*$", text, re.M)
    return m.group(1) if m else None


def expected_sdkconfig_tokens():
    """The CONFIG_* lines the current defines.h variant should produce in sdkconfig,
    mirroring the sdkconfig.variant generation in the top-level CMakeLists.txt."""
    text = DEFINES_H.read_text(encoding="utf-8")
    tokens = []
    display = active_define(text, "DISPLAY_")
    touch = active_define(text, "TOUCH_")
    flash = active_define(text, "FLASH_")
    if display:
        tokens.append(f"CONFIG_{display}=y")
    if touch:
        tokens.append(f"CONFIG_{touch}=y")
    if flash:  # FLASH_16MB -> CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
        tokens.append(f"CONFIG_ESPTOOLPY_FLASHSIZE_{flash[len('FLASH_'):]}=y")
    return tokens


def sdkconfig_is_stale():
    """True if sdkconfig exists but doesn't match the variant in defines.h."""
    if not SDKCONFIG.is_file():
        return False  # nothing to clean; reconfigure will generate it fresh
    sdk = SDKCONFIG.read_text(encoding="utf-8")
    return any(tok not in sdk for tok in expected_sdkconfig_tokens())


def ensure_fresh_sdkconfig(force):
    """Delete sdkconfig when it's from another variant, so the next reconfigure
    regenerates it cleanly. `force` (--clean) skips detection and the prompt."""
    if force:
        if SDKCONFIG.is_file():
            print("--clean: removing sdkconfig to force a clean reconfigure.")
            SDKCONFIG.unlink()
        return

    if not sdkconfig_is_stale():
        return

    print("sdkconfig is from a different HW variant than defines.h "
          "(stale CONFIG_DISPLAY_*/TOUCH_* would build a half-switched binary).")
    if sys.stdin.isatty():
        ans = input("Delete sdkconfig and build cleanly? [Y/n] ").strip().lower()
        if ans in ("n", "no"):
            warn("Keeping sdkconfig — the flashed binary may not match defines.h.")
            return
    else:
        warn("Non-interactive: deleting sdkconfig automatically.")
    SDKCONFIG.unlink()


def ensure_setup(idf_path, adf_arg, force):
    """Make sure ESP-ADF is cloned + patched so a plain build works. Reuses
    build.py's helpers (idempotent), but skips variant selection — the user sets
    the hardware in defines.h by hand. Fast-path: skip the patch step once the
    board is installed; `--setup` forces a re-run."""
    adf = resolve_adf(adf_arg)  # clones ./esp-adf only if missing; sets ADF_PATH
    board = adf / "components" / "audio_board" / BOARD_NAME
    if force or not board.exists():
        patch_adf(adf, idf_path)
    else:
        print(f"ESP-ADF already set up at {adf} — skipping patch (use --setup to re-run).")


ACTIONS = {
    "all":   "Everything / factory          (bootloader + partitions + app + www + config; works on a blank chip; RESETS settings)",
    "fw":    "Firmware only                 (app slot only, over USB; keeps web UI and settings)",
    "ui":    "Firmware + Web UI             (app + www, over USB; keeps settings)",
    "build": "Build only (e.g. OTA image)   (compile + compress web/*.gz; don't flash)",
    "erase": "Erase all                     (wipe the WHOLE flash: app + web UI + settings + NVS)",
}


def pick_action():
    if not sys.stdin.isatty():
        die("No --scope given and not running interactively. "
            "Pass --scope all|fw|ui|build.")
    names = list(ACTIONS)
    print("\nWhat do you want to do?")
    for i, name in enumerate(names, 1):
        if name == "all":
            print("\n  Build & flash to the device:")
        elif name == "build":
            print("\n  Other (no flash):")
        print(f"    {i}) {ACTIONS[name]}")
    print()
    default = str(names.index("fw") + 1)
    while True:
        choice = input(f"Number [{default}]: ").strip() or default
        if choice.isdigit() and 1 <= int(choice) <= len(names):
            return names[int(choice) - 1]
        print("Invalid choice.")


def list_serial_ports():
    """Detected serial ports, best-effort (empty if pyserial isn't importable)."""
    try:
        from serial.tools import list_ports
        return [p.device for p in list_ports.comports()]
    except Exception:
        return []


def pick_port():
    """Ask for the serial port when -p wasn't given. Without it idf.py auto-detects,
    which fails on boards whose native USB doesn't enumerate as a plain serial port."""
    ports = list_serial_ports()
    if ports:
        print("\nSerial port:")
        for i, p in enumerate(ports, 1):
            print(f"  {i}) {p}")
        print("  0) let idf.py auto-detect")
        while True:
            choice = input("Port number [1]: ").strip() or "1"
            if choice == "0":
                return None
            if choice.isdigit() and 1 <= int(choice) <= len(ports):
                return ports[int(choice) - 1]
            print("Invalid choice.")
    val = input("\nNo serial ports detected. Type a port (e.g. COM5), "
                "or leave empty to let idf.py auto-detect: ").strip()
    return val or None


def main():
    ap = argparse.ArgumentParser(description="AtlasCube build & flash (end-user script).")
    ap.add_argument("-p", "--port", help="serial port (e.g. COM5 / /dev/ttyUSB0); auto-detected if omitted")
    ap.add_argument("--scope", choices=list(ACTIONS), help="what to do: flash all|fw|ui, or build (compile only); skips the prompt")
    ap.add_argument("--monitor", action="store_true", help="open the serial monitor after flashing")
    ap.add_argument("--clean", action="store_true", help="force a clean sdkconfig before building")
    ap.add_argument("--setup", action="store_true", help="force re-running the ESP-ADF/IDF setup patches")
    ap.add_argument("--adf-path", help="path to an existing esp-adf checkout (default: $ADF_PATH or ./esp-adf, cloned if absent)")
    args = ap.parse_args()

    idf_path, idf_py = resolve_idf()

    # Ask up front so the user isn't prompted after a long build/setup.
    action = args.scope or pick_action()

    # Serial port for flashing actions (build-only needs none). Ask when -p was
    # omitted rather than leaning on idf.py auto-detect, which finds nothing on
    # boards whose native USB doesn't enumerate as a plain serial port.
    port = args.port
    if action != "build" and not port and sys.stdin.isatty():
        port = pick_port()

    def idf(*a):
        cmd = [sys.executable, str(idf_py)]
        if port:
            cmd += ["-p", port]
        return cmd + list(a)

    def parttool_write(partition, image):
        cmd = [sys.executable, str(idf_path / "components" / "partition_table" / "parttool.py")]
        if port:
            cmd += ["--port", port]
        return cmd + ["write_partition", "--partition-name", partition, "--input", str(image)]

    # First run clones + patches ESP-ADF (and exports ADF_PATH for the build);
    # later runs detect it's done and skip straight through.
    ensure_setup(idf_path, args.adf_path, args.setup)

    # Erase is a standalone, destructive flash op — no build needed.
    if action == "erase":
        if sys.stdin.isatty():
            ans = input("Erase the ENTIRE flash (app, web UI, settings, NVS)? [y/N] ").strip().lower()
            if ans not in ("y", "yes"):
                print("Aborted.")
                return
        run(idf("erase-flash"))
        print("\nFlash erased. Restore a working device with '--scope all' "
              "(or pick 'Everything' in the menu).")
        return

    # Drop a stale sdkconfig before configuring (variant switch guard).
    ensure_fresh_sdkconfig(args.clean)

    # Recompress the web UI so the www partition image is current, then build
    # everything (app + www.bin + config.bin) once; the action only changes which
    # parts get written to the device (if any).
    run([sys.executable, str(REPO_ROOT / "spiffs_image" / "tools" / "compress_web.py")])
    run(idf("build"))

    if action == "build":
        print("\nBuild only — not flashing. Compressed web UI is in spiffs_image/web/; "
              "build artifacts are in build/ (use ci/build.py / idf.py merge-bin "
              "for a distributable image).")
        return

    if action == "fw":
        run(idf("app-flash"))
    elif action == "ui":
        run(idf("app-flash"))
        if not WWW_IMAGE.is_file():
            die(f"{WWW_IMAGE} not found — build did not produce the www image.")
        run(parttool_write("www", WWW_IMAGE))
    else:  # all
        run(idf("flash"))

    if args.monitor:
        run(idf("monitor"))


if __name__ == "__main__":
    main()
