#!/usr/bin/env python3
"""
AtlasCube release/CI build — NOT the script most users want.

This is what the CI matrix runs to build every hardware variant into a merged,
distributable image. It switches the HW variant in main/include/defines.h,
patches ESP-ADF/ESP-IDF, builds, and produces build/AtlasCube-<variant>.bin.

If you just want to build and flash your own configured board, use
scripts/build-flash.py instead — it sets up the toolchain on first run and never
touches your variant selection.

The one-time toolchain setup (clone + patch ESP-ADF/IDF) lives in
scripts/env_setup.py and is shared with build-flash.py.

Run inside an ESP-IDF environment (the "ESP-IDF PowerShell", or after export.ps1):
    python ci/build.py co5300
    python ci/build.py            # interactive variant menu
    python ci/build.py ssd1322 --skip-build   # only set up, don't compile

Pinned toolchain: ESP-IDF v5.5.4, ESP-ADF v2.8.
"""

import argparse
import re
import sys
from pathlib import Path

# Shared toolchain setup lives in scripts/env_setup.py.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
import env_setup  # noqa: E402
from env_setup import (REPO_ROOT, IDF_VERSION, ADF_VERSION, TARGET,  # noqa: E402
                       step, die, run, resolve_idf, resolve_adf, patch_adf)

# variant -> (DISPLAY, UI_PROFILE, TOUCH, FLASH); mirrors the old select-variant.sh table
VARIANTS = {
    "ili9341": ("ILI9341", "320x240",     "FT6336U", "16MB"),
    "st7796":  ("ST7796",  "480x320",     "FT6336U", "16MB"),
    "ili9488": ("ILI9488", "480x320",     "FT6336U", "16MB"),
    "co5300":  ("CO5300",  "240X296",     "CST816D", "16MB"),
    "ssd1322": ("SSD1322", "MONO_256X64", "NONE",    "16MB"),
    # XPT2046 resistive-touch variants (experimental — not HW-verified). Same
    # panels as above, resistive SPI touch instead of the I2C capacitive default.
    "ili9341-xpt2046": ("ILI9341", "320x240", "XPT2046", "16MB"),
    "st7796-xpt2046":  ("ST7796",  "480x320", "XPT2046", "16MB"),
    "ili9488-xpt2046": ("ILI9488", "480x320", "XPT2046", "16MB"),
}

# Full member lists per #define group, so exactly one stays uncommented.
GROUPS = {
    "DISPLAY":    ["DISPLAY_ILI9341", "DISPLAY_ST7796", "DISPLAY_ILI9488", "DISPLAY_CO5300", "DISPLAY_SSD1322"],
    "TOUCH":      ["TOUCH_FT6336U", "TOUCH_CST816D", "TOUCH_XPT2046", "TOUCH_NONE"],
    "UI_PROFILE": ["UI_PROFILE_240X296", "UI_PROFILE_320x240", "UI_PROFILE_480x320",
                   "UI_PROFILE_MONO_128X64", "UI_PROFILE_MONO_256X64"],
    "FLASH":      ["FLASH_16MB"],
}


# ── variant selection (replaces select-variant.sh) ──────────────────────────────

def set_group(text, active, members):
    """Uncomment `active`, comment every other member. Only touches standalone
    `#define NAME` lines (no value after the name), so DISPLAY_PIN_* etc. are safe."""
    for name in members:
        if name == active:
            # "// #define NAME"  ->  "#define NAME"
            pat = re.compile(rf"^([ \t]*)//[ \t]*(#define[ \t]+{name})([ \t]*)$", re.M)
            text = pat.sub(r"\1\2\3", text)
        else:
            # "#define NAME"  ->  "// #define NAME"
            pat = re.compile(rf"^([ \t]*)(#define[ \t]+{name})([ \t]*)$", re.M)
            text = pat.sub(r"\1// \2\3", text)
    return text


