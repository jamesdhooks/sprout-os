#!/usr/bin/env python3
"""Build rotationally exact native and PICO-8 Starlight Snake character atlases."""

from __future__ import annotations

import argparse
import io
import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "games" / "snake" / "assets-src" / "snake"
NATIVE_IMAGE = ROOT / "games" / "snake" / "assets" / "rich-character.png"
NATIVE_MANIFEST = ROOT / "games" / "snake" / "assets-src" / "rich-character-atlas.json"
PICO_CART = ROOT / "pico8" / "snake" / "snake.p8"
PALETTE = ((0, 0, 0), (29, 43, 83), (126, 37, 83), (0, 135, 81),
           (171, 82, 54), (95, 87, 79), (194, 195, 199), (255, 241, 232),
           (255, 0, 77), (255, 163, 0), (255, 236, 39), (0, 228, 54),
           (41, 173, 255), (131, 118, 156), (255, 119, 168), (255, 204, 170))

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


def open_connector(image: Image.Image, edge: str, depth: int = 28) -> Image.Image:
    """Extend the interior through one declared join edge, removing cross-seams."""
    result = image.copy()
    if edge in ("north", "south"):
        sample_y = depth if edge == "north" else image.height - depth - 1
        target = range(0, depth + 1) if edge == "north" else range(image.height - depth - 1, image.height)
        for x in range(image.width):
            sample = image.getpixel((x, sample_y))
            if sample[3] >= 128:
                for y in target:
                    result.putpixel((x, y), sample)
    else:
        sample_x = depth if edge == "west" else image.width - depth - 1
        target = range(0, depth + 1) if edge == "west" else range(image.width - depth - 1, image.width)
        for y in range(image.height):
            sample = image.getpixel((sample_x, y))
            if sample[3] >= 128:
                for x in target:
                    result.putpixel((x, y), sample)
    return result


def soften_body_markings(image: Image.Image) -> Image.Image:
    """Keep modeled shading but prevent large cream stamps dominating tiny cells."""
    result = image.copy()
    for y in range(result.height):
        for x in range(result.width):
            red, green, blue, alpha = result.getpixel((x, y))
            if alpha and red > 225 and green > 205 and blue > 150:
                result.putpixel((x, y), (255, 190, 45, alpha))
    return result


def variants() -> dict[str, Image.Image]:
    head = open_connector(load("snake-head-north"), "south")
    straight = soften_body_markings(open_connector(open_connector(load("snake-body-horizontal"), "west"), "east"))
    # The accepted source connects west+north; other corners are exact rotations.
    corner_wn = soften_body_markings(open_connector(open_connector(load("snake-body-corner-ne"), "west"), "north"))
    tail = soften_body_markings(open_connector(load("snake-tail-north"), "south"))
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


def palette_index(rgb: tuple[int, int, int]) -> int:
    return min(range(1, 16), key=lambda i: sum((rgb[c] - PALETTE[i][c]) ** 2 for c in range(3)))


def pico_gfx(images: dict[str, Image.Image]) -> str:
    pixels = [[0] * 128 for _ in range(128)]
    for index, name in enumerate(FRAME_NAMES):
        frame = images[name].resize((8, 8), Image.Resampling.NEAREST)
        ox = index * 8
        for y in range(8):
            for x in range(8):
                pixel = frame.getpixel((x, y))
                if pixel[3] >= 128:
                    pixels[y][ox + x] = palette_index(pixel[:3])
    return "\n".join("".join(format(value, "x") for value in row) for row in pixels)


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
