#!/usr/bin/env python3
"""
AtlasCube toolchain setup — shared helpers, NOT a script to run.

This module is imported by both scripts/build-flash.py (the end-user build/flash
script) and ci/build.py (the CI/release builder). It locates ESP-IDF / ESP-ADF,
clones ESP-ADF if missing, and patches ESP-ADF + ESP-IDF so a plain `idf.py build`
works. Keeping it here (in scripts/, next to the user script) means build-flash.py
never has to import anything from ci/.

Pinned toolchain: ESP-IDF v5.5.4, ESP-ADF v2.8.
"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# Repo root, resolved from this module's location (scripts/env_setup.py -> repo).
# Both build-flash.py and ci/build.py use THIS constant, so ci/build.py's own
# location doesn't matter for path resolution.
REPO_ROOT = Path(__file__).resolve().parent.parent
BOARD_NAME = "esp32_s3_atlascube"

IDF_VERSION = "v5.5.4"
ADF_VERSION = "v2.8"
ADF_REPO = "https://github.com/espressif/esp-adf.git"
TARGET = "esp32s3"
EIM_URL = "https://github.com/espressif/idf-im-ui/releases/latest"


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


# ── 2. patch ESP-ADF + ESP-IDF (replaces patch-esp-adf.sh) ──────────────────────

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
    # ci/build.py and the VSCode ESP-IDF extension (idf.py build) — don't clobber it.
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
