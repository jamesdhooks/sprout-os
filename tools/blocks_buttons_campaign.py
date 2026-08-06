#!/usr/bin/env python3
"""Generate, validate, and inject the shared 30-room Blocks campaign."""

from __future__ import annotations

import argparse
import hashlib
import json
import random
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / "game-design/blocks-buttons/campaign-spec.json"
CAMPAIGN = ROOT / "game-design/blocks-buttons/campaign.json"
TARGETS = [ROOT / "games/blocks-buttons/game.lua", ROOT / "pico8/blocks-buttons/blocks-buttons.p8"]
BEGIN = "-- BEGIN GENERATED BLOCKS CAMPAIGN"
END = "-- END GENERATED BLOCKS CAMPAIGN"
DIRS = ((0, -1, "u"), (1, 0, "r"), (0, 1, "d"), (-1, 0, "l"))


def floor_cells(walls: tuple[str, ...]) -> set[tuple[int, int]]:
    return {(x, y) for y, row in enumerate(walls) for x, value in enumerate(row) if value != "#"}


def reachable(player: tuple[int, int], crates: frozenset[tuple[int, int]], floors: set[tuple[int, int]]) -> set[tuple[int, int]]:
    seen = {player}
    queue = deque([player])
    while queue:
        x, y = queue.popleft()
        for dx, dy, _ in DIRS:
            point = (x + dx, y + dy)
            if point in floors and point not in crates and point not in seen:
                seen.add(point)
                queue.append(point)
    return seen


def solve(walls: tuple[str, ...], player: tuple[int, int], crates: tuple[tuple[int, int], ...], targets: tuple[tuple[int, int], ...], limit: int = 80000):
    floors = floor_cells(walls)
    goal = frozenset(targets)
    start = (player, frozenset(crates))
    queue = deque([start])
    previous = {start: None}
    pushed = {}
    while queue and len(previous) <= limit:
        state = queue.popleft()
        actor, boxes = state
        if boxes == goal:
            solution = []
            while previous[state] is not None:
                solution.append(pushed[state])
                state = previous[state]
            return list(reversed(solution))
        walk = reachable(actor, boxes, floors)
        for bx, by in sorted(boxes):
            for dx, dy, name in DIRS:
                stand = (bx - dx, by - dy)
                destination = (bx + dx, by + dy)
                if stand not in walk or destination not in floors or destination in boxes:
                    continue
                moved = frozenset((destination if point == (bx, by) else point) for point in boxes)
                following = ((bx, by), moved)
                if following in previous:
                    continue
                previous[following] = state
                pushed[following] = name
                queue.append(following)
    return None


def dead_squares(walls: tuple[str, ...], targets: tuple[tuple[int, int], ...]) -> list[tuple[int, int]]:
    floors = floor_cells(walls)
    safe = set(targets)
    queue = deque(targets)
    while queue:
        x, y = queue.popleft()
        for dx, dy, _ in DIRS:
            source = (x - dx, y - dy)
            stand = (x - 2 * dx, y - 2 * dy)
            if source in floors and stand in floors and source not in safe:
                safe.add(source)
                queue.append(source)
    return sorted(floors - safe)


def walls_for(rng: random.Random, band: int) -> tuple[str, ...]:
    grid = [["#" if x in (0, 7) or y in (0, 7) else "." for x in range(8)] for y in range(8)]
    count = 0 if band == 1 else (1 if band == 2 else rng.randint(1, 3))
    candidates = [(x, y) for y in range(2, 6) for x in range(2, 6)]
    rng.shuffle(candidates)
    for x, y in candidates[:count]:
        grid[y][x] = "#"
    return tuple("".join(row) for row in grid)


def candidate(rng: random.Random, band: int, crate_count: int):
    walls = walls_for(rng, band)
    floors = list(floor_cells(walls))
    targets = tuple(sorted(rng.sample(floors, crate_count)))
    crates = list(targets)
    pull_steps = rng.randint(3 + band * 2, 6 + band * 5)
    for _ in range(pull_steps):
        index = rng.randrange(crate_count)
        x, y = crates[index]
        options = []
        occupied = set(crates)
        for dx, dy, _ in DIRS:
            source, stand = (x - dx, y - dy), (x - 2 * dx, y - 2 * dy)
            if source in floors and stand in floors and source not in occupied and stand not in occupied:
                options.append((source, stand))
        if options:
            crates[index], player = rng.choice(options)
    if tuple(sorted(crates)) == targets:
        return None
    player_options = [point for point in floors if point not in crates]
    if not player_options:
        return None
    player = locals().get("player", rng.choice(player_options))
    if player in crates:
        player = rng.choice(player_options)
    solution = solve(walls, player, tuple(sorted(crates)), targets)
    if not solution:
        return None
    return walls, player, tuple(sorted(crates)), targets, solution


