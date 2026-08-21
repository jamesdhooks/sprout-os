#!/usr/bin/env python3
"""Build deterministic, original PCM sound cues for Sprout Arcade."""

from __future__ import annotations

import argparse
import hashlib
import math
import random
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RATE = 22050

CUES = {
    "mouse-maze": {
        "step": (0.055, 260, 330, "sine"),
        "hint": (0.24, 520, 840, "sparkle"),
        "cheese": (0.55, 420, 1040, "sparkle"),
        "reset": (0.30, 420, 180, "sine"),
    },
    "blocks-buttons": {
        "push": (0.12, 135, 90, "square"),
        "button": (0.18, 380, 610, "sine"),
        "blocked": (0.12, 110, 75, "square"),
        "undo": (0.16, 420, 260, "sine"),
        "win": (0.55, 330, 880, "sparkle"),
        "fail": (0.38, 210, 95, "square"),
        "reset": (0.30, 420, 180, "sine"),
    },
    "snake": {
        "eat": (0.12, 520, 760, "sine"),
        "speed": (0.22, 580, 980, "sparkle"),
        "win": (0.58, 390, 1120, "sparkle"),
        "collision": (0.32, 180, 70, "square"),
        "reset": (0.30, 420, 180, "sine"),
    },
}


def sample(kind: str, phase: float, rng: random.Random) -> float:
    if kind == "square":
        return 0.72 if math.sin(phase) >= 0 else -0.72
    if kind == "sparkle":
        return 0.58 * math.sin(phase) + 0.22 * math.sin(phase * 2.01) + 0.05 * (rng.random() * 2 - 1)
    return 0.78 * math.sin(phase)


def render(duration: float, start: float, end: float, kind: str, seed: int) -> bytes:
    rng = random.Random(seed)
    frames = max(1, round(duration * RATE))
    phase = 0.0
    encoded = bytearray()
    for index in range(frames):
        t = index / max(1, frames - 1)
        frequency = start + (end - start) * (t * t * (3 - 2 * t))
        phase += math.tau * frequency / RATE
        attack = min(1.0, index / max(1, RATE * 0.012))
        release = min(1.0, (frames - index) / max(1, RATE * 0.075))
        value = max(-1.0, min(1.0, sample(kind, phase, rng) * attack * release * 0.55))
        encoded += struct.pack("<h", round(value * 32767))
    return bytes(encoded)


def build(check: bool) -> None:
    stale: list[str] = []
    for game, cues in CUES.items():
        output = ROOT / "games" / game / "assets" / "sfx"
        output.mkdir(parents=True, exist_ok=True)
        for name, values in cues.items():
            seed = int.from_bytes(hashlib.sha256(f"{game}:{name}".encode()).digest()[:4], "little")
            pcm = render(*values, seed)
            target = output / f"{name}.wav"
            temporary = target.with_suffix(".tmp.wav")
            with wave.open(str(temporary), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(RATE)
                wav.writeframes(pcm)
            generated = temporary.read_bytes()
            temporary.unlink()
            if check:
                if not target.exists() or target.read_bytes() != generated:
                    stale.append(str(target.relative_to(ROOT)))
            else:
                target.write_bytes(generated)
    if stale:
        raise SystemExit("stale generated audio:\n" + "\n".join(stale))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    build(parser.parse_args().check)
