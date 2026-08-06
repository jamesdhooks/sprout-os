#!/usr/bin/env python3
"""Compile an enlarged 128x128 pixel-art master into a PICO-8 screen payload."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

PALETTE = (
    (0, 0, 0), (29, 43, 83), (126, 37, 83), (0, 135, 81),
    (171, 82, 54), (95, 87, 79), (194, 195, 199), (255, 241, 232),
    (255, 0, 77), (255, 163, 0), (255, 236, 39), (0, 228, 54),
    (41, 173, 255), (131, 118, 156), (255, 119, 168), (255, 204, 170),
)
BEGIN = "-- BEGIN GENERATED TITLE"
END = "-- END GENERATED TITLE"


def nearest(rgb: tuple[int, int, int]) -> int:
    return min(range(16), key=lambda index: sum(
        (rgb[channel] - PALETTE[index][channel]) ** 2 for channel in range(3)
    ))


def compile_pixels(source: Path) -> tuple[list[int], Image.Image]:
    image = Image.open(source).convert("RGB")
    side = min(image.size)
    left, top = (image.width - side) // 2, (image.height - side) // 2
    # Title masters represent a native 128x128 pixel canvas enlarged for
    # review. Nearest-neighbour sampling preserves those authored pixel blocks;
    # a smoothing filter would turn the title back into a reduced illustration.
    image = image.crop((left, top, left + side, top + side)).resize(
        (128, 128), Image.Resampling.NEAREST
    )
    values = [nearest(image.getpixel((x, y))) for y in range(128) for x in range(128)]
    preview = Image.new("RGB", (128, 128))
    preview.putdata([PALETTE[value] for value in values])
    if not set(preview.getdata()).issubset(set(PALETTE)):
        raise ValueError("compiled title contains a colour outside the PICO-8 palette")
    return values, preview


def encode(values: list[int]) -> str:
    encoded: list[str] = []
    cursor = 0
    while cursor < len(values):
        end = cursor + 1
        while end < len(values) and values[end] == values[cursor] and end - cursor < 15:
            end += 1
        encoded.append(format(end - cursor, "x") + format(values[cursor], "x"))
        cursor = end
    return "".join(encoded)


def lua_chunks(value: str) -> str:
    return '"' + value[:96] + '"' + "".join(
        '\n.."' + value[index:index + 96] + '"'
        for index in range(96, len(value), 96)
    )


def generated_source(payload: str) -> str:
    return f"{BEGIN}\ntitle_data={lua_chunks(payload)}\n{END}"


def inject(cart: Path, payload: str, check: bool) -> None:
    text = cart.read_text(encoding="utf-8")
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        raise ValueError(f"{cart}: expected one generated-title marker pair")
    start = text.index(BEGIN)
    end = text.index(END, start) + len(END)
    expected = text[:start] + generated_source(payload) + text[end:]
    if check:
        if expected != text:
            raise ValueError(f"{cart}: generated title payload is stale")
    else:
        cart.write_text(expected, encoding="utf-8", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--cart", type=Path, required=True)
    parser.add_argument("--preview", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    values, preview = compile_pixels(args.source)
    payload = encode(values)
    if args.check:
        if not args.preview.exists() or list(Image.open(args.preview).convert("RGB").getdata()) != list(preview.getdata()):
            raise ValueError(f"{args.preview}: generated title preview is stale")
    else:
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        preview.save(args.preview)
    inject(args.cart, payload, args.check)
    print(f"{args.cart}: {len(payload)} title payload characters")


if __name__ == "__main__":
    main()
