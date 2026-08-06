#!/usr/bin/env python3
"""Inject deterministic original SFX and a tiny music loop into Arcade carts."""

from __future__ import annotations

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CARTS = {
    ROOT / "pico8/mouse-cheese/mouse-cheese.p8": 4,
    ROOT / "pico8/blocks-buttons/blocks-buttons.p8": 7,
    ROOT / "pico8/snake/snake.p8": 5,
}


def sfx_line(index: int, music: bool = False) -> str:
    speed = 10 if music else max(3, 8 - index % 4)
    notes = []
    for step in range(32):
        active = step < (32 if music else 5 + index % 4)
        if not active:
            notes.append("00000")
            continue
        if music:
            pitch = (20, 23, 27, 30)[(step // 4 + index) % 4]
            volume, effect = 3, 0
        else:
            pitch = max(4, min(55, 18 + index * 3 + step * (1 if index % 2 == 0 else -1)))
            volume, effect = max(2, 7 - step), 1 if index % 3 == 0 else 0
        notes.append(f"{pitch:02x}{index % 6:x}{volume:x}{effect:x}")
    return f"00{speed:02x}0000" + "".join(notes)


def payload(cues: int) -> str:
    lines = [sfx_line(index) for index in range(cues)]
    lines.append(sfx_line(cues, music=True))
    return "__sfx__\n" + "\n".join(lines) + f"\n__music__\n00 {cues:02x}414243\n"


def updated(cart: Path, cues: int) -> bytes:
    text = cart.read_text(encoding="utf-8")
    audio = payload(cues)
    if "__sfx__\n" in text:
        text = text.split("__sfx__\n", 1)[0] + audio
    else:
        text = text.rstrip() + "\n" + audio
    return text.encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for cart, cues in CARTS.items():
        data = updated(cart, cues)
        if args.check:
            if cart.read_bytes() != data: raise SystemExit(f"generated PICO audio is stale: {cart}")
        else: cart.write_bytes(data)
    print("PICO-8 arcade audio passed")
    return 0


if __name__ == "__main__": raise SystemExit(main())
