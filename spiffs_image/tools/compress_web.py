#!/usr/bin/env python3
"""
Compresses www files to spiffs_image/web/*.gz
Run once before idf.py build when you change www files.
Usage:
    python tools/compress_web.py

The web UI lives in spiffs_image/www/. Compressible assets (html/css/js/svg/ico)
are gzip'd into spiffs_image/web/*.gz; any files the firmware reads with a plain
fopen (COPY_EXTENSIONS) are copied verbatim so they stay readable. User data
(settings/theme/events/mqtt JSON plus the default station list playlist.csv) lives
in spiffs_image/config/ and ships to the separate `config` partition — not touched
here — so a www re-upload/re-flash can't clobber it.
"""
import gzip
import hashlib
import re
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT  = Path(__file__).parent.parent.parent
SRC_DIR    = Path(__file__).parent.parent / "www"
DST_DIR    = Path(__file__).parent.parent / "web"
EXTENSIONS = {".html", ".css", ".js", ".svg", ".ico"}
# Shipped verbatim (uncompressed): the firmware opens these with a plain fopen,
# so they must not be gzip'd. Served over HTTP straight from the www partition.
COPY_EXTENSIONS = {".csv"}

# www version stamp — lets the firmware detect a www partition left stale by an
# app-only OTA (OTA never rewrites www). Lands on the partition as
# /spiffs/www_version.txt; the same hash is baked into the app via the generated
# header below, and the two are compared at runtime (see http_server.c).
#
# Written into BOTH www/ and web/: the www/ copy ships next to the source files,
# so re-uploading the web UI through the setup page (which uploads raw www/ files)
# carries the matching version along — the flag clears only when the *whole* new
# set is uploaded, and correctly stays set if old files are re-uploaded. It is NOT
# hashed (.txt is outside EXTENSIONS), so there's no circular dependency.
VERSION_NAME = "www_version.txt"
SRC_VERSION  = SRC_DIR / VERSION_NAME
DST_VERSION  = DST_DIR / VERSION_NAME
HEADER_FILE  = REPO_ROOT / "components" / "web" / "web_assets_version.h"

# Cache-busting: JS/CSS are served with a long max-age (see http_server.c) so the
# browser doesn't re-fetch every asset on each page open — important while radio is
# streaming. The downside is a stale cached script/style after a UI update. To bust
# it we stamp ?v=<www-version> onto every LOCAL .js/.css reference in the HTML.
# Because HTML itself is served no-cache, the browser always sees the fresh tag and
# a changed ?v= forces a re-fetch; unchanged files keep their long cache.
#
# The stamp is injected in memory into the web/*.gz output ONLY — the www/*.html
# source is never rewritten. web/ is gitignored, so the served files carry the
# fresh ?v= while git stays clean (no per-build churn of every HTML file). The
# trade-off: a raw www/ re-upload through /setup won't carry ?v=, which only costs
# browser cache-busting on that niche path (staleness detection uses www_version.txt).
#
# VER_STRIP removes a previously injected ?v=<12 hex> so stamping is idempotent and
# so the query never feeds back into the version hash. VER_INJECT matches a local
# asset ref (not http(s):// or protocol-relative //) whose value ends in .js/.css.
VER_STRIP  = re.compile(rb"\?v=[0-9a-f]{12}")
VER_INJECT = re.compile(rb'((?:src|href)=")(?!https?://|//)([^"]+?\.(?:js|css))(")')

def stamp_html_assets(data: bytes, ver: bytes) -> bytes:
    stripped = VER_STRIP.sub(b"", data)
    return VER_INJECT.sub(lambda m: m.group(1) + m.group(2) + b"?v=" + ver + m.group(3), stripped)

def compress_file(src: Path, version: str):
    rel = src.relative_to(SRC_DIR)
    dst = DST_DIR / (str(rel) + ".gz")
    dst.parent.mkdir(parents=True, exist_ok=True)
    # HTML is always regenerated: its ?v= stamp depends on the whole-UI hash, so a
    # change in any .js/.css must re-stamp every HTML even though its own mtime is
    # untouched. The gz copies are tiny, so skipping the mtime shortcut is free.
    is_html = src.suffix == ".html"
    if not is_html and dst.exists() and dst.stat().st_mtime >= src.stat().st_mtime:
        print(f"  skip  {rel}  (up to date)")
        return
    with open(src, "rb") as f_in:
        data = f_in.read()
    if is_html:
        data = stamp_html_assets(data, version.encode())
    with gzip.open(dst, "wb", compresslevel=9) as f_out:
        f_out.write(data)
    orig_kb = len(data) / 1024
    gz_kb   = dst.stat().st_size / 1024
    ratio   = (1 - gz_kb / orig_kb) * 100 if orig_kb > 0 else 0
    print(f"  gz    {str(rel):<30} {orig_kb:6.1f} KB -> {gz_kb:5.1f} KB  ({ratio:.0f}% smaller)")

