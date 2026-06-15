#!/usr/bin/env python3
"""
AtlasCube release/CI build — the ADVANCED entry point. NOT the script most users
want: it switches HW variant, patches ESP-ADF/ESP-IDF, and produces a merged
per-variant image for distribution. If you just want to build and flash your own
configured board, use scripts/build-flash.py instead.

This script is what the CI matrix runs to build all variants; it can also set up
a fresh toolchain locally. It replaces the former bash helpers (select-variant.sh
+ patch-esp-adf.sh) with a single cross-platform script so a fresh Windows machine
(or sandbox) only needs the official ESP-IDF installer — no bash, no extra tooling.

What it does, end to end:
  1. Locate ESP-IDF (IDF_PATH) and ESP-ADF (ADF_PATH), cloning ESP-ADF v2.8 if absent.
  2. Switch the hardware variant in main/include/defines.h.
  3. Patch ESP-ADF (board sources, Kconfig/CMake registration) and ESP-IDF (FreeRTOS).
  4. Compress the web UI assets into the SPIFFS image.
  5. idf.py fullclean + build (with the web UI bundled).
  6. idf.py merge-bin -> build/AtlasCube-<variant>.bin, ready to flash.

Run inside an ESP-IDF environment (the "ESP-IDF PowerShell", or after export.ps1):
    python scripts/build.py co5300
    python scripts/build.py            # interactive variant menu
    python scripts/build.py ssd1322 --skip-build   # only set up, don't compile

Pinned toolchain: ESP-IDF v5.5.4, ESP-ADF v2.8.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BOARD_NAME = "esp32_s3_atlascube"

IDF_VERSION = "v5.5.4"
ADF_VERSION = "v2.8"
ADF_REPO = "https://github.com/espressif/esp-adf.git"
TARGET = "esp32s3"
EIM_URL = "https://github.com/espressif/idf-im-ui/releases/latest"

# variant -> (DISPLAY, UI_PROFILE, TOUCH, FLASH); mirrors the old select-variant.sh table
VARIANTS = {
    "ili9341": ("ILI9341", "320x240",     "FT6336U", "16MB"),
    "st7796":  ("ST7796",  "480x320",     "FT6336U", "16MB"),
    "ili9488": ("ILI9488", "480x320",     "FT6336U", "16MB"),
    "co5300":  ("CO5300",  "240X296",     "CST816D", "16MB"),
    "ssd1322": ("SSD1322", "MONO_256X64", "NONE",    "16MB"),
}

# Full member lists per #define group, so exactly one stays uncommented.
GROUPS = {
    "DISPLAY":    ["DISPLAY_ILI9341", "DISPLAY_ST7796", "DISPLAY_ILI9488", "DISPLAY_CO5300", "DISPLAY_SSD1322"],
    "TOUCH":      ["TOUCH_FT6336U", "TOUCH_CST816D", "TOUCH_NONE"],
    "UI_PROFILE": ["UI_PROFILE_240X296", "UI_PROFILE_320x240", "UI_PROFILE_480x320",
                   "UI_PROFILE_MONO_128X64", "UI_PROFILE_MONO_256X64"],
    "FLASH":      ["FLASH_16MB"],
}


# ── small helpers ─────────────────────────────────────────────────────────────

def step(msg):
    print(f"\n==> {msg}", flush=True)


def warn(msg):
    print(f"WARN: {msg}", file=sys.stderr, flush=True)


def die(msg):
    print(f"ERROR: {msg}", file=sys.stderr, flush=True)
    sys.exit(1)


def run(cmd, **kwargs):
    """Run a command, echoing it; raise on non-zero exit."""
    print("    $ " + " ".join(str(c) for c in cmd), flush=True)
    subprocess.run(cmd, check=True, **kwargs)


def git_ok(args):
    """True if a git command exits 0, swallowing its output (for idempotency probes)."""
    return subprocess.run(["git", *args],
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL).returncode == 0


# ── 1. environment ──────────────────────────────────────────────────────────--

def resolve_idf():
    idf = os.environ.get("IDF_PATH")
    if not idf or not Path(idf).is_dir():
        die("IDF_PATH is not set — run this from inside an ESP-IDF environment.\n"
            f"       Easiest on Windows: install ESP-IDF {IDF_VERSION} with the ESP-IDF\n"
            f"       Installation Manager ({EIM_URL}),\n"
            "       then open the ESP-IDF terminal it creates and re-run this script.\n"
            "       (Already have ESP-IDF? Run its export.ps1 / export.sh first.)\n"
            "       See docs/build-windows.md for the full walkthrough.")
    idf_py = Path(idf) / "tools" / "idf.py"
    if not idf_py.is_file():
        die(f"idf.py not found at {idf_py} — is IDF_PATH correct?")
    # Soft version check: pinned to v5.5.4, but don't hard-fail on a near match.
    ver = (Path(idf) / "version.txt")
    if ver.is_file():
        got = ver.read_text(encoding="utf-8", errors="ignore").strip()
        if IDF_VERSION not in got:
            warn(f"ESP-IDF version is '{got}', AtlasCube is pinned to {IDF_VERSION}. "
                 "Build may fail if it differs significantly.")
    return Path(idf), idf_py


def resolve_adf(adf_arg):
    adf = adf_arg or os.environ.get("ADF_PATH") or str(REPO_ROOT / "esp-adf")
    adf = Path(adf)
    if not adf.is_dir():
        step(f"ESP-ADF not found at {adf} — cloning {ADF_VERSION}")
        run(["git", "clone", "--branch", ADF_VERSION, "--depth", "1", ADF_REPO, str(adf)])
    else:
        # Soft version check when the user supplied an existing checkout.
        out = subprocess.run(["git", "-C", str(adf), "describe", "--tags", "--always"],
                             capture_output=True, text=True)
        tag = out.stdout.strip()
        if tag and ADF_VERSION not in tag:
            warn(f"ESP-ADF checkout reports '{tag}', AtlasCube is pinned to {ADF_VERSION}.")
    os.environ["ADF_PATH"] = str(adf)
    return adf


# ── 2. variant selection (replaces select-variant.sh) ──────────────────────────

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


# ── 3. patch ESP-ADF + ESP-IDF (replaces patch-esp-adf.sh) ──────────────────────

def insert_before(text, marker_re, lines, sentinel):
    """Insert `lines` before the first line matching `marker_re`, unless `sentinel`
    already appears in the text (idempotent)."""
    if sentinel in text:
        return text, False
    out, done = [], False
    for line in text.splitlines(keepends=True):
        if not done and marker_re.search(line):
            for ins in lines:
                out.append(ins + "\n")
            done = True
        out.append(line)
    return "".join(out), done


def patch_adf(adf, idf):
    audio_board = adf / "components" / "audio_board"
    board_src = REPO_ROOT / "components" / "audio_board" / BOARD_NAME
    if not board_src.is_dir():
        die(f"board sources missing at {board_src}")

    step("ESP-ADF submodules (esp-adf-libs, esp-sr)")
    run(["git", "-C", str(adf), "submodule", "update", "--init",
         "components/esp-adf-libs", "components/esp-sr"])

    dest = audio_board / BOARD_NAME
    # A symlink/junction into the repo (preferred dev setup) stays live for both
    # build.py and the VSCode ESP-IDF extension (idf.py build) — don't clobber it.
    # Otherwise install a fresh copy (fresh ADF clone / CI).
    if dest.is_symlink() or getattr(os.path, "isjunction", lambda _: False)(str(dest)):
        step(f"Board sources linked at {dest} — leaving live link in place")
    else:
        step(f"Installing board sources into {dest}")
        if dest.exists():
            shutil.rmtree(dest)
        shutil.copytree(board_src, dest)

    step("Patching Kconfig.projbuild")
    kconfig = audio_board / "Kconfig.projbuild"
    text, changed = insert_before(
        kconfig.read_text(encoding="utf-8"),
        re.compile(r"^endchoice"),
        ['config ESP32_S3_ATLASCUBE_BOARD', '    bool "ESP32-S3-AtlasCube"'],
        sentinel="ESP32_S3_ATLASCUBE_BOARD",
    )
    kconfig.write_text(text, encoding="utf-8")
    print("    " + ("patched" if changed else "already patched"))

    step("Patching CMakeLists.txt")
    cmake = audio_board / "CMakeLists.txt"
    text, changed = insert_before(
        cmake.read_text(encoding="utf-8"),
        re.compile(r"register_component\(\)"),
        ['if (CONFIG_ESP32_S3_ATLASCUBE_BOARD)',
         'message(STATUS "Current board name is " CONFIG_ESP32_S3_ATLASCUBE_BOARD)',
         'list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_s3_atlascube)',
         'set(COMPONENT_SRCS',
         './esp32_s3_atlascube/board.c',
         './esp32_s3_atlascube/board_pins_config.c',
         ')',
         'endif()',
         ''],
        sentinel="CONFIG_ESP32_S3_ATLASCUBE_BOARD",
    )
    cmake.write_text(text, encoding="utf-8")
    print("    " + ("patched" if changed else "already patched"))

    comp_mk = audio_board / "component.mk"
    if comp_mk.is_file():
        step("Patching component.mk (legacy GNU Make)")
        mk = comp_mk.read_text(encoding="utf-8")
        if "CONFIG_ESP32_S3_ATLASCUBE_BOARD" in mk:
            print("    already patched")
        else:
            mk += ("\nifdef CONFIG_ESP32_S3_ATLASCUBE_BOARD\n"
                   "COMPONENT_ADD_INCLUDEDIRS += ./esp32_s3_atlascube\n"
                   "COMPONENT_SRCDIRS += ./esp32_s3_atlascube\n"
                   "endif\n")
            comp_mk.write_text(mk, encoding="utf-8")
            print("    patched")

    step("ESP-IDF FreeRTOS patch")
    patch = adf / "idf_patches" / "idf_v5.5_freertos.patch"
    if not patch.is_file():
        warn(f"FreeRTOS patch not found at {patch} — skipping")
        return
    apply_args = ["-C", str(idf), "apply", "--ignore-whitespace", str(patch)]
    if git_ok(["-C", str(idf), "apply", "--reverse", "--check", "--ignore-whitespace", str(patch)]):
        print("    already applied")
    elif git_ok(["-C", str(idf), "apply", "--check", "--ignore-whitespace", str(patch)]):
        run(["git", *apply_args])
        print("    applied")
    else:
        die(f"FreeRTOS patch neither applicable nor already-applied — inspect {idf} manually.")


# ── 4-6. assets + build + merge ─────────────────────────────────────────────--

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
    ap = argparse.ArgumentParser(description="Turnkey AtlasCube firmware build.")
    ap.add_argument("variant", nargs="?", choices=list(VARIANTS),
                    help="hardware variant (omit for an interactive menu)")
    ap.add_argument("--adf-path", help="path to esp-adf checkout (default: $ADF_PATH or ./esp-adf, cloned if absent)")
    ap.add_argument("--skip-build", action="store_true", help="set up variant + patches + assets, but don't compile")
    ap.add_argument("--no-clean", action="store_true", help="skip the set-target/clean reconfigure before build (faster, dev only; assumes target already esp32s3)")
    args = ap.parse_args()

    if sys.stdin.isatty():
        print("NOTE: build.py is the release/CI build (switches HW variant, builds a\n"
              "      distributable image). To just build & flash your own board, use\n"
              "      scripts/build-flash.py.\n", file=sys.stderr, flush=True)

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
