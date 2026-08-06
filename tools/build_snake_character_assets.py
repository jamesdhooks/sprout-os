#!/usr/bin/env python3
"""Build rotationally exact native and PICO-8 Starlight Snake character atlases."""

from __future__ import annotations

import argparse
import io
import json
from pathlib import Path

from PIL import Image, ImageDraw

from pico8_sprite_source import compile_gfx


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "games" / "snake" / "assets-src" / "snake"
NATIVE_IMAGE = ROOT / "games" / "snake" / "assets" / "rich-character.png"
NATIVE_MANIFEST = ROOT / "games" / "snake" / "assets-src" / "rich-character-atlas.json"
PICO_CART = ROOT / "pico8" / "snake" / "snake.p8"
PICO_SOURCE = ROOT / "pico8" / "snake" / "sprites.json"
FRAME_NAMES = (
    "snake-head-north", "snake-head-east", "snake-head-south", "snake-head-west",
    "snake-body-vertical", "snake-body-horizontal",
    "snake-corner-ne", "snake-corner-es", "snake-corner-sw", "snake-corner-wn",
    "snake-tail-north", "snake-tail-east", "snake-tail-south", "snake-tail-west",
    "fruit-apple", "fruit-pear",
)


def load(name: str) -> Image.Image:
    path = SOURCE / f"{name}.png"
    if not path.is_file():
        raise ValueError(f"missing reviewed snake source: {path}")
    image = Image.open(path).convert("RGBA")
    if image.size != (256, 256):
        raise ValueError(f"{path.name} must be 256x256")
    return image


def rotate(image: Image.Image, degrees: int) -> Image.Image:
    return image.rotate(degrees, resample=Image.Resampling.NEAREST, expand=False)


OUTLINE = (22, 19, 56, 255)
GOLD = (239, 182, 62, 255)
LIGHT = (255, 222, 112, 255)
SHADOW = (184, 119, 43, 255)
CREAM = (247, 235, 201, 255)
TEAL = (61, 118, 98, 255)
PINK = (238, 106, 145, 255)
OUTER_WIDTH = 15
BODY_WIDTH = 11
TILE_SIZE = 32


def blank() -> Image.Image:
    return Image.new("RGBA", (TILE_SIZE, TILE_SIZE), (0, 0, 0, 0))


def path(points: list[tuple[int, int]]) -> Image.Image:
    image = blank()
    draw = ImageDraw.Draw(image)
    draw.line(points, fill=OUTLINE, width=OUTER_WIDTH, joint="curve")
    draw.line(points, fill=GOLD, width=BODY_WIDTH, joint="curve")
    # A contained, narrow highlight reads as material rather than a second body.
    shifted = [(x - 2, y - 2) for x, y in points]
    draw.line(shifted, fill=LIGHT, width=2, joint="curve")
    for index, (x, y) in enumerate(points[::2]):
        if index % 2 == 0:
            draw.point((x + 2, y + 2), fill=PINK)
    return image


def head_north() -> Image.Image:
    image = path([(16, 32), (16, 17)])
    draw = ImageDraw.Draw(image)
    draw.ellipse((5, 2, 26, 22), fill=OUTLINE)
    draw.ellipse((7, 4, 24, 20), fill=GOLD)
    draw.rectangle((11, 16, 21, 31), fill=GOLD)
    draw.rectangle((11, 16, 12, 31), fill=LIGHT)
    for x in (10, 19):
        draw.ellipse((x, 7, x + 4, 12), fill=CREAM)
        draw.rectangle((x + 2, 8, x + 3, 10), fill=OUTLINE)
        draw.point((x + 2, 8), fill=TEAL)
    draw.line((13, 15, 16, 16, 19, 15), fill=OUTLINE, width=1)
    return image


