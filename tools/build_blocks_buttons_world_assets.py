#!/usr/bin/env python3
"""Build the reviewed Blocks & Buttons high-angle world atlas."""

from __future__ import annotations

import argparse
import io
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "games/blocks-buttons/assets-src/world"
ATLAS = ROOT / "games/blocks-buttons/assets/rich-world.png"
MANIFEST = ROOT / "games/blocks-buttons/assets-src/rich-world-atlas.json"
FRAME_NAMES = (
    "floor-wood", "wall-workshop", "crate-idle",
    "crate-solved", "button-up", "button-down",
)
NATIVE_PALETTE = tuple(
    tuple(int(value[index:index + 2], 16) for index in (1, 3, 5))
    for value in ("#102F5B", "#173B70", "#16579B", "#2584CE", "#39AFCC", "#66768C",
                  "#A9BCC8", "#E4EEF0", "#8F2C36", "#D93932", "#EF654B", "#704327",
                  "#B86B38", "#E4A35B", "#FFC53B", "#F6F0DF")
)


def exact_palette(image: Image.Image) -> Image.Image:
    palette = Image.new("P", (1, 1))
    values = [component for color in NATIVE_PALETTE for component in color]
    palette.putpalette(values + [0] * (768 - len(values)))
    quantized = image.convert("RGB").quantize(
        palette=palette, dither=Image.Dither.NONE).convert("RGBA")
    quantized.putalpha(image.getchannel("A"))
    return quantized


def source(name: str) -> Image.Image:
    path = SOURCE / f"{name}.png"
    if not path.is_file():
        raise ValueError(f"missing reviewed world frame: {path}")
    image = Image.open(path).convert("RGBA")
    if image.size != (256, 256):
        raise ValueError(f"{path.name} must be 256x256, got {image.size}")
    return exact_palette(image)


def solved_crate() -> Image.Image:
    image = crate_with_star()
    color = ImageEnhance.Color(image.convert("RGB")).enhance(1.15).convert("RGBA")
    color.putalpha(image.getchannel("A"))
    overlay = Image.new("RGBA", image.size, (255, 190, 55, 0))
    overlay.putalpha(image.getchannel("A").point(lambda alpha: 38 if alpha else 0))
    return Image.alpha_composite(color, overlay)


def star_points(cx: int, cy: int, outer: int, inner: int) -> list[tuple[int, int]]:
    points = []
    for index in range(10):
        radius = outer if index % 2 == 0 else inner
        angle = -math.pi / 2 + index * math.pi / 5
        points.append((round(cx + math.cos(angle) * radius),
                       round(cy + math.sin(angle) * radius)))
    return points


def crate_with_star() -> Image.Image:
    image = source("crate-idle")
    draw = ImageDraw.Draw(image)
    draw.polygon(star_points(128, 112, 42, 19), fill="#704327")
    draw.polygon(star_points(128, 108, 32, 14), fill="#FFC53B")
    return image


def pressed_button() -> Image.Image:
    image = source("button-up")
    bbox = image.getchannel("A").getbbox()
    if bbox is None:
        raise ValueError("button-up is blank")
    subject = image.crop(bbox)
    subject = subject.resize((subject.width, max(1, round(subject.height * 0.82))), Image.Resampling.NEAREST)
    result = Image.new("RGBA", image.size, (0, 0, 0, 0))
    result.alpha_composite(subject, ((256 - subject.width) // 2, 256 - subject.height))
    return result


def outputs() -> tuple[bytes, bytes]:
    images = {name: exact_palette(image) for name, image in {
        "floor-wood": source("floor-wood"),
        "wall-workshop": source("wall-workshop"),
        "crate-idle": crate_with_star(),
        "crate-solved": solved_crate(),
        "button-up": source("button-up"),
        "button-down": pressed_button(),
    }.items()}
    atlas = Image.new("RGBA", (1024, 512), (0, 0, 0, 0))
    frames: dict[str, object] = {}
    for index, name in enumerate(FRAME_NAMES):
        x, y = (index % 4) * 256, (index // 4) * 256
        atlas.alpha_composite(images[name], (x, y))
        frames[name] = {
            "frame": {"x": x, "y": y, "width": 256, "height": 256},
            "rotated": False,
            "trimmed": False,
            "spriteSourceSize": {"x": 0, "y": 0, "width": 256, "height": 256},
            "sourceSize": {"width": 256, "height": 256},
            "pivot": {"x": 0.5, "y": 1.0},
        }
    manifest = {
        "schema": "sprite-atlas.v1",
        "id": "blocks-buttons-world",
        "image": "blocks-buttons-world-atlas.png",
        "size": {"width": 1024, "height": 512},
        "frames": frames,
        "animations": {},
    }
    encoded = io.BytesIO()
    atlas.save(encoded, format="PNG", optimize=False, compress_level=9)
    return encoded.getvalue(), (json.dumps(manifest, indent=2) + "\n").encode()


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
    atlas, manifest = outputs()
    write_or_check(ATLAS, atlas, args.check, image=True)
    write_or_check(MANIFEST, manifest, args.check)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
