#!/usr/bin/env python3
"""Draws RSDKv5-Decompilation-Plus's app icons, for every platform, from the README header.

The header (header.png) has RSDK's tiled "R" and the green "PLUS": both are
lifted from it (their antialiased edges un-blended from the black
background) and set, the R above PLUS, on a dark plate shaped as each
platform expects:

  macOS    dependencies/mac/Assets.xcassets/AppIcon.appiconset
           Apple's rounded square with a soft shadow, every size an AppIcon
           set has, and its Contents.json.
  Windows  RSDKv5U/RSDKv5U.ico and RSDKv5/RSDKv5.ico (the .rc files use them)
           a rounded square filling the icon, 16 to 256 pixels.
  Switch   RSDKv5U/switch-icon.jpg and RSDKv5/switch-icon.jpg (the NRO's icon)
           a full square, 256 pixels, as JPEG.
  Linux    RSDKv5/RSDK/Graphics/WindowIcon.hpp
           the window's icon (Linux has no icon in the executable): the
           Windows design at 64 pixels, as RGBA bytes the SDL2 and GLFW render
           devices hand to the window when they create it.
  Android  android/app/src/main/res/mipmap-*/ic_launcher*.png
           ic_launcher: a round icon for launchers before Android 8; the
           adaptive icon's layers (mipmap-anydpi-v26/ic_launcher.xml):
           ic_launcher_background (the plate's gradient), ic_launcher_foreground
           (the marks, inside the safe zone launchers never crop) and
           ic_launcher_monochrome (the same in one colour, for themed icons).

Each icon is drawn at 1024 pixels and scaled down, so small sizes stay
smooth. Sonic-Mania-Decompilation-Plus keeps a copy of the Switch icon at its
root, for the same NRO: pass --mania <path to that repository> to update it too.

Usage (from the repository's root; needs Pillow and NumPy):
    python3 tools/make-icons.py [--header header.png] [--mania ../Sonic-Mania-Decompilation-Plus]
"""
import argparse
import io
import json
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

CANVAS = 1024
TOP_COLOUR = (40, 40, 42)  # the plate's gradient, top to bottom
BOTTOM_COLOUR = (18, 18, 19)

# Where the two marks sit in header.png (pixels, inclusive), measured once.
R_BOX = (107, 106, 522, 627)
PLUS_BOX = (3338, 110, 4353, 410)

# The marks' layout, in pixels of a 1024 canvas at full size: the R's height,
# PLUS's width and the gap between them. Each icon scales the whole block.
R_HEIGHT = 450
PLUS_WIDTH = 540
GAP = 46

# macOS: Apple's icon grid, an 824-point rounded square, centred, radius ~185.
MAC_PLATE = 824
MAC_RADIUS = 185
MAC_SIZES = [(16, 1), (16, 2), (32, 1), (32, 2), (128, 1), (128, 2), (256, 1), (256, 2), (512, 1), (512, 2)]

WINDOWS_SIZES = [16, 24, 32, 48, 64, 128, 256]
LINUX_WINDOW_SIZE = 64
SWITCH_SIZE = 256

# Android: launcher icons (48 dp) and adaptive layers (108 dp), per density.
ANDROID_DENSITIES = {"mdpi": 1.0, "hdpi": 1.5, "xhdpi": 2.0, "xxhdpi": 3.0, "xxxhdpi": 4.0}
# Of an adaptive layer's 108 dp, launchers may show as little as a 66 dp circle.
ADAPTIVE_SAFE_CIRCLE = 66 / 108


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


