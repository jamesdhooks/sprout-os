#!/usr/bin/env python3
"""Build the reviewed Blocks & Buttons robot atlases for Sprout and PICO-8."""

from __future__ import annotations

import argparse
import io
import json
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "games" / "blocks-buttons" / "assets-src" / "robot"
NATIVE_IMAGE = ROOT / "games" / "blocks-buttons" / "assets" / "rich-character.png"
NATIVE_MANIFEST = ROOT / "games" / "blocks-buttons" / "assets-src" / "rich-character-atlas.json"
PICO_CART = ROOT / "pico8" / "blocks-buttons" / "blocks-buttons.p8"
PICO_SOURCE = ROOT / "pico8" / "blocks-buttons" / "sprites.json"
PICO_ALLOWED = set("012456789acdf")

DIRECTIONS = ("south", "north", "west")
PICO_FRAME_NAMES = tuple(
    name
    for direction in DIRECTIONS
    for name in (
        *(f"hero-{direction}-{index:02d}" for index in range(1, 5)),
        f"hero-{direction}-push",
    )
)
NATIVE_FRAME_NAMES = tuple(
    name for direction in DIRECTIONS for name in (
        *(f"hero-{direction}-{index:02d}" for index in range(1, 5)),
        *(f"hero-{direction}-push-{index:02d}" for index in range(1, 5)),
    )
)
NATIVE_PALETTE = (
    "#102F5B", "#173B70", "#16579B", "#2584CE", "#39AFCC", "#66768C",
    "#A9BCC8", "#E4EEF0", "#8F2C36", "#D93932", "#EF654B", "#704327",
    "#B86B38", "#E4A35B", "#FFC53B", "#F6F0DF",
)
MASTER_SIZE = (128, 160)


def _south_robot(phase: int, pushing: bool) -> Image.Image:
    """Author one strict-overhead robot pose on a square rotation canvas."""
    image = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    navy, deep, blue, bright, cyan = NATIVE_PALETTE[:5]
    steel, silver, white = NATIVE_PALETTE[5:8]
    red, coral = NATIVE_PALETTE[9:11]

    # Rear tread pads alternate their highlight but never change the footprint.
    tread = steel if phase % 2 == 0 else deep
    draw.rounded_rectangle((34, 4, 52, 35), radius=5, fill=navy)
    draw.rounded_rectangle((76, 4, 94, 35), radius=5, fill=navy)
    draw.rectangle((38, 8, 50, 29), fill=tread)
    draw.rectangle((78, 8, 90, 29), fill=tread)

    # One overhead shell: no face, torso, or side plane.
    draw.rounded_rectangle((24, 18, 104, 116), radius=23, fill=navy)
    draw.rounded_rectangle((30, 23, 98, 112), radius=18, fill=bright)
    draw.rounded_rectangle((36, 29, 92, 106), radius=15, fill=blue)
    draw.polygon(((40, 31), (67, 25), (89, 34), (82, 43), (47, 43)), fill=cyan)

    # Side arm pods remain lateral in the idle/walk family.
    draw.rounded_rectangle((5, 45, 32, 91), radius=10, fill=navy)
    draw.rounded_rectangle((96, 45, 123, 91), radius=10, fill=navy)
    draw.rounded_rectangle((11, 51, 30, 85), radius=7, fill=silver)
    draw.rounded_rectangle((98, 51, 117, 85), radius=7, fill=silver)
    draw.rectangle((14, 58, 28, 76), fill=steel)
    draw.rectangle((100, 58, 114, 76), fill=steel)

    # Crown beacon and leading-edge navigation chevron.
    draw.ellipse((52, 43, 76, 67), fill=navy)
    draw.ellipse((57, 48, 71, 62), fill=red)
    draw.ellipse((60, 50, 67, 56), fill=coral)
    draw.polygon(((64, 103), (47, 88), (54, 82), (64, 91), (74, 82), (81, 88)), fill=white)

    if pushing:
        reach = (0, 3, 0, 5)[phase]
        draw.rounded_rectangle((34, 91, 51, 120 + reach), radius=6, fill=navy)
        draw.rounded_rectangle((77, 91, 94, 120 + reach), radius=6, fill=navy)
        draw.rectangle((39, 94, 48, 116 + reach), fill=silver)
        draw.rectangle((80, 94, 89, 116 + reach), fill=silver)
        draw.rectangle((36, 121 + reach, 51, 126), fill=white)
        draw.rectangle((77, 121 + reach, 92, 126), fill=white)
    return image


