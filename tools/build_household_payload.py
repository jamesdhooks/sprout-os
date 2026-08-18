#!/usr/bin/env python3
"""Build the selective Onion ROM/art payload used by the household seed."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import urllib.parse
import xml.etree.ElementTree as ET
import zipfile
from collections import defaultdict
from pathlib import Path, PurePosixPath


SYSTEM_FOLDERS = {
    "NES": "FC",
    "SFC": "SFC",
    "GB": "GB",
    "GBC": "GBC",
    "GBA": "GBA",
    "GEN": "MD",
    "SCD": "SEGACD",
    "PCE": "PCE",
    "NEOGEO": "NEOGEO",
    "ARCADE": "ARCADE",
    "PS": "PS",
    "PICO": "PICO",
}

SHORT_NAMES = {
    ("ARCADE", "Bubble Bobble (US, Ver 1.0)"): "bubbobr1",
    ("ARCADE", "Galaga (Namco rev. B)"): "galaga",
    ("ARCADE", "Ms. Pac-Man"): "mspacman",
    ("ARCADE", "Out Run (bootleg)"): "outrunb",
    ("ARCADE", "Pac-Man (Midway)"): "pacman",
    ("ARCADE", "The Simpsons (2 Players World, set 1)"): "simpsn2p",
    ("NEOGEO", "Bust-a-Move"): "pbobblen",
    ("NEOGEO", "Metal Slug X - Super Vehicle-001"): "mslugx",
    ("NEOGEO", "Neo Turf Masters"): "turfmast",
    ("NEOGEO", "Windjammers"): "wjammers",
}

NATIVE_TITLES = {
    ("ARCADE", "Blocks & Buttons"),
    ("ARCADE", "Mouse & Cheese Maze"),
    ("ARCADE", "Sprout Snake"),
}

BIOS_BY_PLATFORM = {
    "PS": {"BIOS/PSXONPSP660.bin"},
    "SCD": {"BIOS/bios_CD_E.bin", "BIOS/bios_CD_J.bin", "BIOS/bios_CD_U.bin"},
}


def unique_seed_items(seed_path: Path) -> list[tuple[str, str]]:
    seed = json.loads(seed_path.read_text(encoding="utf-8"))
    items = {
        (item["platform"], item["title"])
        for profile in seed["profiles"]
        for item in profile.get("curatedItems", []) + profile.get("favoriteItems", [])
    }
    return sorted(items, key=lambda item: (item[0], item[1].casefold()))


def zip_index(archives: list[Path]) -> tuple[dict[str, tuple[Path, zipfile.ZipInfo]], dict[str, list[str]]]:
    exact: dict[str, tuple[Path, zipfile.ZipInfo]] = {}
    by_parent: dict[str, list[str]] = defaultdict(list)
    for archive in archives:
        with zipfile.ZipFile(archive) as source:
            for info in source.infolist():
                if info.is_dir():
                    continue
                normalized = info.filename.replace("\\", "/")
                exact.setdefault(normalized.casefold(), (archive, info))
                by_parent[str(PurePosixPath(normalized).parent).casefold()].append(normalized)
    return exact, by_parent


def extract_entry(archive: Path, info: zipfile.ZipInfo, output: Path, destination: str) -> int:
    target = output / PurePosixPath(destination)
    target.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as source, source.open(info) as reader, target.open("wb") as writer:
        shutil.copyfileobj(reader, writer, length=1024 * 1024)
    return info.file_size


def find_rom(
    platform: str,
    title: str,
    by_parent: dict[str, list[str]],
    exact: dict[str, tuple[Path, zipfile.ZipInfo]],
) -> str:
    folder = SYSTEM_FOLDERS[platform]
    wanted = SHORT_NAMES.get((platform, title), title).casefold()
    candidates = []
    prefix = f"roms/{folder}".casefold()
    for parent, entries in by_parent.items():
        if parent == prefix or (platform == "PS" and parent.startswith(prefix + "/")):
            for entry in entries:
                path = PurePosixPath(entry)
                if "imgs" in (part.casefold() for part in path.parts):
                    continue
                if path.stem.casefold() == wanted:
                    candidates.append(entry)
    # Prefer a visible M3U over a same-title CHD, then a root-level file.
    candidates.sort(key=lambda value: (PurePosixPath(value).suffix.casefold() != ".m3u", len(PurePosixPath(value).parts), value.casefold()))
    if not candidates:
        raise RuntimeError(f"Missing {platform} ROM for {title!r} ({wanted!r})")
    return candidates[0]


def load_pico_catalogue(pico_archive: Path) -> dict[str, tuple[str, str]]:
    with zipfile.ZipFile(pico_archive) as source:
        xml_name = next(name for name in source.namelist() if name.casefold() == "pico8/gamelist.xml")
        root = ET.fromstring(source.read(xml_name))
    catalogue = {}
    for game in root.findall("game"):
        title = (game.findtext("name") or "").strip()
        cart = re.sub(r"^\./", "", (game.findtext("path") or "").strip().replace("\\", "/"))
        image = re.sub(r"^\./", "", (game.findtext("image") or "").strip().replace("\\", "/"))
        if title and cart:
            catalogue[title.casefold()] = (cart, image or cart)
    return catalogue


def onion_id(platform: str, relative: str) -> str:
    encoded = urllib.parse.quote(relative, safe="-_.ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")
    return f"onion:{platform}:{encoded}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=Path, default=Path("config/household-seed.json"))
    parser.add_argument("--tiny-root", type=Path, default=Path("Q:/miyoo/tiny"))
    parser.add_argument("--pico-archive", type=Path, default=Path("Q:/miyoo/PICOwesome v1.5 (apr-12-2024).zip"))
    parser.add_argument("--output", type=Path, default=Path("out/household-payload"))
    args = parser.parse_args()

    game_archives = [
        args.tiny_root / "tiny-best-set-go-games.zip",
        args.tiny_root / "tiny-best-set-go-expansion-64-games.zip",
        args.tiny_root / "tiny-best-set-go-expansion-128-games.zip",
    ]
    image_archives = [
        args.tiny_root / "tiny-best-set-go-imgs-onion.zip",
        args.tiny_root / "tiny-best-set-go-expansion-64-imgs-onion.zip",
        args.tiny_root / "tiny-best-set-go-expansion-128-imgs-onion.zip",
    ]
    for required in [args.seed, args.pico_archive, *game_archives, *image_archives]:
        if not required.is_file():
            raise FileNotFoundError(required)

    if args.output.exists():
        shutil.rmtree(args.output)
    args.output.mkdir(parents=True)

    game_exact, game_parents = zip_index(game_archives)
    image_exact, _ = zip_index(image_archives)
    pico_catalogue = load_pico_catalogue(args.pico_archive)
    selected = [item for item in unique_seed_items(args.seed) if item not in NATIVE_TITLES]
    entries = []
    missing_art = []
    extracted_bytes = 0
    extracted_files = 0
    used_platforms = {platform for platform, _ in selected}

    with zipfile.ZipFile(args.pico_archive) as pico_source:
        pico_lookup = {info.filename.casefold(): info for info in pico_source.infolist() if not info.is_dir()}
        for platform, title in selected:
            folder = SYSTEM_FOLDERS[platform]
            if platform == "PICO":
                known = pico_catalogue.get(title.casefold())
                if not known:
                    raise RuntimeError(f"Missing PICO-8 catalogue title {title!r}")
                source_cart, _ = known
                archive_name = f"pico8/{source_cart}".replace("//", "/")
                info = pico_lookup.get(archive_name.casefold())
                if not info:
                    raise RuntimeError(f"Missing PICO-8 cart {archive_name!r}")
                relative = source_cart
                destination = f"Roms/{folder}/{relative}"
                extracted_bytes += extract_entry(args.pico_archive, info, args.output, destination)
                extracted_files += 1
                cover = destination
            else:
                source_rom = find_rom(platform, title, game_parents, game_exact)
                archive, info = game_exact[source_rom.casefold()]
                relative = str(PurePosixPath(source_rom).relative_to("Roms", folder))
                destination = f"Roms/{folder}/{relative}"
                extracted_bytes += extract_entry(archive, info, args.output, destination)
                extracted_files += 1

                if PurePosixPath(source_rom).suffix.casefold() == ".m3u":
                    with zipfile.ZipFile(archive) as source:
                        references = source.read(info).decode("utf-8-sig").splitlines()
                    for reference in references:
                        reference = reference.strip().replace("\\", "/")
                        if not reference or reference.startswith("#"):
                            continue
                        dependency = str(
                            PurePosixPath("Roms", folder, reference.lstrip("/"))
                            if reference.startswith("/")
                            else PurePosixPath(source_rom).parent / reference
                        )
                        dep_source = game_exact.get(dependency.casefold())
                        if not dep_source:
                            raise RuntimeError(f"Missing M3U dependency {dependency!r}")
                        dep_archive, dep_info = dep_source
                        dep_relative = str(PurePosixPath(dependency).relative_to("Roms", folder))
                        extracted_bytes += extract_entry(dep_archive, dep_info, args.output, f"Roms/{folder}/{dep_relative}")
                        extracted_files += 1

                image_name = f"Roms/{folder}/Imgs/{PurePosixPath(relative).stem}.png"
                image_source = image_exact.get(image_name.casefold())
                if image_source:
                    image_archive, image_info = image_source
                    extracted_bytes += extract_entry(image_archive, image_info, args.output, image_name)
                    extracted_files += 1
                    cover = image_name
                else:
                    cover = ""
                    missing_art.append(f"{platform}: {title}")

            entries.append({
                "platform": platform,
                "cart": relative,
                "id": onion_id(platform, relative),
                "title": title,
                **({"cover": cover} if cover else {}),
            })

    for platform in used_platforms:
        for bios_name in BIOS_BY_PLATFORM.get(platform, set()):
            source = game_exact.get(bios_name.casefold())
            if source:
                archive, info = source
                extracted_bytes += extract_entry(archive, info, args.output, bios_name)
                extracted_files += 1

    catalogue_path = args.output / "Sprout/catalogue/games.json"
    catalogue_path.parent.mkdir(parents=True, exist_ok=True)
    catalogue_path.write_text(json.dumps({"schemaVersion": 1, "entries": entries}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    extracted_files += 1

    report = {
        "seedUniqueTitles": len(selected) + len(NATIVE_TITLES),
        "nativeTitles": len(NATIVE_TITLES),
        "emulatedTitles": len(selected),
        "catalogueEntries": len(entries),
        "extractedFiles": extracted_files,
        "extractedBytes": extracted_bytes,
        "missingArtwork": missing_art,
        "output": str(args.output.resolve()),
    }
    (args.output / "payload-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
