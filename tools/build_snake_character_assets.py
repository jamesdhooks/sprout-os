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


OUTLINE = (28, 24, 69, 255)
GOLD = (244, 177, 36, 255)
LIGHT = (255, 218, 79, 255)
SHADOW = (211, 118, 31, 255)
CREAM = (255, 240, 189, 255)
TEAL = (46, 154, 160, 255)
OUTER_WIDTH = 116
BODY_WIDTH = 88


def blank() -> Image.Image:
    return Image.new("RGBA", (256, 256), (0, 0, 0, 0))


def path(points: list[tuple[int, int]]) -> Image.Image:
    image = blank()
    draw = ImageDraw.Draw(image)
    draw.line(points, fill=OUTLINE, width=OUTER_WIDTH, joint="curve")
    draw.line(points, fill=GOLD, width=BODY_WIDTH, joint="curve")
    # A contained, narrow highlight reads as material rather than a second body.
    shifted = [(x - 13, y - 13) for x, y in points]
    draw.line(shifted, fill=LIGHT, width=9, joint="curve")
    return image


def head_north() -> Image.Image:
    image = path([(128, 256), (128, 137)])
    draw = ImageDraw.Draw(image)
    draw.ellipse((42, 18, 214, 184), fill=OUTLINE)
    draw.ellipse((54, 29, 202, 172), fill=GOLD)
    draw.ellipse((67, 40, 187, 105), fill=LIGHT)
    for x in (86, 150):
        draw.ellipse((x, 55, x + 25, 85), fill=CREAM)
        draw.ellipse((x + 8, 60, x + 19, 77), fill=OUTLINE)
        draw.ellipse((x + 11, 62, x + 15, 67), fill=TEAL)
    draw.arc((95, 88, 161, 132), 20, 160, fill=OUTLINE, width=8)
    # Re-open the body connector after the head/body overlap.
    draw.rectangle((128 - BODY_WIDTH // 2, 165, 128 + BODY_WIDTH // 2, 255), fill=GOLD)
    draw.rectangle((128 - BODY_WIDTH // 2 + 12, 165, 128 - BODY_WIDTH // 2 + 20, 255), fill=LIGHT)
    return image


def tail_north() -> Image.Image:
    image = blank()
    draw = ImageDraw.Draw(image)
    draw.polygon(((128, 18), (128 - OUTER_WIDTH // 2, 256),
                  (128 + OUTER_WIDTH // 2, 256)), fill=OUTLINE)
    draw.polygon(((128, 39), (128 - BODY_WIDTH // 2, 256),
                  (128 + BODY_WIDTH // 2, 256)), fill=GOLD)
    draw.line((113, 67, 95, 244), fill=LIGHT, width=8)
    return image


def variants() -> dict[str, Image.Image]:
    head = head_north()
    straight = path([(0, 128), (255, 128)])
    corner_wn = path([(0, 128), (128, 128), (128, 0)])
    tail = tail_north()
    return {
        "snake-head-north": head, "snake-head-east": rotate(head, -90),
        "snake-head-south": rotate(head, 180), "snake-head-west": rotate(head, 90),
        "snake-body-vertical": rotate(straight, 90), "snake-body-horizontal": straight,
        "snake-corner-ne": rotate(corner_wn, -90), "snake-corner-es": rotate(corner_wn, 180),
        "snake-corner-sw": rotate(corner_wn, 90), "snake-corner-wn": corner_wn,
        "snake-tail-north": tail, "snake-tail-east": rotate(tail, -90),
        "snake-tail-south": rotate(tail, 180), "snake-tail-west": rotate(tail, 90),
        "fruit-apple": load("fruit-apple"), "fruit-pear": load("fruit-pear"),
    }


def native_outputs(images: dict[str, Image.Image]) -> tuple[bytes, bytes]:
    atlas = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    frames = {}
    for index, name in enumerate(FRAME_NAMES):
        x, y = (index % 4) * 256, (index // 4) * 256
        atlas.alpha_composite(images[name], (x, y))
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
    if "__gff__" in tail:
        _, rest = tail.split("__gff__", 1)
        text = head + "__gfx__\n" + gfx + "\n__gff__" + rest
    else:
        text = head + "__gfx__\n" + gfx + "\n"
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
