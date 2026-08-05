#!/usr/bin/env python3
"""Capture deterministic PICO-8 review states without changing a release cart."""

from __future__ import annotations

import argparse
import ctypes
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

from PIL import ImageGrab


def find_window(title: str, timeout: float = 10.0) -> int:
    user32 = ctypes.windll.user32
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        handle = user32.FindWindowW(None, title)
        if handle:
            return handle
        time.sleep(0.1)
    raise RuntimeError(f"PICO-8 window did not appear: {title}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("cart", type=Path)
    parser.add_argument("state", choices=("title", "gameplay", "win", "fail"))
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--pico8",
        type=Path,
        default=Path(r"C:\Program Files (x86)\PICO-8\pico8.exe"),
    )
    args = parser.parse_args()
    source = args.cart.read_text(encoding="utf-8")
    marker = 'qa_capture="normal"'
    if source.count(marker) != 1:
        raise ValueError(f"{args.cart}: missing unique QA capture marker")
    with tempfile.TemporaryDirectory(prefix="sprout-pico-") as temporary:
        cart = Path(temporary) / f"{args.cart.stem}-{args.state}.p8"
        cart.write_text(
            source.replace(marker, f'qa_capture="{args.state}"'),
            encoding="utf-8",
            newline="\n",
        )
        subprocess.run(
            ["taskkill", "/IM", "pico8.exe", "/F"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        process = subprocess.Popen(
            [
                str(args.pico8),
                "-windowed",
                "1",
                "-width",
                "512",
                "-height",
                "512",
                "-run",
                str(cart),
            ]
        )
        try:
            title = f"{cart.name.upper()} (PICO-8)"
            handle = find_window(title)
            # PICO-8 creates the window before compiling and running the cart.
            time.sleep(3.0)
            args.output.parent.mkdir(parents=True, exist_ok=True)
            ImageGrab.grab(window=handle).save(args.output)
        finally:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
    print(args.output.resolve())


if __name__ == "__main__":
    main()