def _fallback_source_image(name: str) -> Image.Image:
    parts = name.split("-")
    if len(parts) not in (3, 4) or parts[0] != "hero":
        raise ValueError(f"invalid robot frame name: {name}")
    direction = parts[1]
    pushing = parts[2] == "push"
    phase = int(parts[3] if pushing else parts[2]) - 1
    if direction not in DIRECTIONS or phase not in range(4):
        raise ValueError(f"invalid robot frame name: {name}")
    square = _south_robot(phase, pushing)
    if direction == "north":
        square = square.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    elif direction == "west":
        square = square.transpose(Image.Transpose.ROTATE_270)
    image = Image.new("RGBA", MASTER_SIZE, (0, 0, 0, 0))
    image.alpha_composite(square, (0, MASTER_SIZE[1] - square.height))
    return image


def source_image(name: str) -> Image.Image:
    path = SOURCE / f"{name}.png"
    if not path.is_file():
        return _fallback_source_image(name)
    image = Image.open(path).convert("RGBA")
    if image.width < 64 or image.height < 64 or image.width > 512 or image.height > 512:
        raise ValueError(f"{path.name} has unsupported source dimensions {image.size}")
    if image.getbbox() is None:
        raise ValueError(f"{path.name} is blank")
    return image


def master_image(name: str) -> Image.Image:
    source = source_image(name)
    alpha = source.getchannel("A").point(lambda value: 255 if value >= 32 else 0)
    source.putalpha(alpha)
    bounds = alpha.getbbox()
    if bounds is None:
        raise ValueError(f"{name} has no opaque pixels")
    subject = source.crop(bounds)
    scale = min(118 / subject.width, 150 / subject.height)
    subject = subject.resize((max(1, round(subject.width * scale)),
                              max(1, round(subject.height * scale))),
                             Image.Resampling.NEAREST)
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
    atlas = Image.new("RGBA", (512, 960), (0, 0, 0, 0))
    frames: dict[str, object] = {}
    for index, name in enumerate(NATIVE_FRAME_NAMES):
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
            "frames": [f"hero-{direction}-push-{index:02d}" for index in range(1, 5)],
            "fps": 8, "loop": True,
        }
    manifest = {
        "schema": "sprite-atlas.v1",
        "id": "blocks-buttons-character",
        "image": "blocks-buttons-character-atlas.png",
        "size": {"width": 512, "height": 960},
        "frames": frames,
        "animations": animations,
    }
    encoded = io.BytesIO()
    atlas.save(encoded, format="PNG", optimize=False, compress_level=9)
    return encoded.getvalue(), (json.dumps(manifest, indent=2) + "\n").encode()


def pico_gfx() -> str:
    payload = json.loads(PICO_SOURCE.read_text(encoding="utf-8"))
    frames = payload.get("frames", {})
    for name, frame in frames.items():
        source_x, source_y, width, height = frame["rect"]
        rows = frame.get("pixels", [])
        if len(rows) != height or any(len(row) != width for row in rows):
            raise ValueError(f"{name} pixel rows must exactly match its {width}x{height} rectangle")
        colours = set("".join(rows))
        if not colours.issubset(PICO_ALLOWED):
            raise ValueError(f"{name} uses undeclared PICO colours: {sorted(colours - PICO_ALLOWED)}")
        if source_x < 0 or source_y < 0 or source_x + width > 128 or source_y + height > 128:
            raise ValueError(f"{name} rectangle escapes the 128x128 PICO sprite bank")
    for name in ("wall", "crate", "crate-solved"):
        if frames.get(name, {}).get("rect", [None] * 4)[2:] != [16, 16]:
            raise ValueError(f"{name} must remain a strict top-down 16x16 frame")
    for name in PICO_FRAME_NAMES:
        if frames.get(name, {}).get("rect", [None] * 4)[2:] != [16, 16]:
            raise ValueError(f"{name} must remain a bottom-anchored 16x16 robot frame")
        colours = set("".join(frames[name]["pixels"]))
        if not {"8", "6", "c"}.issubset(colours):
            raise ValueError(f"{name} must retain its red light, silver arms, and blue body")
    for name in ("button-up", "button-down"):
        colours = set("".join(frames[name]["pixels"]))
        if not {"8", "6"}.issubset(colours):
            raise ValueError(f"{name} must retain its red cap and silver housing")
    canvas = [["0"] * 128 for _ in range(128)]
    occupied: set[tuple[int, int]] = set()
    for name, frame in frames.items():
        source_x, source_y, width, height = frame["rect"]
        rows = frame["pixels"]
        if name.startswith("hero-"):
            direction = name.split("-")[1]
            target_y = {"south": 0, "north": 16, "west": 32}[direction]
            target_x, target_width, target_height = source_x, 16, 16
            output_rows = rows
        else:
            target_x, target_y = source_x, source_y
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
