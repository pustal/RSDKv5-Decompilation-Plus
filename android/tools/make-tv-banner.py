#!/usr/bin/env python3
"""Draws the Android TV banner (res/drawable-xhdpi/banner.png, 320x180) from the
launcher icon's foreground: the icon (with its green "PLUS") on the left, the app's name on the right,
on the icon's background colour.

Usage: python3 android/tools/make-tv-banner.py
"""
import pathlib

from PIL import Image, ImageDraw, ImageFont

RES = pathlib.Path(__file__).resolve().parent.parent / "app" / "src" / "main" / "res"
WIDTH, HEIGHT = 320, 180
BACKGROUND = (0x12, 0x12, 0x13, 255)   # values/ic_launcher_background.xml
WHITE = (255, 255, 255, 255)
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


def main() -> None:
    banner = Image.new("RGBA", (WIDTH, HEIGHT), BACKGROUND)
    # The adaptive icon's foreground keeps its artwork in the middle 66%.
    foreground = Image.open(RES / "mipmap-xxxhdpi" / "ic_launcher_foreground.png").convert("RGBA")
    side = foreground.width
    inset = int(side * 0.17)
    art = foreground.crop((inset, inset, side - inset, side - inset)).resize((132, 132), Image.LANCZOS)
    banner.alpha_composite(art, (16, (HEIGHT - 132) // 2))

    # The app's name (strings.xml app_name), centred in the space right of the icon.
    draw = ImageDraw.Draw(banner)
    name = ImageFont.truetype(FONT, 34)
    left, right = 160, WIDTH - 12
    box = draw.textbbox((0, 0), "RSDKv5", font=name)
    width, height = box[2] - box[0], box[3] - box[1]
    draw.text((left + (right - left - width) // 2 - box[0], (HEIGHT - height) // 2 - box[1]), "RSDKv5", font=name, fill=WHITE)

    out = RES / "drawable-xhdpi"
    out.mkdir(exist_ok=True)
    banner.convert("RGB").save(out / "banner.png", optimize=True)
    print("wrote", out / "banner.png")


if __name__ == "__main__":
    main()
