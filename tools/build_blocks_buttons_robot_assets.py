#!/usr/bin/env python3
"""Build the reviewed Blocks & Buttons robot atlases for Sprout and PICO-8."""

from __future__ import annotations

import argparse
import io
import json
from pathlib import Path

from PIL import Image

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
NATIVE_PALETTE = (
    "#102F5B", "#173B70", "#16579B", "#2584CE", "#39AFCC", "#66768C",
    "#A9BCC8", "#E4EEF0", "#8F2C36", "#D93932", "#EF654B", "#704327",
    "#B86B38", "#E4A35B", "#FFC53B", "#F6F0DF",
)
MASTER_SIZE = (128, 160)


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


def master_image(name: str) -> Image.Image:
    source = source_image(name)
    bounds = source.getchannel("A").getbbox()
    if bounds is None:
        raise ValueError(f"{name} has no opaque pixels")
    subject = source.crop(bounds)
    subject.thumbnail((118, 150), Image.Resampling.NEAREST)
    result = Image.new("RGBA", MASTER_SIZE, (0, 0, 0, 0))
    result.alpha_composite(subject, ((MASTER_SIZE[0] - subject.width) // 2,
                                     MASTER_SIZE[1] - subject.height))
    colors = [tuple(int(value[index:index + 2], 16) for index in (1, 3, 5))
              for value in NATIVE_PALETTE]
    for y in range(result.height):
        for x in range(result.width):
            red, green, blue, alpha = result.getpixel((x, y))
            if not alpha:
                continue
            chosen = min(colors, key=lambda color: sum((component - target) ** 2
                         for component, target in zip(color, (red, green, blue))))
            result.putpixel((x, y), (*chosen, alpha))
    return result


def native_outputs() -> tuple[bytes, bytes]:
    atlas = Image.new("RGBA", (512, 640), (0, 0, 0, 0))
    frames: dict[str, object] = {}
    for index, name in enumerate(FRAME_NAMES):
        x, y = (index % 4) * MASTER_SIZE[0], (index // 4) * MASTER_SIZE[1]
        atlas.alpha_composite(master_image(name), (x, y))
        frames[name] = {
            "frame": {"x": x, "y": y, "width": MASTER_SIZE[0], "height": MASTER_SIZE[1]},
            "rotated": False,
            "trimmed": False,
            "spriteSourceSize": {"x": 0, "y": 0, "width": MASTER_SIZE[0], "height": MASTER_SIZE[1]},
            "sourceSize": {"width": MASTER_SIZE[0], "height": MASTER_SIZE[1]},
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
        "size": {"width": 512, "height": 640},
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
        colours = set("".join(frames[name]["pixels"]))
        if not {"8", "6", "c"}.issubset(colours):
            raise ValueError(f"{name} must retain its red light, silver arms, and blue body")
    for name in ("button-up", "button-down"):
        colours = set("".join(frames[name]["pixels"]))
        if not {"8", "6"}.issubset(colours):
            raise ValueError(f"{name} must retain its red cap and silver housing")
    if "f" not in "".join(frames["crate"]["pixels"]):
        raise ValueError("crate must retain its engraved star")
    canvas = [["0"] * 128 for _ in range(128)]
    occupied: set[tuple[int, int]] = set()
    for name, frame in frames.items():
        source_x, source_y, width, height = frame["rect"]
        rows = frame["pixels"]
        if name.startswith("hero-"):
            direction = name.split("-")[1]
            target_y = {"south": 0, "north": 20, "west": 40}[direction]
            target_x, target_width, target_height = source_x, 16, 20
            output_rows = [rows[min(15, dy * 16 // 20)] for dy in range(20)]
        else:
            target_x, target_y = source_x, source_y + 16
            target_width, target_height, output_rows = width, height, rows
        for dy, row in enumerate(output_rows):
            for dx, value in enumerate(row):
                point = (target_x + dx, target_y + dy)
                if point in occupied:
                    raise ValueError(f"compiled PICO frame {name} overlaps at {point}")
                occupied.add(point); canvas[point[1]][point[0]] = value
    return "\n".join("".join(row) for row in canvas)


def cart_with_gfx(gfx: str) -> bytes:
    text = PICO_CART.read_text(encoding="utf-8")
    head, tail = text.split("__gfx__\n", 1)
    boundary = tail.find("\n__")
    rest = tail[boundary + 1:] if boundary >= 0 else ""
    text = head + "__gfx__\n" + gfx + "\n" + rest
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
