#!/usr/bin/env python3
"""Compose a topology-safe Starlight Snake title from a generated backdrop."""

from __future__ import annotations

import argparse
import io
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

from pico8_title_assets import PALETTE, compile_pixels


NAVY = PALETTE[1]
DARK_GREEN = PALETTE[3]
BRIGHT_GREEN = PALETTE[11]
CREAM = PALETTE[7]
YELLOW = PALETTE[10]
PINK = PALETTE[14]

# One open centerline: tail -> body -> neck. It never crosses or branches.
CENTERLINE = ((10, 94), (26, 93), (39, 84), (53, 82), (66, 88), (78, 92), (89, 84))


def tapered_tail(draw: ImageDraw.ImageDraw, colour: tuple[int, int, int], radius: int) -> None:
    tail, body = CENTERLINE[0], CENTERLINE[1]
    draw.polygon((tail, (body[0], body[1] - radius), (body[0], body[1] + radius)), fill=colour)


def validate_topology() -> None:
    mask = Image.new("1", (128, 128), 0)
    draw = ImageDraw.Draw(mask)
    tail, body = CENTERLINE[0], CENTERLINE[1]
    draw.polygon((tail, (body[0], body[1] - 7), (body[0], body[1] + 7)), fill=1)
    draw.line(CENTERLINE[1:], fill=1, width=15, joint="curve")
    draw.ellipse((83, 75, 106, 94), fill=1)
    points = {(x, y) for y in range(128) for x in range(128) if mask.getpixel((x, y))}
    if not points:
        raise ValueError("snake silhouette is empty")
    visited = {next(iter(points))}
    pending = list(visited)
    while pending:
        x, y = pending.pop()
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                neighbour = (x + dx, y + dy)
                if neighbour in points and neighbour not in visited:
                    visited.add(neighbour)
                    pending.append(neighbour)
    if visited != points:
        raise ValueError("snake silhouette is disconnected")


def paint_snake(image: Image.Image) -> Image.Image:
    draw = ImageDraw.Draw(image)

    # Connected silhouette and body. Each narrower layer is drawn over the same
    # centerline, so palette accents cannot split the underlying snake.
    tapered_tail(draw, NAVY, 7)
    draw.line(CENTERLINE[1:], fill=NAVY, width=15, joint="curve")
    tapered_tail(draw, DARK_GREEN, 5)
    draw.line(CENTERLINE[1:], fill=DARK_GREEN, width=11, joint="curve")
    tapered_tail(draw, BRIGHT_GREEN, 2)
    highlight = tuple((x, y - 2) for x, y in CENTERLINE[1:])
    draw.line(highlight, fill=BRIGHT_GREEN, width=3, joint="curve")

    # The head overlaps the final neck point and is therefore part of the same
    # silhouette rather than a separately floating sprite.
    draw.ellipse((83, 75, 106, 94), fill=NAVY)
    draw.polygon(((88, 77), (101, 77), (108, 84), (104, 91), (89, 92), (84, 86)), fill=DARK_GREEN)
    draw.polygon(((89, 78), (99, 78), (104, 82), (91, 83)), fill=BRIGHT_GREEN)
    draw.rectangle((98, 80, 101, 84), fill=CREAM)
    draw.rectangle((100, 81, 101, 83), fill=NAVY)
    draw.point((104, 87), fill=NAVY)
    draw.line(((106, 89), (110, 90)), fill=PINK, width=1)

    # Sparse highlights follow the same unbranched body path.
    for x, y in ((31, 89), (44, 82), (58, 83), (71, 88), (82, 88)):
        draw.rectangle((x, y, x + 1, y + 1), fill=YELLOW)
    return image


def render(background: Path) -> Image.Image:
    validate_topology()
    _, plate = compile_pixels(background)
    composed = paint_snake(plate.copy())
    if not set(composed.getdata()).issubset(set(PALETTE)):
        raise ValueError("composed title contains a colour outside the PICO-8 palette")
    return composed.resize((1024, 1024), Image.Resampling.NEAREST)


def encoded(image: Image.Image) -> bytes:
    output = io.BytesIO()
    image.save(output, format="PNG", optimize=False, compress_level=9)
    return output.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--background", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = render(args.background)
    if args.check:
        if not args.output.exists():
            raise ValueError(f"{args.output}: composed title is missing")
        actual = Image.open(args.output).convert("RGB")
        if actual.size != expected.size or ImageChops.difference(actual, expected).getbbox() is not None:
            raise ValueError(f"{args.output}: composed title is stale")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(encoded(expected))
    print(args.output.resolve())


if __name__ == "__main__":
    main()
