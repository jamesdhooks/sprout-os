#!/usr/bin/env python3
"""Build the reviewed Blocks & Buttons robot atlases for Sprout and PICO-8."""

from __future__ import annotations

import argparse
import io
import json
from pathlib import Path

from PIL import Image

from pico8_sprite_source import compile_gfx


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "games" / "blocks-buttons" / "assets-src" / "robot"
NATIVE_IMAGE = ROOT / "games" / "blocks-buttons" / "assets" / "rich-character.png"
NATIVE_MANIFEST = ROOT / "games" / "blocks-buttons" / "assets-src" / "rich-character-atlas.json"
PICO_CART = ROOT / "pico8" / "blocks-buttons" / "blocks-buttons.p8"
PICO_SOURCE = ROOT / "pico8" / "blocks-buttons" / "sprites.json"

DIRECTIONS = ("south", "north", "west")
FRAME_NAMES = tuple(
    name
    for direction in DIRECTIONS
    for name in (
        *(f"hero-{direction}-{index:02d}" for index in range(1, 5)),
        f"hero-{direction}-push",
    )
)


def source_image(name: str) -> Image.Image:
    path = SOURCE / f"{name}.png"
    if not path.is_file():
        raise ValueError(f"missing reviewed robot frame: {path}")
    image = Image.open(path).convert("RGBA")
    if image.size != (256, 256):
        raise ValueError(f"{path.name} must be RGBA 256x256, got {image.size}")
    if image.getbbox() is None:
        raise ValueError(f"{path.name} is blank")
    return image


def native_outputs() -> tuple[bytes, bytes]:
    atlas = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    frames: dict[str, object] = {}
    for index, name in enumerate(FRAME_NAMES):
        x, y = (index % 4) * 256, (index // 4) * 256
        atlas.alpha_composite(source_image(name), (x, y))
        frames[name] = {
            "frame": {"x": x, "y": y, "width": 256, "height": 256},
            "rotated": False,
            "trimmed": False,
            "spriteSourceSize": {"x": 0, "y": 0, "width": 256, "height": 256},
            "sourceSize": {"width": 256, "height": 256},
            "pivot": {"x": 0.5, "y": 1.0},
        }
    animations = {}
    for direction in DIRECTIONS:
        animations[f"hero-walk-{direction}"] = {
            "frames": [f"hero-{direction}-{index:02d}" for index in range(1, 5)],
            "fps": 8,
            "loop": True,
        }
        animations[f"hero-push-{direction}"] = {
            "frames": [f"hero-{direction}-push"], "fps": 8, "loop": True,
        }
    manifest = {
        "schema": "sprite-atlas.v1",
        "id": "blocks-buttons-character",
        "image": "blocks-buttons-character-atlas.png",
        "size": {"width": 1024, "height": 1024},
        "frames": frames,
        "animations": animations,
    }
    encoded = io.BytesIO()
    atlas.save(encoded, format="PNG", optimize=False, compress_level=9)
    return encoded.getvalue(), (json.dumps(manifest, indent=2) + "\n").encode()


def pico_gfx() -> str:
    payload = json.loads(PICO_SOURCE.read_text(encoding="utf-8"))
    frames = payload.get("frames", {})
    for name in ("wall", "crate", "crate-solved"):
        if frames.get(name, {}).get("rect", [None] * 4)[2:] != [16, 20]:
            raise ValueError(f"{name} must remain a bottom-anchored 16x20 raised frame")
    for name in FRAME_NAMES:
        if frames.get(name, {}).get("rect", [None] * 4)[2:] != [16, 16]:
            raise ValueError(f"{name} must remain a bottom-anchored 16x16 robot frame")
    return compile_gfx(PICO_SOURCE)


def cart_with_gfx(gfx: str) -> bytes:
    text = PICO_CART.read_text(encoding="utf-8")
    head, tail = text.split("__gfx__\n", 1)
    if "__gff__" in tail:
        _, rest = tail.split("__gff__", 1)
        text = head + "__gfx__\n" + gfx + "\n__gff__" + rest
    else:
        text = head + "__gfx__\n" + gfx + "\n"
    return text.encode("utf-8")


def write_or_check(path: Path, data: bytes, check: bool, image: bool = False) -> None:
    if check:
        matches = path.is_file()
        if matches and image:
            actual = Image.open(path).convert("RGBA")
            expected = Image.open(io.BytesIO(data)).convert("RGBA")
            matches = actual.size == expected.size and actual.tobytes() == expected.tobytes()
        elif matches:
            matches = path.read_bytes() == data
        if not matches:
            raise SystemExit(f"generated asset is stale: {path}")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    native_image, native_manifest = native_outputs()
    write_or_check(NATIVE_IMAGE, native_image, args.check, image=True)
    write_or_check(NATIVE_MANIFEST, native_manifest, args.check)
    write_or_check(PICO_CART, cart_with_gfx(pico_gfx()), args.check)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
