#!/usr/bin/env python3
"""
Flash firmware WITH the SPIFFS web UI bundled — quick dev loop.

Use this after changing files in spiffs_image/www/. It bundles the compressed
web UI (ATLAS_SPIFFS=1), flashes the device, then resets the project back to the
fast (no-SPIFFS) config so the VSCode extension's build/flash buttons stay quick.

Run inside an ESP-IDF environment (the "ESP-IDF PowerShell", or after export.ps1):
    python scripts/flash-web.py                  # build + flash (auto-detected port)
    python scripts/flash-web.py -p COM5 flash    # explicit port
    python scripts/flash-web.py -p COM5 flash monitor

Any arguments are forwarded verbatim to idf.py; with none, it defaults to `flash`.
"""

import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def die(msg):
    print(f"ERROR: {msg}", file=sys.stderr, flush=True)
    sys.exit(1)


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


def main():
    idf_py = resolve_idf_py()
    idf = lambda *a: [sys.executable, str(idf_py), *a]
    idf_args = sys.argv[1:] or ["flash"]

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
