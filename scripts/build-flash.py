#!/usr/bin/env python3
"""
AtlasCube — build & flash. This is the script for end users.

Configure your hardware ONCE in main/include/defines.h (display driver, touch,
panel profile), then run this. It compresses the web UI, builds, and asks how
much of the device to overwrite:

  1) Firmware only        — app slot only. Keeps the web UI and your settings.
  2) Firmware + Web UI    — app + www partition. Keeps your settings.
  3) Everything (factory) — app + www + config (default settings JSON). This
                            RESETS saved settings to defaults.

The three choices map to the flash layout: the app, the editable web UI (`www`),
and the user settings (`config`) live in separate partitions, so you can reflash
code without losing the UI, and reflash the UI without losing settings.

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

For CI / multi-variant release images, use scripts/build.py instead.
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFINES_H = REPO_ROOT / "main" / "include" / "defines.h"
SDKCONFIG = REPO_ROOT / "sdkconfig"
WWW_IMAGE = REPO_ROOT / "build" / "www.bin"


def die(msg):
    print(f"ERROR: {msg}", file=sys.stderr, flush=True)
    sys.exit(1)


def warn(msg):
    print(f"WARN: {msg}", file=sys.stderr, flush=True)


def resolve_idf():
    idf = os.environ.get("IDF_PATH")
    if not idf or not Path(idf).is_dir():
        die("IDF_PATH is not set — run this from inside an ESP-IDF environment "
            "(open the ESP-IDF terminal, or run its export.ps1 / export.sh first).")
    idf_py = Path(idf) / "tools" / "idf.py"
    if not idf_py.is_file():
        die(f"idf.py not found at {idf_py} — is IDF_PATH correct?")
    return Path(idf), idf_py


def run(cmd):
    print("    $ " + " ".join(str(c) for c in cmd), flush=True)
    subprocess.run(cmd, check=True)


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


ACTIONS = {
    "fw":    "Firmware only        (app slot; keeps web UI and settings)",
    "ui":    "Firmware + Web UI    (app + www; keeps settings)",
    "all":   "Everything / factory (app + www + config; RESETS settings)",
    "build": "Build only           (compile + compress web/*.gz; don't flash)",
}


def pick_action():
    if not sys.stdin.isatty():
        die("No --scope given and not running interactively. "
            "Pass --scope fw|ui|all|build.")
    names = list(ACTIONS)
    print("\nWhat do you want to do?")
    for i, name in enumerate(names, 1):
        print(f"  {i}) {ACTIONS[name]}")
    while True:
        choice = input("Number [1]: ").strip() or "1"
        if choice.isdigit() and 1 <= int(choice) <= len(names):
            return names[int(choice) - 1]
        print("Invalid choice.")


def main():
    ap = argparse.ArgumentParser(description="AtlasCube build & flash (end-user script).")
    ap.add_argument("-p", "--port", help="serial port (e.g. COM5 / /dev/ttyUSB0); auto-detected if omitted")
    ap.add_argument("--scope", choices=list(ACTIONS), help="what to do: flash fw|ui|all, or build (compile only); skips the prompt")
    ap.add_argument("--monitor", action="store_true", help="open the serial monitor after flashing")
    ap.add_argument("--clean", action="store_true", help="force a clean sdkconfig before building")
    args = ap.parse_args()

    idf_path, idf_py = resolve_idf()

    def idf(*a):
        cmd = [sys.executable, str(idf_py)]
        if args.port:
            cmd += ["-p", args.port]
        return cmd + list(a)

    def parttool_write(partition, image):
        cmd = [sys.executable, str(idf_path / "components" / "partition_table" / "parttool.py")]
        if args.port:
            cmd += ["--port", args.port]
        return cmd + ["write_partition", "--partition-name", partition, "--input", str(image)]

    # Ask up front so the user isn't prompted after a long build.
    action = args.scope or pick_action()

    # Drop a stale sdkconfig before configuring (variant switch guard).
    ensure_fresh_sdkconfig(args.clean)

    # Recompress the web UI so the www partition image is current, then build
    # everything (app + www.bin + config.bin) once; the action only changes which
    # parts get written to the device (if any).
    run([sys.executable, str(REPO_ROOT / "spiffs_image" / "tools" / "compress_web.py")])
    run(idf("build"))

    if action == "build":
        print("\nBuild only — not flashing. Compressed web UI is in spiffs_image/web/; "
              "build artifacts are in build/ (use scripts/build.py / idf.py merge-bin "
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
