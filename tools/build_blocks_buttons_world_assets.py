#!/usr/bin/env python3
"""Build screen-aligned top-down Blocks & Buttons workshop assets."""

from __future__ import annotations

import argparse
import io
import json
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "games/blocks-buttons/assets-src/world"
ATLAS = ROOT / "games/blocks-buttons/assets/rich-world.png"
MANIFEST = ROOT / "games/blocks-buttons/assets-src/rich-world-atlas.json"
FRAME_NAMES = ("floor-wood", "wall-workshop", "crate-idle", "crate-solved", "button-up", "button-down")
P = {
    "navy": "#102F5B", "deep": "#173B70", "blue": "#16579B",
    "bright": "#2584CE", "cyan": "#39AFCC", "steel": "#66768C",
    "silver": "#A9BCC8", "white": "#E4EEF0", "dark_red": "#8F2C36",
    "red": "#D93932", "coral": "#EF654B", "dark_wood": "#704327",
    "wood": "#B86B38", "light_wood": "#E4A35B", "gold": "#FFC53B",
    "cream": "#F6F0DF",
}


def new() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def floor() -> Image.Image:
    image, draw = new()
    # Neutral steel floor keeps the blue robot visually dominant. The bottom
    # and right seams form the room grid without adding a panel inside a panel.
    draw.rectangle((0, 0, 255, 255), fill=P["steel"])
    draw.line((0, 0, 255, 0), fill=P["silver"], width=5)
    draw.line((0, 0, 0, 255), fill=P["silver"], width=5)
    draw.line((0, 253, 255, 253), fill=P["navy"], width=3)
    draw.line((253, 0, 253, 255), fill=P["navy"], width=3)
    return image


def wall() -> Image.Image:
    image, draw = new()
    # One solid cap with edge lighting, never concentric inset squares.
    draw.rounded_rectangle((4, 4, 251, 251), radius=13, fill=P["steel"], outline=P["navy"], width=7)
    draw.line((14, 14, 238, 14), fill=P["white"], width=7)
    draw.line((14, 14, 14, 238), fill=P["silver"], width=7)
    draw.line((17, 241, 241, 241), fill=P["deep"], width=5)
    draw.line((241, 17, 241, 241), fill=P["deep"], width=5)
    return image


def crate(solved: bool = False) -> Image.Image:
    image, draw = new()
    outer = P["gold"] if solved else P["dark_wood"]
    face = P["light_wood"] if solved else P["wood"]
    inset = P["gold"] if solved else P["light_wood"]
    draw.rectangle((12, 12, 243, 243), fill=P["navy"])
    draw.rectangle((20, 20, 235, 235), fill=outer)
    draw.rectangle((32, 32, 223, 223), fill=face)
    draw.rectangle((48, 48, 207, 207), fill=inset)
    draw.line((52, 52, 203, 203), fill=P["dark_wood"], width=20)
    draw.line((203, 52, 52, 203), fill=P["dark_wood"], width=20)
    draw.rectangle((16, 16, 239, 25), fill=P["cream"] if solved else P["light_wood"])
    return image


def button(pressed: bool = False) -> Image.Image:
    image, draw = new()
    # Exactly one silver housing around one red cap.
    draw.ellipse((34, 34, 221, 221), fill=P["navy"])
    draw.ellipse((44, 44, 211, 211), fill=P["silver"])
    if pressed:
        draw.ellipse((61, 61, 194, 194), fill=P["dark_red"])
        draw.ellipse((68, 68, 187, 187), fill=P["red"])
    else:
        draw.ellipse((56, 56, 199, 199), fill=P["dark_red"])
        draw.ellipse((64, 64, 191, 191), fill=P["coral"])
        draw.arc((77, 75, 178, 174), 205, 325, fill=P["cream"], width=7)
    return image


def outputs() -> tuple[bytes, bytes, dict[str, bytes]]:
    images = {"floor-wood": floor(), "wall-workshop": wall(), "crate-idle": crate(),
              "crate-solved": crate(True), "button-up": button(), "button-down": button(True)}
    atlas = Image.new("RGBA", (1024, 512), (0, 0, 0, 0)); frames = {}; sources = {}
    for index, name in enumerate(FRAME_NAMES):
        x, y = (index % 4) * 256, (index // 4) * 256
        atlas.alpha_composite(images[name], (x, y))
        frames[name] = {"frame": {"x": x, "y": y, "width": 256, "height": 256},
                        "rotated": False, "trimmed": False,
                        "spriteSourceSize": {"x": 0, "y": 0, "width": 256, "height": 256},
                        "sourceSize": {"width": 256, "height": 256},
                        "pivot": {"x": 0.5, "y": 1.0}}
        stream = io.BytesIO(); images[name].save(stream, format="PNG", compress_level=9); sources[name] = stream.getvalue()
    manifest = {"schema": "sprite-atlas.v1", "id": "blocks-buttons-world",
                "image": "blocks-buttons-world-atlas.png", "size": {"width": 1024, "height": 512},
                "frames": frames, "animations": {}}
    stream = io.BytesIO(); atlas.save(stream, format="PNG", compress_level=9)
    return stream.getvalue(), (json.dumps(manifest, indent=2) + "\n").encode(), sources


def write_or_check(path: Path, data: bytes, check: bool) -> None:
    if check:
        if not path.is_file() or path.read_bytes() != data: raise SystemExit(f"generated asset is stale: {path}")
    else:
        path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("--check", action="store_true")
    args = parser.parse_args(); atlas, manifest, sources = outputs()
    write_or_check(ATLAS, atlas, args.check); write_or_check(MANIFEST, manifest, args.check)
    for name, data in sources.items(): write_or_check(SOURCE / f"{name}.png", data, args.check)
    return 0


if __name__ == "__main__": raise SystemExit(main())
