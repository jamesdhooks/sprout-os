#!/usr/bin/env python3
"""Build and validate the deterministic Mouse & Cheese PICO-8 campaign."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


CAMPAIGN_SCHEMA_VERSION = 1
GENERATOR_VERSION = 3
CAMPAIGN_LEVEL_COUNT = 100
PAYLOAD_BEGIN = "-- BEGIN GENERATED CAMPAIGN"
PAYLOAD_END = "-- END GENERATED CAMPAIGN"


class XorShift16:
    def __init__(self, seed: int) -> None:
        self.state = seed & 0xFFFF or 0xACE1

    def next(self) -> int:
        value = self.state
        value ^= (value << 7) & 0xFFFF
        value ^= value >> 9
        value ^= (value << 8) & 0xFFFF
        self.state = value & 0xFFFF or 0xACE1
        return self.state

    def below(self, maximum: int) -> int:
        return self.next() % maximum


@dataclass(frozen=True)
class Band:
    through: int
    columns: int
    rows: int
    min_distance: int
    loop_count: int
    omission_percent: int
    shape_chunks: int
    max_chunk_depth: int


def load_spec(path: Path) -> list[Band]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("schemaVersion") != CAMPAIGN_SCHEMA_VERSION:
        raise ValueError("campaign spec schema version is unsupported")
    bands = [Band(**band) for band in payload["bands"]]
    if not bands or bands[-1].through != CAMPAIGN_LEVEL_COUNT:
        raise ValueError(f"campaign spec must end at level {CAMPAIGN_LEVEL_COUNT}")
    previous = 0
    for band in bands:
        if band.through <= previous or band.columns > 23 or band.rows > 21:
            raise ValueError("campaign bands must be ordered and PICO-8 sized")
        previous = band.through
    return bands


def band_for(level: int, bands: Iterable[Band]) -> Band:
    for band in bands:
        if level <= band.through:
            return band
    raise ValueError("level is outside the campaign")


def neighbours(cell: int, columns: int, rows: int) -> list[int]:
    x, y = cell % columns, cell // columns
    result: list[int] = []
    if x:
        result.append(cell - 1)
    if x + 1 < columns:
        result.append(cell + 1)
    if y:
        result.append(cell - columns)
    if y + 1 < rows:
        result.append(cell + columns)
    return result


def edge(left: int, right: int) -> tuple[int, int]:
    return (left, right) if left < right else (right, left)


def bfs(start: int, present: set[int], edges: set[tuple[int, int]]) -> dict[int, int]:
    distances = {start: 0}
    queue: deque[int] = deque([start])
    adjacency: dict[int, list[int]] = {cell: [] for cell in present}
    for left, right in edges:
        if left in present and right in present:
            adjacency[left].append(right)
            adjacency[right].append(left)
    while queue:
        current = queue.popleft()
        for candidate in adjacency[current]:
            if candidate not in distances:
                distances[candidate] = distances[current] + 1
                queue.append(candidate)
    return distances


def layout(seed: int, band: Band) -> dict[str, object]:
    rng = XorShift16(seed)
    total = band.columns * band.rows
    present = set(range(total))
    cutouts: set[int] = set()
    for _ in range(band.shape_chunks):
        side = rng.below(4)
        along = band.rows if side < 2 else band.columns
        span = 1 + rng.below(max(1, min(3, along // 3)))
        depth = 1 + rng.below(band.max_chunk_depth)
        first = rng.below(along - span + 1)
        for position in range(first, first + span):
            for inset in range(depth):
                if side == 0:
                    x, y = inset, position
                elif side == 1:
                    x, y = band.columns - 1 - inset, position
                elif side == 2:
                    x, y = position, inset
                else:
                    x, y = position, band.rows - 1 - inset
                cutouts.add(y * band.columns + x)
    present.difference_update(cutouts)
    active = sorted(present)
    start = active[rng.below(len(active))]
    visited = {start}
    stack = [start]
    edges: set[tuple[int, int]] = set()
    while stack:
        current = stack[-1]
        options = [cell for cell in neighbours(current, band.columns, band.rows)
                   if cell in present and cell not in visited]
        if not options:
            stack.pop()
            continue
        candidate = options[rng.below(len(options))]
        edges.add(edge(current, candidate))
        visited.add(candidate)
        stack.append(candidate)

    if visited != present:
        raise ValueError("generated shape mask is disconnected")

    closed = [edge(cell, candidate) for cell in active
              for candidate in neighbours(cell, band.columns, band.rows)
              if candidate in present and cell < candidate and edge(cell, candidate) not in edges]
    for _ in range(min(band.loop_count, len(closed))):
        index = rng.below(len(closed))
        edges.add(closed.pop(index))

    degree = {cell: 0 for cell in present}
    for left, right in edges:
        degree[left] += 1
        degree[right] += 1
    leaves = [cell for cell in sorted(present) if degree[cell] == 1 and cell != start]
    omissions = min(math.floor(total * band.omission_percent / 100),
                    max(0, len(leaves) - 1))
    for _ in range(omissions):
        index = rng.below(len(leaves))
        present.remove(leaves.pop(index))

    distances = bfs(start, present, edges)
    if len(distances) != len(present):
        raise ValueError("generated maze is disconnected")
    farthest_distance = max(distances.values())
    farthest = sorted(cell for cell, distance in distances.items()
                      if distance == farthest_distance)
    goal = farthest[rng.below(len(farthest))]
    payload = {
        "columns": band.columns,
        "rows": band.rows,
        "start": start,
        "goal": goal,
        "present": sorted(present),
        "cutouts": sorted(cutouts),
        "edges": sorted(edges),
    }
    layout_hash = hashlib.sha256(
        json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
    ).hexdigest()
    junctions = sum(1 for cell in present if sum(
        1 for candidate in neighbours(cell, band.columns, band.rows)
        if edge(cell, candidate) in edges and candidate in present
    ) >= 3)
    return {
        **payload,
        "distance": distances[goal],
        "junctions": junctions,
        "layoutHash": layout_hash,
    }


def candidate_seed(level: int, attempt: int) -> int:
    return ((level * 40503) + (attempt * 7919) + 0x51A7) & 0xFFFF or 1


def build_campaign(bands: list[Band]) -> dict[str, object]:
    levels = []
    hashes: set[str] = set()
    for level in range(1, CAMPAIGN_LEVEL_COUNT + 1):
        band = band_for(level, bands)
        accepted: dict[str, object] | None = None
        seed = 0
        for attempt in range(256):
            seed = candidate_seed(level, attempt)
            try:
                candidate = layout(seed, band)
            except ValueError:
                continue
            if candidate["distance"] < band.min_distance:
                continue
            if candidate["layoutHash"] in hashes:
                continue
            accepted = candidate
            break
        if accepted is None:
            raise ValueError(f"level {level} did not produce an accepted maze")
        hashes.add(str(accepted["layoutHash"]))
        levels.append({
            "level": level,
            "seed": seed,
            "columns": accepted["columns"],
            "rows": accepted["rows"],
            "start": accepted["start"],
            "goal": accepted["goal"],
            "metrics": {
                "routeDistance": accepted["distance"],
                "junctions": accepted["junctions"],
                "omittedRooms": (band.columns * band.rows) - len(accepted["present"]),
                "shapeCutouts": len(accepted["cutouts"]),
                "loopCount": band.loop_count,
            },
            "layoutHash": accepted["layoutHash"],
        })
    seed_payload = "".join(f"{record['seed']:04x}" for record in levels)
    return {
        "schemaVersion": CAMPAIGN_SCHEMA_VERSION,
        "generator": "mouse-cheese-pico8-xorshift16",
        "generatorVersion": GENERATOR_VERSION,
        "levelCount": len(levels),
        "seedPayload": seed_payload,
        "seedPayloadSha256": hashlib.sha256(seed_payload.encode("ascii")).hexdigest(),
        "goldenVectors": [
            {key: levels[index][key] for key in ("level", "seed", "layoutHash")}
            for index in (0, CAMPAIGN_LEVEL_COUNT // 2 - 1, CAMPAIGN_LEVEL_COUNT - 1)
        ],
        "levels": levels,
    }


def validate_campaign(campaign: dict[str, object], bands: list[Band]) -> None:
    if campaign.get("schemaVersion") != CAMPAIGN_SCHEMA_VERSION:
        raise ValueError("campaign schema version is unsupported")
    levels = campaign.get("levels")
    if not isinstance(levels, list) or len(levels) != CAMPAIGN_LEVEL_COUNT:
        raise ValueError(f"campaign must contain exactly {CAMPAIGN_LEVEL_COUNT} levels")
    payload = "".join(f"{record['seed']:04x}" for record in levels)
    if payload != campaign.get("seedPayload"):
        raise ValueError("campaign seed payload does not match level records")
    if hashlib.sha256(payload.encode("ascii")).hexdigest() != campaign.get("seedPayloadSha256"):
        raise ValueError("campaign payload hash is invalid")
    seen = set()
    for expected_level, record in enumerate(levels, start=1):
        if record.get("level") != expected_level:
            raise ValueError("campaign level numbering is invalid")
        generated = layout(int(record["seed"]), band_for(expected_level, bands))
        if generated["layoutHash"] != record.get("layoutHash"):
            raise ValueError(f"level {expected_level} layout hash is invalid")
        if generated["layoutHash"] in seen:
            raise ValueError(f"level {expected_level} duplicates a prior layout")
        seen.add(generated["layoutHash"])
        if generated["distance"] < band_for(expected_level, bands).min_distance:
            raise ValueError(f"level {expected_level} misses its route-distance floor")
        metrics = record.get("metrics", {})
        if metrics.get("shapeCutouts") != len(generated["cutouts"]):
            raise ValueError(f"level {expected_level} shape-cutout count is invalid")
        if not generated["cutouts"]:
            raise ValueError(f"level {expected_level} has no silhouette cutout")
    for vector in campaign.get("goldenVectors", []):
        record = levels[int(vector["level"]) - 1]
        if record["seed"] != vector["seed"] or record["layoutHash"] != vector["layoutHash"]:
            raise ValueError("golden vector does not match campaign data")


def inject_campaign(cart_path: Path, campaign: dict[str, object]) -> None:
    source = cart_path.read_text(encoding="utf-8")
    start = source.find(PAYLOAD_BEGIN)
    end = source.find(PAYLOAD_END)
    if start < 0 or end < start:
        raise ValueError("cart does not contain generated campaign markers")
    payload = str(campaign["seedPayload"])
    chunks = [payload[index:index + 96] for index in range(0, len(payload), 96)]
    lua_payload = "campaign_data=\"" + chunks[0] + "\""
    lua_payload += "".join(f'\n..\"{chunk}\"' for chunk in chunks[1:])
    generated = "\n".join((
        PAYLOAD_BEGIN,
        lua_payload,
        f'campaign_payload_hash="{campaign["seedPayloadSha256"]}"',
        f"campaign_generator_version={campaign['generatorVersion']}",
        PAYLOAD_END,
    ))
    end += len(PAYLOAD_END)
    cart_path.write_text(source[:start] + generated + source[end:], encoding="utf-8", newline="\n")


def preview(campaign: dict[str, object], bands: list[Band], level: int, output: Path) -> None:
    record = campaign["levels"][level - 1]
    generated = layout(int(record["seed"]), band_for(level, bands))
    scale = 12
    width = int(generated["columns"]) * scale
    height = int(generated["rows"]) * scale
    present = set(generated["present"])
    edges = set(tuple(value) for value in generated["edges"])
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">',
             f'<rect width="100%" height="100%" fill="#183f35"/>']
    for cell in present:
        x, y = cell % int(generated["columns"]), cell // int(generated["columns"])
        parts.append(f'<rect x="{x * scale + 1}" y="{y * scale + 1}" width="{scale - 2}" height="{scale - 2}" fill="#c7904d"/>')
    for left, right in edges:
        x1, y1 = left % int(generated["columns"]), left // int(generated["columns"])
        x2, y2 = right % int(generated["columns"]), right // int(generated["columns"])
        parts.append(f'<line x1="{x1 * scale + scale // 2}" y1="{y1 * scale + scale // 2}" x2="{x2 * scale + scale // 2}" y2="{y2 * scale + scale // 2}" stroke="#f5cf7a" stroke-width="3"/>')
    for cell, colour in ((generated["start"], "#f4f1dc"), (generated["goal"], "#f7d13d")):
        x, y = cell % int(generated["columns"]), cell // int(generated["columns"])
        parts.append(f'<circle cx="{x * scale + scale // 2}" cy="{y * scale + scale // 2}" r="{scale // 3}" fill="{colour}"/>')
    output.write_text("".join(parts) + "</svg>\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("generate", "validate", "inject", "preview"))
    parser.add_argument("--spec", type=Path, required=True)
    parser.add_argument("--campaign", type=Path, required=True)
    parser.add_argument("--cart", type=Path)
    parser.add_argument("--level", type=int, default=1)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    bands = load_spec(args.spec)
    if args.command == "generate":
        campaign = build_campaign(bands)
        args.campaign.write_text(json.dumps(campaign, indent=2) + "\n", encoding="utf-8")
        print(f"generated {len(campaign['levels'])} levels")
        return 0
    campaign = json.loads(args.campaign.read_text(encoding="utf-8"))
    validate_campaign(campaign, bands)
    if args.command == "validate":
        if args.cart:
            cart = args.cart.read_text(encoding="utf-8")
            start = cart.find(PAYLOAD_BEGIN)
            end = cart.find(PAYLOAD_END, start)
            generated = cart[start:end] if start >= 0 and end > start else ""
            payload_parts = re.findall(r'"([0-9a-f]+)"', generated)
            cart_payload = "".join(payload_parts[:-1]) if payload_parts else ""
            if (cart_payload != campaign["seedPayload"] or
                    campaign["seedPayloadSha256"] not in generated):
                raise ValueError("cart payload differs from validated campaign")
        print("campaign validation passed")
    elif args.command == "inject":
        if not args.cart:
            raise ValueError("inject requires --cart")
        inject_campaign(args.cart, campaign)
        print("campaign injected")
    else:
        if not args.output:
            raise ValueError("preview requires --output")
        if args.level < 1 or args.level > CAMPAIGN_LEVEL_COUNT:
            raise ValueError(f"preview level must be between 1 and {CAMPAIGN_LEVEL_COUNT}")
        preview(campaign, bands, args.level, args.output)
        print(f"previewed level {args.level}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
