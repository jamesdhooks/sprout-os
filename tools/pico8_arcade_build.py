#!/usr/bin/env python3
"""Inject the small shared Sprout Arcade lifecycle core into PICO-8 carts."""

from __future__ import annotations

import argparse
from pathlib import Path

BEGIN = "-- BEGIN SHARED ARCADE CORE"
END = "-- END SHARED ARCADE CORE"


def inject(cart: Path, shared: Path, check: bool) -> None:
    source = cart.read_text(encoding="utf-8")
    core = shared.read_text(encoding="utf-8").rstrip()
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError(f"{cart}: shared-core markers must occur exactly once")
    prefix, remainder = source.split(BEGIN, 1)
    _, suffix = remainder.split(END, 1)
    expected = f"{prefix}{BEGIN}\n{core}\n{END}{suffix}"
    if check:
        if source != expected:
            raise ValueError(f"{cart}: shared core is stale; run inject")
        required = (
            'qa_capture="normal"',
            "function _update60()",
            "function _draw()",
            'arc_begin_card("win"',
            "arc_update_card()",
            '__gfx__',
        )
        missing = [token for token in required if token not in source]
        if missing:
            raise ValueError(f"{cart}: missing release contract: {missing}")
        if source.count("cartdata(") != 1:
            raise ValueError(f"{cart}: expected one profile save marker")
    else:
        cart.write_text(expected, encoding="utf-8", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("inject", "validate"))
    parser.add_argument("carts", nargs="+", type=Path)
    parser.add_argument(
        "--shared", type=Path, default=Path("pico8/shared/arcade_core.lua")
    )
    args = parser.parse_args()
    for cart in args.carts:
        inject(cart, args.shared, check=args.command == "validate")
    print(f"{args.command} passed for {len(args.carts)} cart(s)")


if __name__ == "__main__":
    main()
