#!/usr/bin/env python3
"""
Compresses www files to spiffs_image/web/*.gz
Run once before idf.py build when you change www files.
Usage:
    python tools/compress_web.py

The web UI and the default station list (playlist.csv) live in spiffs_image/www/.
Compressible assets (html/css/js/svg/ico) are gzip'd into spiffs_image/web/*.gz;
files that the firmware reads with a plain fopen (playlist.csv) are copied verbatim
so they stay readable. User settings (settings/theme/events/mqtt JSON) live in
spiffs_image/config/ and ship to the separate `config` partition — not touched here.
"""
import gzip
import shutil
from pathlib import Path

SRC_DIR    = Path(__file__).parent.parent / "www"
DST_DIR    = Path(__file__).parent.parent / "web"
EXTENSIONS = {".html", ".css", ".js", ".svg", ".ico"}
# Shipped verbatim (uncompressed): the firmware opens these with a plain fopen,
# so they must not be gzip'd. Served over HTTP straight from the www partition.
COPY_EXTENSIONS = {".csv"}

def compress_file(src: Path):
    rel = src.relative_to(SRC_DIR)
    dst = DST_DIR / (str(rel) + ".gz")
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists() and dst.stat().st_mtime >= src.stat().st_mtime:
        print(f"  skip  {rel}  (up to date)")
        return
    with open(src, "rb") as f_in:
        data = f_in.read()
    with gzip.open(dst, "wb", compresslevel=9) as f_out:
        f_out.write(data)
    orig_kb = len(data) / 1024
    gz_kb   = dst.stat().st_size / 1024
    ratio   = (1 - gz_kb / orig_kb) * 100 if orig_kb > 0 else 0
    print(f"  gz    {str(rel):<30} {orig_kb:6.1f} KB → {gz_kb:5.1f} KB  ({ratio:.0f}% smaller)")

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

def main():
    print(f"Compressing web assets from {SRC_DIR}/\n")
    expected = set()
    for path in sorted(SRC_DIR.rglob("*")):
        if not path.is_file() or ".gz" in path.suffixes:
            continue
        if path.suffix in EXTENSIONS:
            compress_file(path)
            expected.add(DST_DIR / (str(path.relative_to(SRC_DIR)) + ".gz"))
        elif path.suffix in COPY_EXTENSIONS:
            copy_file(path)
            expected.add(DST_DIR / path.relative_to(SRC_DIR))
    prune_stale(expected)
    print("\nDone. Flash with: idf.py build flash")

if __name__ == "__main__":
    main()
