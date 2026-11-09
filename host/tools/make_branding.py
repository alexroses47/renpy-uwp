# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""Builds the app tiles/splash and the menu images from host/branding/isrotaxo-logo.png.

The logo (flower + "IsroTaxo" lettering on a dark navy background) is the project author's artwork.
  - square icons (44, 50, 150): the flower only, centred on the background colour
  - wide tile (310x150) and splash (620x300): the whole logo
  - menu images: logo.png (whole logo, 1100 px wide) and flower.png (flower, 360 px)
Usage: python3 host/tools/make_branding.py      (needs Pillow)
"""

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "host" / "branding" / "isrotaxo-logo.png"
ASSETS = ROOT / "host" / "RenpyUWP" / "Assets"
MENU_IMAGES = ROOT / "host" / "RenpyUWP" / "menu" / "game" / "images"
BG = (7, 14, 31)


def content_bbox(im, region=None, threshold=40):
    """Bounding box of pixels that differ from the background (optionally inside a region)."""
    box = region or (0, 0, im.width, im.height)
    sub = im.crop(box)
    mask = sub.point(lambda v: 255 if v > threshold else 0).convert("L")
    diff = Image.new("L", sub.size)
    px, dp = sub.load(), diff.load()
    for y in range(sub.height):
        for x in range(sub.width):
            r, g, b = px[x, y]
            if abs(r - BG[0]) + abs(g - BG[1]) + abs(b - BG[2]) > threshold:
                dp[x, y] = 255
    del mask
    l, t, r, b = diff.getbbox()
    return (box[0] + l, box[1] + t, box[0] + r, box[1] + b)


def fit(im, size, margin=0.08):
    """Scale `im` to fit `size` with a margin, centred on the background colour."""
    w, h = size
    avail = (int(w * (1 - 2 * margin)), int(h * (1 - 2 * margin)))
    scaled = im.copy()
    scaled.thumbnail(avail, Image.LANCZOS)
    canvas = Image.new("RGB", size, BG)
    canvas.paste(scaled, ((w - scaled.width) // 2, (h - scaled.height) // 2))
    return canvas


def main():
    logo = Image.open(SRC).convert("RGB")
    whole = logo.crop(content_bbox(logo))
    # The flower is in the left ~46% of the logo; its vine overlaps the "I" of the lettering in x but
    # not in y, so blank the lettering band (right of 44%, below 38%) before cropping.
    only_flower = logo.copy()
    only_flower.paste(BG, (int(logo.width * 0.44), int(logo.height * 0.38), logo.width, logo.height))
    flower = only_flower.crop(content_bbox(only_flower, (0, 0, int(logo.width * 0.46), logo.height)))

    ASSETS.mkdir(parents=True, exist_ok=True)
    fit(flower, (44, 44), 0.04).save(ASSETS / "Square44x44Logo.png")
    fit(flower, (50, 50), 0.04).save(ASSETS / "StoreLogo.png")
    fit(flower, (150, 150), 0.08).save(ASSETS / "Square150x150Logo.png")
    fit(whole, (310, 150), 0.08).save(ASSETS / "Wide310x150Logo.png")
    fit(whole, (620, 300), 0.06).save(ASSETS / "SplashScreen.png")

    MENU_IMAGES.mkdir(parents=True, exist_ok=True)
    w = whole.copy()
    w.thumbnail((1100, 1100), Image.LANCZOS)
    w.save(MENU_IMAGES / "logo.png", optimize=True)
    f = flower.copy()
    f.thumbnail((360, 360), Image.LANCZOS)
    f.save(MENU_IMAGES / "flower.png", optimize=True)
    print("whole", whole.size, "flower", flower.size)


if __name__ == "__main__":
    main()
