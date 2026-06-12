#!/usr/bin/env python3
"""Convert JPG/PNG images into LVGL v9 binary images (.bin) for AtlasCube.

This is the REFERENCE implementation of the slide format. The Android app must
produce byte-identical files; keep the two in sync.

Output layout (matches LVGL 9.2 `lv_image_header_t`, little-endian):

    offset  size  field        value
    0       1     magic        0x19   (LV_IMAGE_HEADER_MAGIC)
    1       1     cf           0x12   (LV_COLOR_FORMAT_RGB565)
    2       2     flags        0
    4       2     w            width in pixels
    6       2     h            height in pixels
    8       2     stride       width * 2  (bytes per row, no padding)
    10      2     reserved_2   0
    12      ...   pixels       RGB565, little-endian (low byte first),
                               row-major, top-to-bottom

The firmware renders RGB565 little-endian internally and byte-swaps to the panel
in the flush callback, so the pixel order here is panel-agnostic — only width,
height and rotation depend on the target display.

Usage:
    python scripts/img2lvgl.py photo.jpg                 # → photo.bin (480x320, crop)
    python scripts/img2lvgl.py *.jpg --outdir slides     # batch into ./slides
    python scripts/img2lvgl.py p.png -W 240 -H 296       # CO5300 panel size
    python scripts/img2lvgl.py p.png --fit contain --bg 000000
    python scripts/img2lvgl.py p.png --rotate 90         # clockwise

Requires: pillow, numpy  (pip install pillow numpy)
"""

import argparse
import struct
import sys
from pathlib import Path

try:
    from PIL import Image, ImageOps
except ImportError:
    sys.exit("error: Pillow is required — `pip install pillow`")

try:
    import numpy as np
except ImportError:
    sys.exit("error: numpy is required — `pip install numpy`")

LV_IMAGE_HEADER_MAGIC = 0x19
LV_COLOR_FORMAT_RGB565 = 0x12

try:
    RESAMPLE = Image.Resampling.LANCZOS
except AttributeError:  # Pillow < 9.1
    RESAMPLE = Image.LANCZOS

# Clockwise rotation → PIL transpose op (PIL's own rotations are counter-clockwise).
ROTATE_OPS = {
    0:   None,
    90:  Image.Transpose.ROTATE_270,
    180: Image.Transpose.ROTATE_180,
    270: Image.Transpose.ROTATE_90,
}


def to_rgb565_bytes(img: Image.Image) -> bytes:
    """Pack an RGB image into little-endian RGB565 pixel bytes (row-major)."""
    arr = np.asarray(img.convert("RGB"), dtype=np.uint16)  # H x W x 3
    r = (arr[:, :, 0] >> 3) & 0x1F
    g = (arr[:, :, 1] >> 2) & 0x3F
    b = (arr[:, :, 2] >> 3) & 0x1F
    rgb565 = (r << 11) | (g << 5) | b
    return rgb565.astype("<u2").tobytes()  # '<u2' = little-endian uint16


def build_bin(img: Image.Image, w: int, h: int, fit: str, bg: tuple, rotate: int) -> bytes:
    """Rotate, fit to w×h, then serialize to an LVGL v9 RGB565 .bin."""
    op = ROTATE_OPS[rotate]
    if op is not None:
        img = img.transpose(op)

    if fit == "crop":
        # Scale to cover the panel, then center-crop the overflow.
        img = ImageOps.fit(img, (w, h), method=RESAMPLE, centering=(0.5, 0.5))
    else:  # contain — scale to fit, letterbox the rest with bg
        fitted = ImageOps.contain(img, (w, h), method=RESAMPLE)
        canvas = Image.new("RGB", (w, h), bg)
        canvas.paste(fitted, ((w - fitted.width) // 2, (h - fitted.height) // 2))
        img = canvas

    header = struct.pack(
        "<BBHHHHH",
        LV_IMAGE_HEADER_MAGIC,
        LV_COLOR_FORMAT_RGB565,
        0,          # flags
        w,
        h,
        w * 2,      # stride
        0,          # reserved_2
    )
    return header + to_rgb565_bytes(img)


def parse_bg(s: str) -> tuple:
    s = s.lstrip("#")
    if len(s) != 6:
        raise argparse.ArgumentTypeError("background must be a 6-digit hex color, e.g. 000000")
    return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4))


def main():
    ap = argparse.ArgumentParser(description="Convert images to LVGL v9 RGB565 .bin slides.")
    ap.add_argument("inputs", nargs="+", help="source image(s) (jpg/png/...)")
    ap.add_argument("-W", "--width", type=int, default=480, help="panel width (default 480)")
    ap.add_argument("-H", "--height", type=int, default=320, help="panel height (default 320)")
    ap.add_argument("--fit", choices=["crop", "contain"], default="crop",
                    help="crop = fill panel (default); contain = letterbox")
    ap.add_argument("--bg", type=parse_bg, default=(0, 0, 0),
                    help="letterbox background hex color (default 000000)")
    ap.add_argument("--rotate", type=int, choices=[0, 90, 180, 270], default=0,
                    help="clockwise rotation applied before fitting")
    ap.add_argument("-o", "--out", help="output file (only with a single input)")
    ap.add_argument("--outdir", default=".", help="output directory (default: cwd)")
    args = ap.parse_args()

    if args.out and len(args.inputs) > 1:
        ap.error("--out works with a single input; use --outdir for batches")

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    for src in args.inputs:
        src_path = Path(src)
        try:
            img = Image.open(src_path)
        except Exception as e:
            print(f"skip {src}: {e}", file=sys.stderr)
            continue

        data = build_bin(img, args.width, args.height, args.fit, args.bg, args.rotate)

        dst = Path(args.out) if args.out else outdir / (src_path.stem + ".bin")
        dst.write_bytes(data)
        print(f"{src}  ->  {dst}  ({args.width}x{args.height}, {args.fit}, {len(data)} bytes)")


if __name__ == "__main__":
    main()
