#!/usr/bin/env python3
"""Validate Sprout Arcade palette contracts and emit exact prompt strips."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
GAMES = {
    "mouse-cheese": (ROOT / "game-design/mouse-cheese/palette.json", ROOT / "pico8/mouse-cheese/mouse-cheese.p8"),
    "blocks-buttons": (ROOT / "game-design/blocks-buttons/palette.json", ROOT / "pico8/blocks-buttons/blocks-buttons.p8"),
    "starlight-snake": (ROOT / "game-design/snake/palette.json", ROOT / "pico8/snake/snake.p8"),
}
HEX = re.compile(r"#[0-9A-F]{6}")


def load_contract(path: Path) -> dict[str, object]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    colors = payload.get("native")
    if payload.get("schemaVersion") != 1 or not isinstance(colors, list) or len(colors) != 16:
        raise ValueError(f"{path}: native palette must contain exactly 16 colors")
    if len(set(colors)) != 16 or any(not isinstance(value, str) or not HEX.fullmatch(value) for value in colors):
        raise ValueError(f"{path}: native colors must be unique uppercase #RRGGBB values")
    pico = payload.get("pico8")
    if not isinstance(pico, dict) or not pico.get("standardIndices"):
        raise ValueError(f"{path}: missing PICO-8 subset")
    indices = pico["standardIndices"]
    if any(not isinstance(value, int) or value < 0 or value > 15 for value in indices):
        raise ValueError(f"{path}: invalid PICO-8 index")
    return payload


def validate_cart(cart: Path, contract: dict[str, object]) -> None:
    text = cart.read_text(encoding="utf-8")
    try:
        gfx = text.split("__gfx__\n", 1)[1].split("__", 1)[0]
    except IndexError as error:
        raise ValueError(f"{cart}: missing __gfx__ section") from error
    used = {int(value, 16) for value in gfx.lower() if value in "0123456789abcdef"}
    allowed = set(contract["pico8"]["standardIndices"])
    allowed.update(int(value) for value in contract["pico8"].get("extendedRemaps", {}))
    undeclared = sorted(used - allowed)
    if undeclared:
        raise ValueError(f"{cart}: undeclared PICO-8 colors {undeclared}")


def write_strip(name: str, colors: list[str], directory: Path) -> None:
    cell = 64
    image = Image.new("RGB", (cell * 8, cell * 2), "white")
    draw = ImageDraw.Draw(image)
    for index, value in enumerate(colors):
        x, y = index % 8 * cell, index // 8 * cell
        draw.rectangle((x, y, x + cell - 1, y + cell - 1), fill=value)
        ink = "white" if sum(int(value[offset:offset + 2], 16) for offset in (1, 3, 5)) < 360 else "black"
        draw.text((x + 4, y + 44), value, fill=ink)
    directory.mkdir(parents=True, exist_ok=True)
    image.save(directory / f"{name}.png")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--strips", type=Path, help="write local prompt palette strips")
    args = parser.parse_args()
    for name, (palette, cart) in GAMES.items():
        contract = load_contract(palette)
        validate_cart(cart, contract)
        if args.strips:
            write_strip(name, contract["native"], args.strips)
    print("arcade palette contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
