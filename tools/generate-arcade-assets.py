#!/usr/bin/env python3
"""Generate Sprout Arcade's small deterministic RGBA sprite atlases."""

from __future__ import annotations

import argparse
import binascii
import struct
import sys
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
W, H = 128, 64
T = (0, 0, 0, 0)
C = {
    "ink": (15, 31, 24, 255),
    "deep": (29, 55, 43, 255),
    "green": (70, 132, 84, 255),
    "leaf": (83, 198, 126, 255),
    "cream": (239, 225, 171, 255),
    "sand": (204, 178, 119, 255),
    "brown": (142, 91, 55, 255),
    "dark_brown": (84, 54, 39, 255),
    "gold": (239, 166, 60, 255),
    "orange": (205, 103, 52, 255),
    "stone": (116, 125, 103, 255),
    "stone_hi": (157, 163, 130, 255),
    "red": (194, 71, 61, 255),
}


class Canvas:
    def __init__(self) -> None:
        self.pixels = [[T for _ in range(W)] for _ in range(H)]

    def pixel(self, x: int, y: int, color: tuple[int, int, int, int]) -> None:
        if 0 <= x < W and 0 <= y < H:
            self.pixels[y][x] = color

    def rect(self, x: int, y: int, width: int, height: int,
             color: tuple[int, int, int, int]) -> None:
        for py in range(y, y + height):
            for px in range(x, x + width):
                self.pixel(px, py, color)

    def rows(self, x: int, y: int, rows: list[tuple[int, int]],
             color: tuple[int, int, int, int]) -> None:
        for offset, (start, length) in enumerate(rows):
            self.rect(x + start, y + offset, length, 1, color)


def chunk(kind: bytes, data: bytes) -> bytes:
    return (struct.pack(">I", len(data)) + kind + data +
            struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF))


def deterministic_zlib(data: bytes) -> bytes:
    """Encode DEFLATE stored blocks so bytes do not depend on zlib versions."""
    output = bytearray(b"\x78\x01")
    for offset in range(0, len(data), 65535):
        block = data[offset:offset + 65535]
        final = offset + len(block) == len(data)
        output.append(1 if final else 0)
        output.extend(struct.pack("<H", len(block)))
        output.extend(struct.pack("<H", 0xFFFF ^ len(block)))
        output.extend(block)
    output.extend(struct.pack(">I", zlib.adler32(data) & 0xFFFFFFFF))
    return bytes(output)


def encode_png(canvas: Canvas) -> bytes:
    scanlines = bytearray()
    for row in canvas.pixels:
        scanlines.append(0)
        for pixel in row:
            scanlines.extend(pixel)
    header = struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) +
            chunk(b"IDAT", deterministic_zlib(bytes(scanlines))) +
            chunk(b"IEND", b""))


def floor_tile(c: Canvas, x: int, y: int, base: str = "sand") -> None:
    c.rect(x, y, 16, 16, C[base])
    c.rect(x, y, 16, 1, C["cream"] if base == "sand" else C["stone_hi"])
    c.rect(x, y, 1, 16, C["cream"] if base == "sand" else C["stone_hi"])
    c.pixel(x + 4, y + 5, C["deep"])
    c.pixel(x + 11, y + 11, C["deep"])


def wall_tile(c: Canvas, x: int, y: int, leafy: bool = True) -> None:
    c.rect(x, y, 16, 16, C["deep"])
    c.rect(x + 1, y + 1, 14, 14, C["green"] if leafy else C["stone"])
    c.rect(x + 2, y + 2, 12, 3, C["leaf"] if leafy else C["stone_hi"])
    c.rect(x + 3, y + 8, 10, 2, C["deep"])
    c.pixel(x + 4, y + 12, C["ink"])
    c.pixel(x + 11, y + 6, C["ink"])


def cheese(c: Canvas, x: int, y: int, sparkle: bool) -> None:
    c.rows(x, y, [(7, 2), (5, 6), (3, 10), (2, 12), (2, 12),
                   (2, 12), (3, 10), (4, 8)], C["gold"])
    c.rect(x + 3, y + 6, 10, 2, C["brown"])
    c.rect(x + 7, y + 3, 2, 2, C["orange"])
    c.pixel(x + 11, y + 5, C["orange"])
    if sparkle:
        c.pixel(x + 2, y + 1, C["cream"])
        c.pixel(x + 1, y + 2, C["cream"])
        c.pixel(x + 3, y + 2, C["cream"])
        c.pixel(x + 2, y + 3, C["cream"])


