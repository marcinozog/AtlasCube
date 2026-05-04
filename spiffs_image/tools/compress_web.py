#!/usr/bin/env python3
"""
Compresses www files to spiffs_image/web/*.gz
Run once before idf.py build when you change www files.
Usage:
    python tools/compress_web.py
"""
import os
import gzip
import shutil
from pathlib import Path

SRC_DIR    = Path(__file__).parent.parent / "www"
DST_DIR    = Path(__file__).parent.parent / "web"
EXTENSIONS = {".html", ".css", ".js", ".svg", ".ico"}
COPY_FILES = {"data/playlist.csv", "settings.json", "theme.json", "events.json"}

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
    size_kb = src.stat().st_size / 1024
    print(f"  copy  {str(rel):<30} {size_kb:6.1f} KB")

def main():
    print(f"Compressing web assets from {SRC_DIR}/\n")
    for path in sorted(SRC_DIR.rglob("*")):
        if not path.is_file() or ".gz" in path.suffixes:
            continue
        rel_posix = path.relative_to(SRC_DIR).as_posix()
        if rel_posix in COPY_FILES:
            copy_file(path)
        elif path.suffix in EXTENSIONS:
            compress_file(path)
    print("\nDone. Flash with: idf.py build flash")

if __name__ == "__main__":
    main()