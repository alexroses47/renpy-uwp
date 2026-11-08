#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""Generate the placeholder logo/splash PNGs required by Package.appxmanifest.

These are original, trivially simple images (solid background + a light square),
so they carry no third-party license. Run from the repo root:

    python3 host/tools/make_placeholder_assets.py
"""
import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "RenpyUWP" / "Assets"

SIZES = {
    "StoreLogo.png": (50, 50),
    "Square44x44Logo.png": (44, 44),
    "Square150x150Logo.png": (150, 150),
    "Wide310x150Logo.png": (310, 150),
    "SplashScreen.png": (620, 300),
}

BG = (0x20, 0x20, 0x20)
FG = (0x00, 0x8C, 0x9E)


def png(width: int, height: int) -> bytes:
    side = min(width, height) // 2
    x0, y0 = (width - side) // 2, (height - side) // 2
    rows = bytearray()
    for y in range(height):
        rows.append(0)  # filter: none
        for x in range(width):
            inside = x0 <= x < x0 + side and y0 <= y < y0 + side
            rows.extend(FG if inside else BG)

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (w, h) in SIZES.items():
        (OUT / name).write_bytes(png(w, h))
        print(f"wrote {OUT / name} ({w}x{h})")


if __name__ == "__main__":
    main()
