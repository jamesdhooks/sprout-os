#!/usr/bin/env python3
"""Build exact-palette 32-pixel native Starlight Snake world art."""

from __future__ import annotations

import argparse
import io
import json
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ATLAS = ROOT / "games/snake/assets/rich-world.png"
MANIFEST = ROOT / "games/snake/assets-src/rich-world-atlas.json"
SCALE = 8
TILE = 32
C = {
    "night": "#201B52", "grass": "#3D7662", "grass2": "#78AA74",
    "mint": "#8DE5C2", "cream": "#F7EBC9", "gold": "#EFB63E",
    "light": "#FFDE70", "brown": "#B8772B", "pink": "#C83B68",
    "pink2": "#EE6A91", "outline": "#161338", "leaf": "#244A48",
}
NAMES = ("grass-plain", "grass-clover", "grass-flower", "grass-pebble",
         *(f"apple-{index:02d}" for index in range(1, 5)),
         *(f"pear-{index:02d}" for index in range(1, 5)),
         *(f"eat-burst-{index:02d}" for index in range(1, 5)))


def grass(kind: str) -> Image.Image:
    image = Image.new("RGBA", (TILE, TILE), C["grass"])
    draw = ImageDraw.Draw(image)
    for x, y in ((3, 5), (14, 3), (25, 7), (7, 20), (21, 25), (29, 16)):
        draw.rectangle((x, y, x + 2, y + 1), fill=C["leaf"])
    if kind == "clover":
        draw.ellipse((10, 9, 16, 15), fill=C["grass2"])
        draw.ellipse((16, 9, 22, 15), fill=C["grass2"])
        draw.ellipse((13, 14, 19, 20), fill=C["grass2"])
        draw.line((16, 18, 19, 25), fill=C["mint"], width=1)
    elif kind == "flower":
        draw.line((16, 16, 16, 26), fill=C["leaf"], width=2)
        for box in ((12, 9, 17, 15), (16, 9, 21, 15), (14, 6, 19, 12)):
            draw.ellipse(box, fill=C["pink2"])
        draw.rectangle((15, 11, 18, 14), fill=C["light"])
    elif kind == "pebble":
        draw.ellipse((9, 14, 22, 23), fill=C["outline"])
        draw.ellipse((10, 14, 21, 21), fill=C["cream"])
    return image


def fruit(kind: str, frame: int) -> Image.Image:
    image = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    bob = (0, -1, 0, 1)[frame]
    draw.rectangle((15, 3 + bob, 17, 8 + bob), fill=C["brown"])
    draw.polygon(((17, 5 + bob), (24, 3 + bob), (21, 9 + bob)), fill=C["leaf"])
    if kind == "apple":
        draw.ellipse((5, 7 + bob, 27, 29 + bob), fill=C["outline"])
        draw.ellipse((7, 9 + bob, 25, 27 + bob), fill=C["pink"])
        draw.rectangle((10, 11 + bob, 14, 14 + bob), fill=C["pink2"])
    else:
        draw.ellipse((11, 6 + bob, 21, 18 + bob), fill=C["outline"])
        draw.ellipse((5, 13 + bob, 27, 30 + bob), fill=C["outline"])
        draw.ellipse((13, 8 + bob, 19, 18 + bob), fill=C["light"])
        draw.ellipse((7, 15 + bob, 25, 28 + bob), fill=C["light"])
    return image


def burst(frame: int) -> Image.Image:
    image = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    distance = 3 + frame * 3
    radius = max(1, 3 - frame // 2)
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1)):
        x, y = 16 + dx * distance, 16 + dy * distance
        draw.ellipse((x - radius, y - radius, x + radius, y + radius),
                     fill=C["light"] if frame < 2 else C["pink2"])
    return image


def frames() -> dict[str, Image.Image]:
    result = {f"grass-{kind}": grass(kind) for kind in ("plain", "clover", "flower", "pebble")}
    for kind in ("apple", "pear"):
        for frame in range(4): result[f"{kind}-{frame + 1:02d}"] = fruit(kind, frame)
    for frame in range(4): result[f"eat-burst-{frame + 1:02d}"] = burst(frame)
    return result


def outputs() -> tuple[bytes, bytes]:
    images = frames()
    atlas = Image.new("RGBA", (TILE * SCALE * 4, TILE * SCALE * 4), (0, 0, 0, 0))
    metadata = {}
    for index, name in enumerate(NAMES):
        x, y = index % 4 * TILE * SCALE, index // 4 * TILE * SCALE
        atlas.alpha_composite(images[name].resize((TILE * SCALE, TILE * SCALE), Image.Resampling.NEAREST), (x, y))
        metadata[name] = {
            "frame": {"x": x, "y": y, "width": TILE * SCALE, "height": TILE * SCALE},
            "rotated": False, "trimmed": False,
            "spriteSourceSize": {"x": 0, "y": 0, "width": TILE * SCALE, "height": TILE * SCALE},
            "sourceSize": {"width": TILE * SCALE, "height": TILE * SCALE},
            "pivot": {"x": 0.5, "y": 0.5},
        }
    animations = {
        "apple-idle": {"frames": [f"apple-{i:02d}" for i in range(1, 5)], "fps": 6, "loop": True},
        "pear-idle": {"frames": [f"pear-{i:02d}" for i in range(1, 5)], "fps": 6, "loop": True},
        "eat-burst": {"frames": [f"eat-burst-{i:02d}" for i in range(1, 5)], "fps": 12, "loop": False},
    }
    manifest = {"schema": "sprite-atlas.v1", "id": "snake-world",
                "image": "snake-world-atlas.png",
                "size": {"width": atlas.width, "height": atlas.height},
                "frames": metadata, "animations": animations}
    encoded = io.BytesIO(); atlas.save(encoded, format="PNG", compress_level=9)
    return encoded.getvalue(), (json.dumps(manifest, indent=2) + "\n").encode()


def emit(path: Path, data: bytes, check: bool) -> None:
    if check:
        if not path.is_file() or path.read_bytes() != data:
            raise SystemExit(f"generated asset is stale: {path}")
    else:
        path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("--check", action="store_true")
    args = parser.parse_args(); atlas, manifest = outputs()
    emit(ATLAS, atlas, args.check); emit(MANIFEST, manifest, args.check)
    return 0


if __name__ == "__main__": raise SystemExit(main())
