#!/usr/bin/env python3
"""Validate the shipped profile-avatar catalogue without imaging dependencies."""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def png_header(path: Path) -> tuple[int, int, int]:
    with path.open("rb") as stream:
        if stream.read(8) != PNG_SIGNATURE:
            raise ValueError(f"{path} is not a PNG")
        length = struct.unpack(">I", stream.read(4))[0]
        if stream.read(4) != b"IHDR" or length != 13:
            raise ValueError(f"{path} has no canonical IHDR")
        width, height, bit_depth, color_type, compression, filtering, interlace = (
            struct.unpack(">IIBBBBB", stream.read(13))
        )
        if (bit_depth, compression, filtering, interlace) != (8, 0, 0, 0):
            raise ValueError(f"{path} uses unsupported PNG encoding")
        return width, height, color_type


def validate(root: Path) -> None:
    catalogue_path = root / "catalogue.json"
    catalogue = json.loads(catalogue_path.read_text(encoding="utf-8"))
    if catalogue.get("schema") != "sprout.profile-avatar-catalogue.v1":
        raise ValueError("Unexpected profile-avatar catalogue schema")
    avatars = catalogue.get("avatars")
    if not isinstance(avatars, list) or len(avatars) != 32:
        raise ValueError("The built-in profile-avatar catalogue must contain 32 entries")

    ids: set[str] = set()
    expected: dict[str, set[str]] = {"masters": set(), "thumbs": set()}
    for entry in avatars:
        avatar_id = entry.get("id") if isinstance(entry, dict) else None
        label = entry.get("label") if isinstance(entry, dict) else None
        if not isinstance(avatar_id, str) or not avatar_id or avatar_id in ids:
            raise ValueError(f"Invalid or duplicate avatar id: {avatar_id!r}")
        if not all(character.islower() or character.isdigit() or character == "-" for character in avatar_id):
            raise ValueError(f"Unsafe avatar id: {avatar_id}")
        if not isinstance(label, str) or not label.strip():
            raise ValueError(f"Avatar {avatar_id} has no label")
        ids.add(avatar_id)
        for folder, size in (("masters", 512), ("thumbs", 128)):
            filename = f"{avatar_id}.png"
            expected[folder].add(filename)
            path = root / folder / filename
            width, height, color_type = png_header(path)
            if (width, height, color_type) != (size, size, 6):
                raise ValueError(
                    f"{path} must be {size}x{size} 8-bit RGBA; got "
                    f"{width}x{height}, color type {color_type}"
                )

    for folder in ("masters", "thumbs"):
        actual = {path.name for path in (root / folder).glob("*.png")}
        if actual != expected[folder]:
            raise ValueError(
                f"Unexpected {folder} files; missing={sorted(expected[folder] - actual)}, "
                f"extra={sorted(actual - expected[folder])}"
            )


if __name__ == "__main__":
    avatar_root = (
        Path(sys.argv[1])
        if len(sys.argv) > 1
        else Path(__file__).resolve().parents[1] / "launcher" / "assets" / "avatars"
    )
    validate(avatar_root)
    print("profile avatar assets: 32 masters and 32 thumbnails valid")
