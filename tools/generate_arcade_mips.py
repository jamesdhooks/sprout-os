#!/usr/bin/env python3
"""Generate deterministic nearest-neighbour mip atlases for native games."""

from __future__ import annotations

import argparse
import io
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ATLASES = tuple(
    ROOT / "games" / game / "assets" / name
    for game in ("mouse-maze", "blocks-buttons", "snake")
    for name in ("rich-character.png", "rich-world.png")
)
SCALES = (0.25, 0.5)


def derivative(path: Path, scale: float) -> bytes:
    source = Image.open(path).convert("RGBA")
    size = (round(source.width * scale), round(source.height * scale))
    result = source.resize(size, Image.Resampling.NEAREST)
    encoded = io.BytesIO(); result.save(encoded, format="PNG", compress_level=9)
    return encoded.getvalue()


def output_path(path: Path, scale: float) -> Path:
    return path.with_name(f"{path.stem}@{scale:g}x.png")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for atlas in ATLASES:
        if not atlas.is_file(): raise ValueError(f"missing native atlas: {atlas}")
        for scale in SCALES:
            destination, payload = output_path(atlas, scale), derivative(atlas, scale)
            if args.check:
                if not destination.is_file() or destination.read_bytes() != payload:
                    raise SystemExit(f"generated mip is stale: {destination}")
            else:
                destination.write_bytes(payload)
    print("arcade mip atlases passed")
    return 0


if __name__ == "__main__": raise SystemExit(main())
