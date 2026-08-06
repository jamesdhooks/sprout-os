#!/usr/bin/env python3
"""Build the reviewed Blocks & Buttons high-angle world atlas."""

from __future__ import annotations

import argparse
import io
import json
from pathlib import Path

from PIL import Image, ImageEnhance


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "games/blocks-buttons/assets-src/world"
ATLAS = ROOT / "games/blocks-buttons/assets/rich-world.png"
MANIFEST = ROOT / "games/blocks-buttons/assets-src/rich-world-atlas.json"
FRAME_NAMES = (
    "floor-wood", "wall-workshop", "crate-idle",
    "crate-solved", "button-up", "button-down",
)


def source(name: str) -> Image.Image:
    path = SOURCE / f"{name}.png"
    if not path.is_file():
        raise ValueError(f"missing reviewed world frame: {path}")
    image = Image.open(path).convert("RGBA")
    if image.size != (256, 256):
        raise ValueError(f"{path.name} must be 256x256, got {image.size}")
    return image


def solved_crate() -> Image.Image:
    image = source("crate-idle")
    color = ImageEnhance.Color(image.convert("RGB")).enhance(1.15).convert("RGBA")
    color.putalpha(image.getchannel("A"))
    overlay = Image.new("RGBA", image.size, (255, 190, 55, 0))
    overlay.putalpha(image.getchannel("A").point(lambda alpha: 38 if alpha else 0))
    return Image.alpha_composite(color, overlay)


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
    images = {
        "floor-wood": source("floor-wood"),
        "wall-workshop": source("wall-workshop"),
        "crate-idle": source("crate-idle"),
        "crate-solved": solved_crate(),
        "button-up": source("button-up"),
        "button-down": pressed_button(),
    }
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
