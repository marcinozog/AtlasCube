#!/usr/bin/env python3
"""
Flash firmware WITH the SPIFFS web UI bundled — quick dev loop.

Use this after changing files in spiffs_image/www/. It bundles the compressed
web UI (ATLAS_SPIFFS=1), flashes the device, then resets the project back to the
fast (no-SPIFFS) config so the VSCode extension's build/flash buttons stay quick.

It also guards against a stale sdkconfig: ESP-IDF only applies sdkconfig.defaults
(incl. the auto-generated sdkconfig.variant) when sdkconfig doesn't yet exist, so
switching the HW variant in defines.h leaves the OLD CONFIG_DISPLAY_*/TOUCH_*
baked into sdkconfig. The result is a half-switched binary (e.g. new UI_PROFILE
header but old display driver). Before flashing we compare defines.h against
sdkconfig and, on a mismatch, delete sdkconfig so the next reconfigure rebuilds
it cleanly (prompted; auto when non-interactive; forced with --clean).

Run inside an ESP-IDF environment (the "ESP-IDF PowerShell", or after export.ps1):
    python scripts/flash-web.py                  # build + flash (auto-detected port)
    python scripts/flash-web.py -p COM5 flash    # explicit port
    python scripts/flash-web.py -p COM5 flash monitor
    python scripts/flash-web.py --clean flash    # force a clean sdkconfig first

Any arguments (besides --clean) are forwarded verbatim to idf.py; with none,
it defaults to `flash`.
"""

import os
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFINES_H = REPO_ROOT / "main" / "include" / "defines.h"
SDKCONFIG = REPO_ROOT / "sdkconfig"


def die(msg):
    print(f"ERROR: {msg}", file=sys.stderr, flush=True)
    sys.exit(1)


def warn(msg):
    print(f"WARN: {msg}", file=sys.stderr, flush=True)


def resolve_idf_py():
    idf = os.environ.get("IDF_PATH")
    if not idf or not Path(idf).is_dir():
        die("IDF_PATH is not set — run this from inside an ESP-IDF environment "
            "(open the ESP-IDF terminal, or run its export.ps1 / export.sh first).")
    idf_py = Path(idf) / "tools" / "idf.py"
    if not idf_py.is_file():
        die(f"idf.py not found at {idf_py} — is IDF_PATH correct?")
    return idf_py


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


def main():
    args = sys.argv[1:]
    force_clean = "--clean" in args
    idf_args = [a for a in args if a != "--clean"] or ["flash"]

    idf_py = resolve_idf_py()
    idf = lambda *a: [sys.executable, str(idf_py), *a]

    # Drop a stale sdkconfig before configuring (variant switch guard).
    ensure_fresh_sdkconfig(force_clean)

    # Bundle the web UI: compress assets and flag the SPIFFS build.
    run([sys.executable, str(REPO_ROOT / "spiffs_image" / "tools" / "compress_web.py")])
    os.environ["ATLAS_SPIFFS"] = "1"

    try:
        # Force a reconfigure so CMake picks up ATLAS_SPIFFS=1
        # (env changes alone don't retrigger configure).
        run(idf("reconfigure"))
        run(idf(*idf_args))
    finally:
        # Reset back to the fast (no-SPIFFS) config so extension buttons stay quick.
        os.environ.pop("ATLAS_SPIFFS", None)
        run(idf("reconfigure"))


if __name__ == "__main__":
    main()
