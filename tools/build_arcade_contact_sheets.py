#!/usr/bin/env python3
"""Build local native-size and enlarged contact sheets from capture folders."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def make_sheet(files: list[Path], output: Path, thumb: tuple[int, int], columns: int) -> None:
    label_height = 28
    rows = (len(files) + columns - 1) // columns
    sheet = Image.new("RGB", (thumb[0] * columns, (thumb[1] + label_height) * rows), "#10182d")
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for index, path in enumerate(files):
        image = Image.open(path).convert("RGB")
        image.thumbnail(thumb, Image.Resampling.NEAREST)
        column, row = index % columns, index // columns
        x = column * thumb[0] + (thumb[0] - image.width) // 2
        y = row * (thumb[1] + label_height)
        sheet.paste(image, (x, y))
        label = f"{path.parent.name}/{path.stem}"
        draw.rectangle((column * thumb[0], y + thumb[1], (column + 1) * thumb[0], y + thumb[1] + label_height), fill="#202a49")
        draw.text((column * thumb[0] + 6, y + thumb[1] + 8), label, fill="#fff8e8", font=font)
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_root", type=Path)
    parser.add_argument("output_prefix", type=Path)
    parser.add_argument("--native-width", type=int, required=True)
    parser.add_argument("--native-height", type=int, required=True)
    parser.add_argument("--columns", type=int, default=4)
    args = parser.parse_args()
    files = sorted(
        path for path in args.capture_root.rglob("*")
        if path.parent != args.capture_root and path.suffix.lower() in {".png", ".bmp"}
    )
    if not files:
        raise SystemExit(f"no captures beneath {args.capture_root}")
    native = (args.native_width, args.native_height)
    make_sheet(files, args.output_prefix.with_name(args.output_prefix.name + "-native.png"), native, args.columns)
    make_sheet(files, args.output_prefix.with_name(args.output_prefix.name + "-enlarged.png"), (native[0] * 2, native[1] * 2), max(1, args.columns // 2))
    print(f"built contact sheets for {len(files)} captures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
