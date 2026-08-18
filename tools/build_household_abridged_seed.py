#!/usr/bin/env python3
"""Derive the fast-to-deploy household library from the private full seed.

The selection preserves a broad, kid-friendly cross-section while omitting the
large long-tail CD and PlayStation catalogue.  It is intentionally expressed by
the seed-facing display titles so its memberships stay aligned with Sprout's
catalogue and artwork payload.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


SELECTED_TITLES = {
    "ARCADE": {
        "Blocks & Buttons", "Bubble Bobble (US, Ver 1.0)",
        "Galaga (Namco rev. B)", "Mouse & Cheese Maze", "Out Run (bootleg)",
        "Pac-Man (Midway)", "Sprout Snake", "The Simpsons (2 Players World, set 1)",
    },
    "GB": {
        "Donkey Kong (Japan, USA) (SGB Enhanced)", "Kirby's Dream Land (USA, Europe)",
        "Super Mario Land 2 - 6 Golden Coins (USA, Europe) (Rev 2)", "Tetris (World) (Rev 1)",
    },
    "GBA": {
        "Kirby - Nightmare in Dream Land (USA)", "Legend of Zelda, The - The Minish Cap (USA)",
        "Mario Kart - Super Circuit (USA)", "Pokemon - Emerald Version (USA, Europe)",
        "WarioWare, Inc. - Mega Microgame$! (USA)",
    },
    "GBC": {"Mario Tennis (USA)", "Pokemon - Crystal Version (USA, Europe) (Rev 1)", "Wario Land 3 (World) (En,Ja)"},
    "GEN": {"Gunstar Heroes (USA)", "Rocket Knight Adventures (USA)", "Sonic & Knuckles + Sonic The Hedgehog 3 (USA)", "Sonic The Hedgehog 2 (World) (Rev B)"},
    "NEOGEO": {"Bust-a-Move", "Neo Turf Masters", "Windjammers"},
    "NES": {"Dr. Mario (Japan, USA) (Rev 1)", "DuckTales (USA)", "Kirby's Adventure (USA) (Rev 1)", "Legend of Zelda, The (USA) (Rev 1)", "Super Mario Bros. 3 (USA) (Rev 1)"},
    "PCE": {"Bomberman '94 (Japan)", "Bonk's Revenge (USA)"},
    "PICO": {"Alpine Alpaca", "Celeste", "Combo Pool", "Dusk Child", "Golf Sunday", "Just One Boss", "Mouse Maze", "Pico Driller", "Pico Tennis", "Pico Tetris", "Pullfrog", "Tomb of G'Nir"},
    "PS": {"Crash Bandicoot - Warped (USA)", "Harvest Moon - Back to Nature (USA)"},
    "SCD": {"Sonic CD (USA) (RE125)"},
    "SFC": {"Donkey Kong Country 2 - Diddy's Kong Quest (USA) (En,Fr) (Rev 1)", "Kirby Super Star (USA)", "Legend of Zelda, The - A Link to the Past (USA)", "Super Mario Kart (USA)", "Super Mario World (USA)", "Super Mario World 2 - Yoshi's Island (USA) (Rev 1)", "Tetris Attack (USA) (En,Ja)"},
}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=Path("config/household-seed.json"))
    parser.add_argument("--output", type=Path, default=Path("out/household-seed-abridged.json"))
    args = parser.parse_args()

    seed = json.loads(args.source.read_text(encoding="utf-8"))
    for profile in seed["profiles"]:
        for field in ("curatedItems", "favoriteItems"):
            profile[field] = [
                item for item in profile.get(field, [])
                if item["title"] in SELECTED_TITLES.get(item["platform"], set())
            ]
    actual = {
        (item["platform"], item["title"])
        for profile in seed["profiles"]
        for field in ("curatedItems", "favoriteItems")
        for item in profile.get(field, [])
    }
    requested = {(platform, title) for platform, titles in SELECTED_TITLES.items() for title in titles}
    missing = requested - actual
    if missing:
        raise RuntimeError(f"Selected titles missing from source seed: {sorted(missing)}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(seed, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"uniqueTitles": len(actual), "profiles": [
        {"name": profile["displayName"], "curated": len(profile.get("curatedItems", [])), "favorites": len(profile.get("favoriteItems", []))}
        for profile in seed["profiles"]
    ]}, indent=2))


if __name__ == "__main__":
    main()
