"""Compile hand-authored hexadecimal pixel grids into a PICO-8 graphics bank."""

from __future__ import annotations

import json
from pathlib import Path


def compile_gfx(source: Path) -> str:
    payload = json.loads(source.read_text(encoding="utf-8"))
    if payload.get("schemaVersion") != 1 or payload.get("canvas") != [128, 128]:
        raise ValueError(f"{source}: expected hand-authored PICO sprite source v1")
    canvas = [["0"] * 128 for _ in range(128)]
    occupied: set[tuple[int, int]] = set()
    for name, frame in payload.get("frames", {}).items():
        rect = frame.get("rect")
        rows = frame.get("pixels")
        if not isinstance(rect, list) or len(rect) != 4 or not isinstance(rows, list):
            raise ValueError(f"{source}: malformed frame {name}")
        x, y, width, height = rect
        if width <= 0 or height <= 0 or x < 0 or y < 0 or x + width > 128 or y + height > 128:
            raise ValueError(f"{source}: frame {name} is outside the graphics bank")
        if len(rows) != height or any(len(row) != width for row in rows):
            raise ValueError(f"{source}: frame {name} does not match {width}x{height}")
        for dy, row in enumerate(rows):
            for dx, value in enumerate(row.lower()):
                if value not in "0123456789abcdef":
                    raise ValueError(f"{source}: frame {name} contains non-palette pixel {value!r}")
                point = (x + dx, y + dy)
                if point in occupied:
                    raise ValueError(f"{source}: frame {name} overlaps another frame at {point}")
                occupied.add(point)
                canvas[y + dy][x + dx] = value
    if not occupied:
        raise ValueError(f"{source}: no frames")
    return "\n".join("".join(row) for row in canvas)