def marks(header_path: Path) -> Image.Image:
    """The R above PLUS on a transparent background, at full size (R_HEIGHT tall)."""
    header = np.array(Image.open(header_path).convert("RGB"))
    r = lift(header, R_BOX)
    r = r.resize((round(r.width * R_HEIGHT / r.height), R_HEIGHT), Image.LANCZOS)
    plus = lift(header, PLUS_BOX)
    plus = plus.resize((PLUS_WIDTH, round(plus.height * PLUS_WIDTH / plus.width)), Image.LANCZOS)
    block = Image.new("RGBA", (max(r.width, plus.width), r.height + GAP + plus.height), (0, 0, 0, 0))
    block.alpha_composite(r, ((block.width - r.width) // 2, 0))
    block.alpha_composite(plus, ((block.width - plus.width) // 2, r.height + GAP))
    return block


def gradient(size: int) -> Image.Image:
    """A size x size square of the plate's vertical gradient, opaque."""
    ramp = np.linspace(0.0, 1.0, size)[:, None, None]
    top, bottom = np.array(TOP_COLOUR, float), np.array(BOTTOM_COLOUR, float)
    fill = (top + (bottom - top) * ramp) * np.ones((size, size, 1))
    return Image.fromarray(fill.astype(np.uint8), "RGB").convert("RGBA")


def plate(size: int, shape: str, radius: int = 0) -> Image.Image:
    """The dark plate: "square" (full bleed), "rounded" (corner @p radius, with a faint rim) or "circle"."""
    square = gradient(size)
    if shape == "square":
        return square
    mask = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask)
    if shape == "circle":
        draw.ellipse((0, 0, size - 1, size - 1), fill=255)
    else:
        draw.rounded_rectangle((0, 0, size - 1, size - 1), radius, fill=255)
    square.putalpha(mask)
    if shape == "rounded":
        rim = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        ImageDraw.Draw(rim).rounded_rectangle((1, 1, size - 2, size - 2), radius, outline=(255, 255, 255, 28), width=3)
        square = Image.alpha_composite(square, rim)
    return square


def place(canvas: Image.Image, block: Image.Image, height: float) -> Image.Image:
    """@p block scaled to @p height (a fraction of the canvas), centred on @p canvas."""
    pixels = round(canvas.height * height)
    scaled = block.resize((round(block.width * pixels / block.height), pixels), Image.LANCZOS)
    canvas.alpha_composite(scaled, ((canvas.width - scaled.width) // 2, (canvas.height - scaled.height) // 2))
    return canvas


def fit_circle(block: Image.Image, diameter: float) -> float:
    """The block's height (fraction of the canvas) for its corners to touch a circle of @p diameter."""
    diagonal = (block.width ** 2 + block.height ** 2) ** 0.5
    return diameter * block.height / diagonal


def mac_icon(block: Image.Image) -> Image.Image:
    """macOS: the rounded square on Apple's grid, with its shadow."""
    canvas = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    offset = (CANVAS - MAC_PLATE) // 2
    shadow = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle((offset, offset + 12, offset + MAC_PLATE, offset + MAC_PLATE + 12), MAC_RADIUS,
                                             fill=(0, 0, 0, 110))
    canvas = Image.alpha_composite(canvas, shadow.filter(ImageFilter.GaussianBlur(14)))
    canvas.alpha_composite(plate(MAC_PLATE, "rounded", MAC_RADIUS), (offset, offset))
    # The same proportions as the marks on the full canvas: R_HEIGHT + GAP + PLUS on 1024.
    return place(canvas, block, block.height / CANVAS)


def windows_icon(block: Image.Image) -> Image.Image:
    """Windows: a rounded square filling the icon."""
    canvas = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    margin = 24
    canvas.alpha_composite(plate(CANVAS - 2 * margin, "rounded", 170), (margin, margin))
    return place(canvas, block, 0.74)


def switch_icon(block: Image.Image) -> Image.Image:
    """Switch: a full square (the Homebrew Menu shows it as it is)."""
    return place(plate(CANVAS, "square"), block, 0.74).convert("RGB")


def android_legacy(block: Image.Image) -> Image.Image:
    """Android before 8: a round icon."""
    return place(plate(CANVAS, "circle"), block, fit_circle(block, 0.86))


def android_foreground(block: Image.Image) -> Image.Image:
    """The adaptive icon's foreground: the marks inside the safe circle, on transparency."""
    return place(Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0)), block, fit_circle(block, ADAPTIVE_SAFE_CIRCLE * 0.98))


def android_monochrome(foreground: Image.Image) -> Image.Image:
    """The themed icon: the foreground's shape in white (the system tints it)."""
    white = Image.new("RGBA", foreground.size, (255, 255, 255, 0))
    white.putalpha(foreground.getchannel("A"))
    return white


def shrink(image: Image.Image, size: int) -> Image.Image:
    return image.resize((size, size), Image.LANCZOS)


def write_mac(icon: Image.Image, root: Path) -> None:
    out_dir = root / "dependencies" / "mac" / "Assets.xcassets" / "AppIcon.appiconset"
    out_dir.mkdir(parents=True, exist_ok=True)
    images = []
    for points, scale in MAC_SIZES:
        name = f"icon_{points}x{points}{'@2x' if scale == 2 else ''}.png"
        shrink(icon, points * scale).save(out_dir / name, optimize=True)
        images.append({"idiom": "mac", "size": f"{points}x{points}", "scale": f"{scale}x", "filename": name})
    (out_dir / "Contents.json").write_text(json.dumps({"images": images, "info": {"author": "xcode", "version": 1}}, indent=2) + "\n")
    print(f"macOS: {len(images)} sizes in {out_dir.relative_to(root)}")


def ico_bitmap(image: Image.Image) -> bytes:
    """One icon image as Windows stores most of them: a 32-bit DIB (BGRA, bottom-up) and its AND mask."""
    width, height = image.size
    header = struct.pack("<IiiHHIIiiII", 40, width, height * 2, 1, 32, 0, 0, 0, 0, 0, 0)  # height x2: colour + mask
    rgba = np.array(image.convert("RGBA"))
    bgra = rgba[::-1, :, [2, 1, 0, 3]].tobytes()
    mask_row = ((width + 31) // 32) * 4  # 1 bit per pixel, rows padded to 32 bits; all 0: the alpha channel decides
    return header + bgra + bytes(mask_row * height)


def write_ico(path: Path, icon: Image.Image, sizes: list) -> None:
    """An .ico with every size in @p sizes: uncompressed bitmaps, and PNG for 256 pixels.

    Pillow would store every size as PNG, which some parts of Windows (and
    some resource compilers) only read at 256 pixels: Explorer's small views
    and the title bar then fall back to another icon. Bitmaps work everywhere.
    """
    images = []
    for size in sizes:
        scaled = shrink(icon, size)
        if size >= 256:
            buffer = io.BytesIO()
            scaled.save(buffer, format="PNG", optimize=True)
            images.append((size, buffer.getvalue()))
        else:
            images.append((size, ico_bitmap(scaled)))
    out = struct.pack("<HHH", 0, 1, len(images))  # reserved, type 1 (icon), count
    offset = 6 + 16 * len(images)
    for size, data in images:
        side = 0 if size >= 256 else size  # 0 means 256
        out += struct.pack("<BBBBHHII", side, side, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    path.write_bytes(out + b"".join(data for _, data in images))


def write_windows(icon: Image.Image, root: Path) -> None:
    for name in ("RSDKv5U/RSDKv5U.ico", "RSDKv5/RSDKv5.ico"):
        write_ico(root / name, icon, WINDOWS_SIZES)
        print(f"Windows: {name}")


def write_switch(icon: Image.Image, root: Path, mania: Path | None) -> None:
    targets = [root / "RSDKv5U" / "switch-icon.jpg", root / "RSDKv5" / "switch-icon.jpg"]
    if mania:
        targets.append(mania / "switch-icon.jpg")
    for target in targets:
        shrink(icon, SWITCH_SIZE).save(target, quality=95)
        print(f"Switch: {target}")


def write_linux(icon: Image.Image, root: Path) -> None:
    """The window icon as a C++ header: LINUX_WINDOW_SIZE squared RGBA pixels, row by row."""
    size = LINUX_WINDOW_SIZE
    pixels = shrink(icon, size).convert("RGBA").tobytes()
    rows = []
    for start in range(0, len(pixels), 16):
        rows.append("    " + ", ".join(f"0x{b:02X}" for b in pixels[start:start + 16]) + ",")
    target = root / "RSDKv5" / "RSDK" / "Graphics" / "WindowIcon.hpp"
    target.write_text(
        "// =====================\n"
        "// (Plus) The window's icon on Linux\n"
        "// =====================\n"
        "//\n"
        "// Written by tools/make-icons.py from header.png: do not edit by hand.\n"
        "// Linux executables carry no icon, so the SDL2 and GLFW render devices set\n"
        "// this one on the window they create (Windows uses the .ico in the .rc\n"
        "// file, macOS the app bundle's).\n"
        "\n"
        "#ifndef RSDK_WINDOW_ICON_H\n"
        "#define RSDK_WINDOW_ICON_H\n"
        "\n"
        "namespace RSDK\n"
        "{\n"
        "\n"
        f"/** Width and height of kWindowIconPixels. */\n"
        f"constexpr int kWindowIconSize = {size};\n"
        "\n"
        "/** kWindowIconSize x kWindowIconSize pixels, row by row, 4 bytes each (R, G, B, A). */\n"
        f"const unsigned char kWindowIconPixels[{size} * {size} * 4] = {{\n"
        + "\n".join(rows) + "\n"
        "};\n"
        "\n"
        "} // namespace RSDK\n"
        "\n"
        "#endif // RSDK_WINDOW_ICON_H\n")
    print(f"Linux: {target.relative_to(root)}")


def write_android(block: Image.Image, root: Path) -> None:
    res = root / "android" / "app" / "src" / "main" / "res"
    legacy = android_legacy(block)
    foreground = android_foreground(block)
    layers = {
        "ic_launcher_background": plate(CANVAS, "square"),
        "ic_launcher_foreground": foreground,
        "ic_launcher_monochrome": android_monochrome(foreground),
    }
    for density, scale in ANDROID_DENSITIES.items():
        folder = res / f"mipmap-{density}"
        folder.mkdir(parents=True, exist_ok=True)
        shrink(legacy, round(48 * scale)).save(folder / "ic_launcher.png", optimize=True)
        for name, layer in layers.items():
            shrink(layer, round(108 * scale)).save(folder / f"{name}.png", optimize=True)
    # The colour launchers use behind the icon while the layers load.
    (res / "values" / "ic_launcher_background.xml").write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n'
        f'    <color name="ic_launcher_background">#{BOTTOM_COLOUR[0]:02X}{BOTTOM_COLOUR[1]:02X}{BOTTOM_COLOUR[2]:02X}</color>\n'
        "</resources>\n")
    print(f"Android: {len(ANDROID_DENSITIES)} densities in {res.relative_to(root)}")


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--header", type=Path, default=root / "header.png", help="the README header (default: header.png)")
    parser.add_argument("--mania", type=Path, help="Sonic-Mania-Decompilation-Plus, to update its switch-icon.jpg too")
    args = parser.parse_args()

    block = marks(args.header)
    write_mac(mac_icon(block), root)
    windows = windows_icon(block)
    write_windows(windows, root)
    write_linux(windows, root)
    write_switch(switch_icon(block), root, args.mania)
    write_android(block, root)


if __name__ == "__main__":
    main()