def tail_north() -> Image.Image:
    image = blank()
    draw = ImageDraw.Draw(image)
    draw.polygon(((16, 2), (16 - OUTER_WIDTH // 2, 32),
                  (16 + OUTER_WIDTH // 2, 32)), fill=OUTLINE)
    draw.polygon(((16, 5), (16 - BODY_WIDTH // 2, 32),
                  (16 + BODY_WIDTH // 2, 32)), fill=GOLD)
    draw.line((14, 8, 12, 30), fill=LIGHT, width=1)
    return image


def fruit(kind: str) -> Image.Image:
    image = blank()
    draw = ImageDraw.Draw(image)
    if kind == "apple":
        draw.rectangle((15, 3, 17, 8), fill=SHADOW)
        draw.polygon(((17, 5), (23, 3), (21, 8)), fill=TEAL)
        draw.ellipse((6, 7, 26, 28), fill=OUTLINE)
        draw.ellipse((8, 9, 24, 26), fill=(200, 59, 104, 255))
        draw.rectangle((10, 11, 13, 14), fill=PINK)
    else:
        draw.rectangle((15, 2, 17, 7), fill=SHADOW)
        draw.polygon(((17, 4), (23, 2), (21, 7)), fill=TEAL)
        draw.ellipse((11, 6, 21, 18), fill=OUTLINE)
        draw.ellipse((6, 13, 26, 29), fill=OUTLINE)
        draw.ellipse((13, 8, 19, 18), fill=LIGHT)
        draw.ellipse((8, 14, 24, 27), fill=LIGHT)
    return image


def variants() -> dict[str, Image.Image]:
    head = head_north()
    straight = path([(0, 16), (31, 16)])
    corner_wn = path([(0, 16), (16, 16), (16, 0)])
    tail = tail_north()
    return {
        "snake-head-north": head, "snake-head-east": rotate(head, -90),
        "snake-head-south": rotate(head, 180), "snake-head-west": rotate(head, 90),
        "snake-body-vertical": rotate(straight, 90), "snake-body-horizontal": straight,
        "snake-corner-ne": rotate(corner_wn, -90), "snake-corner-es": rotate(corner_wn, 180),
        "snake-corner-sw": rotate(corner_wn, 90), "snake-corner-wn": corner_wn,
        "snake-tail-north": tail, "snake-tail-east": rotate(tail, -90),
        "snake-tail-south": rotate(tail, 180), "snake-tail-west": rotate(tail, 90),
        "fruit-apple": fruit("apple"), "fruit-pear": fruit("pear"),
    }


def native_outputs(images: dict[str, Image.Image]) -> tuple[bytes, bytes]:
    atlas = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    frames = {}
    for index, name in enumerate(FRAME_NAMES):
        x, y = (index % 4) * 256, (index // 4) * 256
        atlas.alpha_composite(images[name].resize((256, 256), Image.Resampling.NEAREST), (x, y))
        frames[name] = {
            "frame": {"x": x, "y": y, "width": 256, "height": 256},
            "rotated": False, "trimmed": False,
            "spriteSourceSize": {"x": 0, "y": 0, "width": 256, "height": 256},
            "sourceSize": {"width": 256, "height": 256},
            "pivot": {"x": 0.5, "y": 0.5},
        }
    manifest = {"schema": "sprite-atlas.v1", "id": "snake-character",
                "image": "snake-character-atlas.png",
                "size": {"width": 1024, "height": 1024}, "frames": frames,
                "animations": {"default": {"frames": list(FRAME_NAMES), "fps": 12, "loop": True}}}
    encoded = io.BytesIO()
    atlas.save(encoded, format="PNG", compress_level=9)
    return encoded.getvalue(), (json.dumps(manifest, indent=2) + "\n").encode()


def pico_gfx(_images: dict[str, Image.Image]) -> str:
    return compile_gfx(PICO_SOURCE)


def cart_bytes(gfx: str) -> bytes:
    text = PICO_CART.read_text(encoding="utf-8")
    head, tail = text.split("__gfx__\n", 1)
    boundary = tail.find("\n__")
    rest = tail[boundary + 1:] if boundary >= 0 else ""
    text = head + "__gfx__\n" + gfx + "\n" + rest
    return text.encode()


def output(path: Path, data: bytes, check: bool, image: bool = False) -> None:
    if check:
        matches = path.is_file()
        if matches and image:
            a = Image.open(path).convert("RGBA")
            b = Image.open(io.BytesIO(data)).convert("RGBA")
            matches = a.size == b.size and a.tobytes() == b.tobytes()
        elif matches:
            matches = path.read_bytes() == data
        if not matches:
            raise SystemExit(f"generated asset is stale: {path}")
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    images = variants()
    atlas, manifest = native_outputs(images)
    output(NATIVE_IMAGE, atlas, args.check, image=True)
    output(NATIVE_MANIFEST, manifest, args.check)
    output(PICO_CART, cart_bytes(pico_gfx(images)), args.check)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