def select_variant(variant):
    display, profile, touch, flash = VARIANTS[variant]
    defines = REPO_ROOT / "main" / "include" / "defines.h"
    text = defines.read_text(encoding="utf-8")
    text = set_group(text, f"DISPLAY_{display}",    GROUPS["DISPLAY"])
    text = set_group(text, f"TOUCH_{touch}",        GROUPS["TOUCH"])
    text = set_group(text, f"UI_PROFILE_{profile}", GROUPS["UI_PROFILE"])
    text = set_group(text, f"FLASH_{flash}",        GROUPS["FLASH"])
    defines.write_text(text, encoding="utf-8")
    print(f"    DISPLAY_{display} + UI_PROFILE_{profile} + TOUCH_{touch} + FLASH_{flash}")


# ── assets + build + merge ──────────────────────────────────────────────────--

def idf_cmd(idf_py, *args):
    return [sys.executable, str(idf_py), *args]


def build(idf_py, variant, do_clean, do_build):
    step("Compressing web UI assets")
    run([sys.executable, str(REPO_ROOT / "spiffs_image" / "tools" / "compress_web.py")])

    if not do_build:
        print("\nSetup complete (--skip-build). Run `idf.py build` when ready.")
        return

    if do_clean:
        # set-target both selects ESP32-S3 (idf.py otherwise defaults to plain
        # esp32, whose smaller IRAM overflows at link time) and forces a clean
        # reconfigure so the freshly-switched variant in defines.h takes effect.
        step(f"idf.py set-target {TARGET}")
        run(idf_cmd(idf_py, "set-target", TARGET))

    step("idf.py build")
    run(idf_cmd(idf_py, "build"))

    out_name = f"AtlasCube-{variant}.bin"
    step(f"idf.py merge-bin -> build/{out_name}")
    run(idf_cmd(idf_py, "merge-bin", "-o", out_name))

    built = REPO_ROOT / "build" / out_name
    print(f"\nDone. Flashable image: {built}")
    print("Full image (bootloader + app + www + config partitions). A full flash "
          "re-seeds default settings; an OTA app update never touches them.\n"
          "Flash from offset 0x0 with the web flasher (atlascube.net/flash) or:\n"
          "    esptool.py --chip esp32s3 -p <PORT> write_flash 0x0 "
          f"build/{out_name}")


# ── entry point ────────────────────────────────────────────────────────────--

def pick_variant_interactively():
    if not sys.stdin.isatty():
        die("No variant given and not running interactively. "
            "Pass one of: " + ", ".join(VARIANTS))
    names = list(VARIANTS)
    print("Select hardware variant:")
    for i, name in enumerate(names, 1):
        disp, prof, touch, _ = VARIANTS[name]
        print(f"  {i}) {name:8} — {disp} / {prof} / touch {touch}")
    while True:
        choice = input("Number: ").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(names):
            return names[int(choice) - 1]
        print("Invalid choice.")


def main():
    ap = argparse.ArgumentParser(description="AtlasCube CI/release firmware build.")
    ap.add_argument("variant", nargs="?", choices=list(VARIANTS),
                    help="hardware variant (omit for an interactive menu)")
    ap.add_argument("--adf-path", help="path to esp-adf checkout (default: $ADF_PATH or ./esp-adf, cloned if absent)")
    ap.add_argument("--skip-build", action="store_true", help="set up variant + patches + assets, but don't compile")
    ap.add_argument("--no-clean", action="store_true", help="skip the set-target/clean reconfigure before build (faster, dev only; assumes target already esp32s3)")
    args = ap.parse_args()

    variant = args.variant or pick_variant_interactively()
    print(f"AtlasCube build — variant '{variant}' (ESP-IDF {IDF_VERSION}, ESP-ADF {ADF_VERSION})")

    idf, idf_py = resolve_idf()
    adf = resolve_adf(args.adf_path)

    step(f"Selecting variant '{variant}' in defines.h")
    select_variant(variant)

    patch_adf(adf, idf)

    build(idf_py, variant,
          do_clean=not args.no_clean,
          do_build=not args.skip_build)


if __name__ == "__main__":
    main()
