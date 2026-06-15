#!/usr/bin/env python3
"""
Compresses www files to spiffs_image/web/*.gz
Run once before idf.py build when you change www files.
Usage:
    python tools/compress_web.py

Only the web UI lives in spiffs_image/www/. User settings (settings/theme/events/
mqtt JSON + playlist CSV) live in spiffs_image/config/ and ship to the separate
`config` partition uncompressed — they are not touched here.
"""
import gzip
from pathlib import Path

SRC_DIR    = Path(__file__).parent.parent / "www"
DST_DIR    = Path(__file__).parent.parent / "web"
EXTENSIONS = {".html", ".css", ".js", ".svg", ".ico"}

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
    prune_stale(expected)
    print("\nDone. Flash with: idf.py build flash")

if __name__ == "__main__":
    main()