def generate() -> dict:
    spec = json.loads(SPEC.read_text(encoding="utf-8"))
    rng = random.Random(0x5A17B10C)
    rooms = []
    signatures = set()
    attempts = 0
    while len(rooms) < spec["rooms"] and attempts < 200000:
        attempts += 1
        number = len(rooms) + 1
        band = 1 if number <= 8 else 2 if number <= 20 else 3
        crate_count = 1 if band == 1 else 2 if band == 2 else (2 if number <= 25 else 3)
        made = candidate(rng, band, crate_count)
        if made is None:
            continue
        walls, player, crates, targets, solution = made
        minimum, maximum = spec["bands"][band - 1]["minimumPushes"]
        if not minimum <= len(solution) <= maximum:
            continue
        canonical = "|".join(walls) + ":" + ";".join(f"{x},{y}" for x, y in crates) + ":" + ";".join(f"{x},{y}" for x, y in targets)
        layout_hash = hashlib.sha256(canonical.encode()).hexdigest()
        signature = (tuple(walls), crates, targets, "".join(solution))
        if signature in signatures:
            continue
        signatures.add(signature)
        changes = sum(a != b for a, b in zip(solution, solution[1:]))
        dead = dead_squares(walls, targets)
        rooms.append({
            "number": number, "seed": str(rng.getrandbits(64)), "band": band,
            "walls": list(walls), "player": list(player),
            "crates": [list(point) for point in crates], "buttons": [list(point) for point in targets],
            "deadSquares": [list(point) for point in dead],
            "metrics": {"minimumPushes": len(solution), "directionChanges": changes,
                        "crateDependencies": max(0, crate_count - 1)},
            "solutionSignature": "".join(solution), "layoutHash": layout_hash,
        })
    if len(rooms) != spec["rooms"]:
        raise SystemExit(f"generated only {len(rooms)} rooms after {attempts} attempts")
    payload = {"schemaVersion": 1, "gameId": spec["gameId"], "generator": spec["generator"],
               "generatorVersion": spec["generatorVersion"], "rooms": rooms}
    payload["sourceHash"] = hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return payload


def validate(payload: dict) -> None:
    if len(payload.get("rooms", [])) != 30:
        raise SystemExit("campaign must contain exactly 30 rooms")
    hashes = set()
    for room in payload["rooms"]:
        walls = tuple(room["walls"])
        solution = solve(walls, tuple(room["player"]), tuple(map(tuple, room["crates"])), tuple(map(tuple, room["buttons"])))
        if solution is None or len(solution) != room["metrics"]["minimumPushes"]:
            raise SystemExit(f"room {room['number']} solver proof mismatch")
        if room["layoutHash"] in hashes:
            raise SystemExit("duplicate room layout")
        hashes.add(room["layoutHash"])
        if dead_squares(walls, tuple(map(tuple, room["buttons"]))) != list(map(tuple, room["deadSquares"])):
            raise SystemExit(f"room {room['number']} dead-square mismatch")


def lua_payload(payload: dict, compact: bool = False) -> str:
    if compact:
        encoded = []
        for room in payload["rooms"]:
            point = lambda value: f"{value[1] * 8 + value[0]:02x}"
            encoded.append("".join(room["walls"]) + "|" + point(room["player"]) + "|" +
                           "".join(map(point, room["crates"])) + "|" +
                           "".join(map(point, room["buttons"])) + "|" +
                           "".join(map(point, room["deadSquares"])))
        return f"blocks_campaign_version=1\nblocks_campaign_hash={json.dumps(payload['sourceHash'])}\nblocks_campaign={{" + ",".join(map(json.dumps, encoded)) + "}"
    chunks = []
    for room in payload["rooms"]:
        rows = ",".join(json.dumps(row) for row in room["walls"])
        points = lambda values: "{" + ",".join("{%d,%d}" % tuple(value) for value in values) + "}"
        chunks.append("{w={%s},p={%d,%d},c=%s,b=%s,d=%s}" % (
            rows, *room["player"], points(room["crates"]), points(room["buttons"]), points(room["deadSquares"])))
    return f"blocks_campaign_version=1\nblocks_campaign_hash={json.dumps(payload['sourceHash'])}\nblocks_campaign={{\n" + ",\n".join(chunks) + "\n}"


def inject(payload: dict) -> None:
    for target in TARGETS:
        text = target.read_text(encoding="utf-8")
        if BEGIN not in text or END not in text:
            raise SystemExit(f"missing campaign markers in {target}")
        before, rest = text.split(BEGIN, 1)
        _, after = rest.split(END, 1)
        generated = lua_payload(payload, target.suffix == ".p8")
        target.write_text(before + BEGIN + "\n" + generated + "\n" + END + after, encoding="utf-8", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("generate", "validate", "inject"))
    args = parser.parse_args()
    if args.command == "generate":
        payload = generate()
        CAMPAIGN.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        validate(payload)
    else:
        payload = json.loads(CAMPAIGN.read_text(encoding="utf-8"))
        validate(payload)
        if args.command == "inject":
            inject(payload)
    print(f"blocks campaign {args.command} passed")


if __name__ == "__main__":
    main()