def mouse(c: Canvas, x: int, y: int, direction: str, step: int) -> None:
    # A compact top-down silhouette. Direction changes the nose, ears, and eyes.
    c.rows(x, y, [(6, 4), (4, 8), (3, 10), (2, 12), (2, 12),
                   (2, 12), (3, 10), (4, 8), (5, 6)], C["dark_brown"])
    c.rows(x, y + 1, [(6, 4), (4, 8), (3, 10), (3, 10),
                       (3, 10), (4, 8), (5, 6)], C["brown"])
    if direction == "up":
        c.rect(x + 4, y, 3, 3, C["cream"]); c.rect(x + 9, y, 3, 3, C["cream"])
        c.pixel(x + 6, y + 3, C["ink"]); c.pixel(x + 9, y + 3, C["ink"])
        c.pixel(x + 7, y, C["cream"]); c.pixel(x + 8, y, C["cream"])
    elif direction == "down":
        c.rect(x + 4, y + 2, 3, 3, C["cream"]); c.rect(x + 9, y + 2, 3, 3, C["cream"])
        c.pixel(x + 6, y + 6, C["ink"]); c.pixel(x + 9, y + 6, C["ink"])
        c.pixel(x + 7, y + 9, C["cream"]); c.pixel(x + 8, y + 9, C["cream"])
    elif direction == "left":
        c.rect(x + 4, y + 1, 3, 3, C["cream"]); c.rect(x + 4, y + 6, 3, 3, C["cream"])
        c.pixel(x + 4, y + 4, C["ink"]); c.pixel(x + 4, y + 6, C["ink"])
        c.pixel(x + 1, y + 5, C["cream"])
    else:
        c.rect(x + 9, y + 1, 3, 3, C["cream"]); c.rect(x + 9, y + 6, 3, 3, C["cream"])
        c.pixel(x + 11, y + 4, C["ink"]); c.pixel(x + 11, y + 6, C["ink"])
        c.pixel(x + 14, y + 5, C["cream"])
    foot_y = y + 10 + step
    c.rect(x + 4, foot_y, 2, 2, C["cream"])
    c.rect(x + 10, y + 11 - step, 2, 2, C["cream"])
    c.pixel(x + 7, y + 13, C["brown"]); c.pixel(x + 7, y + 14, C["brown"])


def sprout_player(c: Canvas, x: int, y: int, direction: str, step: int) -> None:
    c.rect(x + 5, y + 1, 2, 4, C["green"])
    c.rect(x + 9, y, 2, 5, C["green"])
    c.rect(x + 3, y + 1, 4, 2, C["leaf"])
    c.rect(x + 9, y, 4, 2, C["leaf"])
    c.rows(x, y + 4, [(5, 6), (3, 10), (2, 12), (2, 12), (2, 12),
                       (3, 10), (4, 8), (4, 8)], C["cream"])
    if direction == "left":
        c.pixel(x + 4, y + 7, C["ink"]); c.pixel(x + 4, y + 9, C["ink"])
    elif direction == "right":
        c.pixel(x + 11, y + 7, C["ink"]); c.pixel(x + 11, y + 9, C["ink"])
    else:
        c.pixel(x + 6, y + 8, C["ink"]); c.pixel(x + 9, y + 8, C["ink"])
    c.rect(x + 4 + step, y + 13, 3, 2, C["green"])
    c.rect(x + 9 - step, y + 13, 3, 2, C["green"])


def crate(c: Canvas, x: int, y: int, active: bool = False) -> None:
    c.rect(x + 1, y + 1, 14, 14, C["dark_brown"])
    c.rect(x + 2, y + 2, 12, 12, C["gold"] if active else C["brown"])
    for i in range(3, 13):
        c.pixel(x + i, y + i, C["cream"] if active else C["sand"])
        c.pixel(x + 15 - i, y + i, C["cream"] if active else C["sand"])
    c.rect(x + 2, y + 2, 12, 1, C["cream"] if active else C["gold"])


def button(c: Canvas, x: int, y: int, active: bool = False) -> None:
    floor_tile(c, x, y, "stone")
    c.rows(x, y + 5, [(5, 6), (3, 10), (2, 12), (3, 10), (5, 6)],
           C["leaf"] if active else C["orange"])
    c.rect(x + 5, y + 6, 6, 2, C["cream"] if active else C["gold"])


def retry(c: Canvas, x: int, y: int) -> None:
    c.rect(x + 3, y + 3, 10, 2, C["red"])
    c.rect(x + 3, y + 3, 2, 9, C["red"])
    c.rect(x + 4, y + 10, 8, 2, C["red"])
    c.rect(x + 10, y + 8, 2, 4, C["red"])
    c.rect(x + 1, y + 2, 4, 2, C["red"])
    c.rect(x + 2, y + 1, 2, 4, C["red"])


def build_mouse() -> bytes:
    c = Canvas()
    floor_tile(c, 0, 0); wall_tile(c, 16, 0); cheese(c, 32, 0, False); cheese(c, 48, 0, True)
    directions = ["down", "up", "left", "right"]
    for column, direction in enumerate(directions):
        mouse(c, column * 32, 16, direction, 0)
        mouse(c, column * 32 + 16, 16, direction, 1)
    return encode_png(c)


def build_blocks() -> bytes:
    c = Canvas()
    floor_tile(c, 0, 0, "stone"); wall_tile(c, 16, 0, False)
    button(c, 32, 0, False); button(c, 48, 0, True)
    crate(c, 64, 0, False); crate(c, 80, 0, True); retry(c, 96, 0)
    directions = ["down", "up", "left", "right"]
    for column, direction in enumerate(directions):
        sprout_player(c, column * 32, 16, direction, 0)
        sprout_player(c, column * 32 + 16, 16, direction, 1)
    return encode_png(c)


OUTPUTS = {
    ROOT / "games" / "mouse-maze" / "assets" / "sprites.png": build_mouse,
    ROOT / "games" / "blocks-buttons" / "assets" / "sprites.png": build_blocks,
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true",
                        help="fail if tracked atlases differ from generated bytes")
    args = parser.parse_args()
    stale: list[Path] = []
    for path, builder in OUTPUTS.items():
        expected = builder()
        if args.check:
            if not path.exists() or path.read_bytes() != expected:
                stale.append(path)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(expected)
            print(path.relative_to(ROOT))
    if stale:
        for path in stale:
            print(f"stale arcade atlas: {path.relative_to(ROOT)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
