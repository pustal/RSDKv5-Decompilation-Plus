#!/usr/bin/env python3
"""Draws the macOS app icon of RSDKv5-Decompilation-Plus from the README header.

The header (header.png) has RSDK's tiled "R" and the green "PLUS": both are
lifted from it (their antialiased edges un-blended from the black
background) and set on a dark macOS-style rounded square, the R above PLUS.
Writes the AppIcon set of an Xcode asset catalog: every size macOS asks for,
and its Contents.json.

Usage (from the repository's root; needs Pillow and NumPy):
    python3 dependencies/mac/make-icon.py [header.png] [AppIcon.appiconset folder]
which by default reads header.png and writes
dependencies/mac/Assets.xcassets/AppIcon.appiconset.
"""
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

CANVAS = 1024
# Apple's macOS icon grid: an 824-point rounded square, centred, radius ~185.
PLATE = 824
PLATE_RADIUS = 185
TOP_COLOUR = (40, 40, 42)     # the plate's gradient, top to bottom
BOTTOM_COLOUR = (18, 18, 19)

# Where the two marks sit in header.png (pixels, inclusive), measured once.
R_BOX = (107, 106, 522, 627)
PLUS_BOX = (3338, 110, 4353, 410)

R_HEIGHT = 450   # the R's height on the 1024 canvas
PLUS_WIDTH = 540 # PLUS's width
GAP = 46         # between them

# The sizes (points, scales) of a macOS AppIcon set.
SIZES = [(16, 1), (16, 2), (32, 1), (32, 2), (128, 1), (128, 2), (256, 1), (256, 2), (512, 1), (512, 2)]


def lift(header: np.ndarray, box: tuple) -> Image.Image:
    """The mark in @p box as RGBA: the black background removed.

    Each pixel's opacity is its brightness relative to the brightest pixel
    nearby (the solid colour of the tile it belongs to), so the antialiased
    edges of white, red, green and blue tiles alike keep their colour.
    """
    x0, y0, x1, y1 = box
    crop = header[y0 - 4:y1 + 5, x0 - 4:x1 + 5].astype(np.float64)
    brightness = crop.max(axis=2)
    local = np.array(Image.fromarray(brightness.astype(np.uint8)).filter(ImageFilter.MaxFilter(15))).astype(np.float64)
    alpha = np.clip(brightness / np.maximum(local, 1.0), 0.0, 1.0)
    alpha[brightness < 12] = 0.0  # JPEG-like noise in the black
    colour = np.clip(crop / np.maximum(alpha[..., None], 1e-6), 0, 255)
    rgba = np.dstack([colour, alpha * 255]).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA")


def plate() -> Image.Image:
    """The dark rounded square, with a vertical gradient and a faint rim."""
    gradient = np.linspace(0.0, 1.0, PLATE)[:, None, None]
    top, bottom = np.array(TOP_COLOUR, float), np.array(BOTTOM_COLOUR, float)
    fill = (top + (bottom - top) * gradient) * np.ones((PLATE, PLATE, 1))
    square = Image.fromarray(fill.astype(np.uint8), "RGB").convert("RGBA")
    mask = Image.new("L", (PLATE, PLATE), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, PLATE - 1, PLATE - 1), PLATE_RADIUS, fill=255)
    square.putalpha(mask)
    rim = Image.new("RGBA", (PLATE, PLATE), (0, 0, 0, 0))
    ImageDraw.Draw(rim).rounded_rectangle((1, 1, PLATE - 2, PLATE - 2), PLATE_RADIUS, outline=(255, 255, 255, 28), width=3)
    return Image.alpha_composite(square, rim)


def icon(header_path: str) -> Image.Image:
    """The 1024-pixel icon."""
    header = np.array(Image.open(header_path).convert("RGB"))
    r = lift(header, R_BOX)
    r = r.resize((round(r.width * R_HEIGHT / r.height), R_HEIGHT), Image.LANCZOS)
    plus = lift(header, PLUS_BOX)
    plus = plus.resize((PLUS_WIDTH, round(plus.height * PLUS_WIDTH / plus.width)), Image.LANCZOS)

    canvas = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    # A soft shadow under the plate, as macOS icons have.
    shadow = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    offset = (CANVAS - PLATE) // 2
    ImageDraw.Draw(shadow).rounded_rectangle((offset, offset + 12, offset + PLATE, offset + PLATE + 12), PLATE_RADIUS, fill=(0, 0, 0, 110))
    canvas = Image.alpha_composite(canvas, shadow.filter(ImageFilter.GaussianBlur(14)))
    canvas.alpha_composite(plate(), (offset, offset))

    total = r.height + GAP + plus.height
    top = (CANVAS - total) // 2
    canvas.alpha_composite(r, ((CANVAS - r.width) // 2, top))
    canvas.alpha_composite(plus, ((CANVAS - plus.width) // 2, top + r.height + GAP))
    return canvas


def main() -> None:
    root = Path(__file__).resolve().parent.parent.parent
    header_path = sys.argv[1] if len(sys.argv) > 1 else str(root / "header.png")
    out_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else root / "dependencies" / "mac" / "Assets.xcassets" / "AppIcon.appiconset"
    out_dir.mkdir(parents=True, exist_ok=True)
    full = icon(header_path)
    images = []
    for points, scale in SIZES:
        pixels = points * scale
        name = f"icon_{points}x{points}{'@2x' if scale == 2 else ''}.png"
        full.resize((pixels, pixels), Image.LANCZOS).save(out_dir / name, optimize=True)
        images.append({"idiom": "mac", "size": f"{points}x{points}", "scale": f"{scale}x", "filename": name})
    contents = {"images": images, "info": {"author": "xcode", "version": 1}}
    (out_dir / "Contents.json").write_text(json.dumps(contents, indent=2) + "\n")
    print(f"wrote {len(images)} sizes to {out_dir}")


if __name__ == "__main__":
    main()
