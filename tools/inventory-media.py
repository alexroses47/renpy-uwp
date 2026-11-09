#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""Inventory the media formats a Ren'Py game uses, to configure FFmpeg/SDL2_image minimally.

Scans the game/ folder (loose files and RPA-3.0 archives) and runs ffprobe on the first bytes of
every audio/video file (fed through stdin; nothing is extracted to disk). Images are identified by
their magic bytes. Only aggregate counts are written: no file names, no game content.

Usage: tools/inventory-media.py GAME_DIR [OUTPUT_JSON]
       (default output: docs/<game folder name>-media-inventory.json)
Requires ffprobe in PATH.
"""

import collections
import io
import json
import pickle
import struct
import subprocess
import sys
import zlib
from pathlib import Path

AUDIO_EXT = {".ogg", ".opus", ".oga", ".mp3", ".wav", ".flac", ".m4a", ".aac", ".mp2"}
VIDEO_EXT = {".webm", ".mkv", ".ogv", ".mp4", ".m4v", ".avi", ".mpg", ".mpeg", ".mov"}
IMAGE_EXT = {".png", ".jpg", ".jpeg", ".webp", ".avif", ".bmp", ".gif", ".tga", ".svg"}
HEAD_BYTES = {"audio": 1 << 20, "video": 16 << 20}


class SafeUnpickler(pickle.Unpickler):
    """RPA indexes only contain dicts/lists/tuples/str/bytes/int: refuse any class lookup."""

    def find_class(self, module, name):
        raise pickle.UnpicklingError(f"refusing to load {module}.{name} from an RPA index")


def rpa_entries(path):
    """Yields (name, offset, length, prefix) for an RPA-3.0 archive."""
    with open(path, "rb") as f:
        header = f.readline().decode("ascii", "replace").split()
        if not header or header[0] != "RPA-3.0":
            print(f"skip {path.name}: unsupported archive header {header[:1]}", file=sys.stderr)
            return
        offset, key = int(header[1], 16), int(header[2], 16)
        f.seek(offset)
        index = SafeUnpickler(io.BytesIO(zlib.decompress(f.read()))).load()
    for name, chunks in index.items():
        chunk = chunks[0]
        off, length = chunk[0] ^ key, chunk[1] ^ key
        prefix = chunk[2] if len(chunk) > 2 else b""
        if isinstance(prefix, str):
            prefix = prefix.encode("latin-1")
        yield name, off, length, prefix


def read_head(path, offset, length, prefix, limit):
    with open(path, "rb") as f:
        f.seek(offset)
        return prefix + f.read(max(0, min(length, limit) - len(prefix)))


def image_kind(data):
    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        info = "png"
        if len(data) >= 26:
            bit_depth, color_type = data[24], data[25]
            info += f" (color type {color_type}, {bit_depth}-bit)"
        return info
    if data[:3] == b"\xff\xd8\xff":
        return "jpeg"
    if data[:4] == b"RIFF" and data[8:12] == b"WEBP":
        sub = data[12:16].decode("ascii", "replace")
        return f"webp ({sub.strip()})"
    if data[4:12] in (b"ftypavif", b"ftypavis"):
        return "avif"
    if data[:2] == b"BM":
        return "bmp"
    if data[:4] in (b"GIF8",):
        return "gif"
    if b"<svg" in data[:512]:
        return "svg"
    return "unknown"


def ffprobe(data):
    p = subprocess.run(
        ["ffprobe", "-v", "error", "-show_format", "-show_streams", "-of", "json", "-i", "pipe:0"],
        input=data, capture_output=True)
    try:
        return json.loads(p.stdout.decode("utf-8", "replace") or "{}")
    except json.JSONDecodeError:
        return {}


def main():
    game = Path(sys.argv[1]).expanduser()
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else (
        Path(__file__).resolve().parent.parent / "docs" / f"{game.parent.name.lower().replace(' ', '-')}-media-inventory.json")

    items = []  # (ext, source_path, offset, length, prefix)
    archives = []
    for f in sorted(game.rglob("*")):
        if not f.is_file() or "saves" in f.parts or "cache" in f.parts:
            continue
        if f.suffix.lower() == ".rpa":
            n = 0
            for name, off, length, prefix in rpa_entries(f):
                items.append((Path(name).suffix.lower(), f, off, length, prefix))
                n += 1
            archives.append({"archive": f.name, "entries": n})
        else:
            items.append((f.suffix.lower(), f, 0, f.stat().st_size, b""))

    by_ext = collections.Counter(ext or "(none)" for ext, *_ in items)
    containers = collections.Counter()
    codecs = {"audio": collections.Counter(), "video": collections.Counter(), "subtitle": collections.Counter()}
    audio_params = collections.Counter()
    video_params = collections.Counter()
    images = collections.Counter()
    probe_failures = collections.Counter()

    for i, (ext, src, off, length, prefix) in enumerate(items):
        if ext in IMAGE_EXT:
            images[image_kind(read_head(src, off, length, prefix, 64))] += 1
            continue
        kind = "audio" if ext in AUDIO_EXT else "video" if ext in VIDEO_EXT else None
        if kind is None:
            continue
        info = ffprobe(read_head(src, off, length, prefix, HEAD_BYTES[kind]))
        fmt = info.get("format", {}).get("format_name")
        if not fmt:
            probe_failures[ext] += 1
            continue
        containers[f"{fmt} ({ext})"] += 1
        for s in info.get("streams", []):
            t = s.get("codec_type")
            name = s.get("codec_name", "?")
            if t in codecs:
                codecs[t][name] += 1
            if t == "audio":
                audio_params[f"{name} {s.get('sample_rate')} Hz {s.get('channels')} ch {s.get('sample_fmt')}"] += 1
            elif t == "video":
                rate = s.get("avg_frame_rate") or s.get("r_frame_rate")
                video_params[f"{name} {s.get('width')}x{s.get('height')} {s.get('pix_fmt')} {rate} fps"] += 1
        if (i + 1) % 200 == 0:
            print(f"  {i + 1}/{len(items)} entries scanned", file=sys.stderr)

    result = {
        "game_folder": game.parent.name,
        "archives": archives,
        "files_by_extension": dict(by_ext.most_common()),
        "containers": dict(containers.most_common()),
        "codecs": {k: dict(v.most_common()) for k, v in codecs.items() if v},
        "audio_streams": dict(audio_params.most_common()),
        "video_streams": dict(video_params.most_common()),
        "image_formats": dict(images.most_common()),
        "ffprobe_failures": dict(probe_failures),
        "ffprobe_version": subprocess.run(["ffprobe", "-version"], capture_output=True, text=True).stdout.split("\n")[0],
    }
    out.write_text(json.dumps(result, indent=1, sort_keys=False) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=1))
    print(f"written {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