def copy_file(src: Path):
    rel = src.relative_to(SRC_DIR)
    dst = DST_DIR / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists() and dst.stat().st_mtime >= src.stat().st_mtime:
        print(f"  skip  {rel}  (up to date)")
        return
    shutil.copy2(src, dst)
    print(f"  copy  {str(rel):<30} {src.stat().st_size / 1024:6.1f} KB  (verbatim)")


def prune_stale(expected: set):
    """Remove anything in web/ that no longer maps to a www/ source. Without this,
    files deleted/moved out of www/ (e.g. the config JSON now on /config) would
    linger in the www partition image and shadow the live copy at serve time."""
    if not DST_DIR.exists():
        return
    for path in sorted(DST_DIR.rglob("*"), reverse=True):
        if path.is_file() and path not in expected:
            path.unlink()
            print(f"  prune {path.relative_to(DST_DIR)}  (no source)")
        elif path.is_dir() and not any(path.iterdir()):
            path.rmdir()

def compute_version() -> str:
    """Short hash of the UI source assets (uncompressed). Identifies which build
    the www partition came from, independent of gzip output (the on-device file
    editor re-gzips uploads with a different zlib, so hashing the .gz would drift).
    Only EXTENSIONS are hashed: any verbatim/user-editable data (COPY_EXTENSIONS)
    is excluded so editing it never flags the web UI as stale."""
    h = hashlib.sha256()
    for path in sorted(SRC_DIR.rglob("*")):
        if not path.is_file() or path.suffix not in EXTENSIONS:
            continue
        h.update(path.relative_to(SRC_DIR).as_posix().encode())
        h.update(b"\0")
        # Normalise CRLF -> LF so the hash matches on Windows (autocrlf checkout)
        # and Linux/CI, which would otherwise hash different bytes for the same
        # source and flag a spurious www mismatch after an OTA.
        data = path.read_bytes().replace(b"\r\n", b"\n")
        # Strip our own cache-busting ?v=<hash> from HTML before hashing, otherwise
        # injecting the hash would change the HTML and thus the next hash forever.
        if path.suffix == ".html":
            data = VER_STRIP.sub(b"", data)
        h.update(data)
    return h.hexdigest()[:12]


def fw_version() -> str:
    """Firmware version from `git describe` — the same source ESP-IDF uses for the
    app descriptor (PROJECT_VER_FROM_GIT). Purely informational metadata in the
    stamp; '?' when git isn't available (the build still works on the hash alone)."""
    try:
        out = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "describe", "--tags", "--dirty", "--always"],
            capture_output=True, text=True, timeout=5)
        return out.stdout.strip() if out.returncode == 0 and out.stdout.strip() else "?"
    except Exception:
        return "?"


def write_version(ver: str):
    # Stamp = "<hash> <fw-version> <UTC-date>". The firmware compares ONLY the first
    # token (the hash) for staleness; the rest is human-readable context so the
    # web UI warning can show which side is older/newer at a glance. Keep the hash
    # first and space-separated — http_server.c splits on the first whitespace.
    stamp = f"{ver} {fw_version()} {datetime.now(timezone.utc):%Y-%m-%d %H:%M:%S} UTC"
    DST_DIR.mkdir(parents=True, exist_ok=True)
    SRC_VERSION.write_text(stamp, encoding="utf-8")
    DST_VERSION.write_text(stamp, encoding="utf-8")
    HEADER_FILE.write_text(
        "// Generated by spiffs_image/tools/compress_web.py - do not edit.\n"
        "// Stamp of the bundled web UI: \"<hash> <fw-version> <date>\". Only the\n"
        "// leading hash is compared at runtime against /spiffs/www_version.txt to\n"
        "// flag a www partition left stale by an app-only OTA update; the rest is\n"
        "// shown in the UI to tell which side is older.\n"
        "#pragma once\n"
        f'#define WEB_ASSETS_VERSION "{stamp}"\n',
        encoding="utf-8")
    print(f"  ver   www_version.txt / WEB_ASSETS_VERSION = {stamp}")


def main():
    print(f"Compressing web assets from {SRC_DIR}/\n")

    # Version first: it's independent of the ?v= we stamp into the gz output (the
    # hash normalises it away), and compress_file needs it to stamp each HTML.
    version = compute_version()

    expected = set()
    for path in sorted(SRC_DIR.rglob("*")):
        if not path.is_file() or ".gz" in path.suffixes:
            continue
        if path.suffix in EXTENSIONS:
            compress_file(path, version)
            expected.add(DST_DIR / (str(path.relative_to(SRC_DIR)) + ".gz"))
        elif path.suffix in COPY_EXTENSIONS:
            copy_file(path)
            expected.add(DST_DIR / path.relative_to(SRC_DIR))
    write_version(version)
    expected.add(DST_VERSION)
    prune_stale(expected)
    print("\nDone. Flash with: idf.py build flash")

if __name__ == "__main__":
    main()
