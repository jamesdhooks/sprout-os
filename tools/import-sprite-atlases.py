#!/usr/bin/env python3
"""Compile common atlas exports into a Sprout Runtime asset manifest."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any


def require_object(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{context} must be an object")
    return value


def positive_int(value: Any, context: str, maximum: int = 16384) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value <= 0 or value > maximum:
        raise ValueError(f"{context} must be an integer from 1 through {maximum}")
    return value


def dimensions(value: Any, context: str) -> tuple[int, int]:
    record = require_object(value, context)
    width = record.get("width", record.get("w"))
    height = record.get("height", record.get("h"))
    return positive_int(width, f"{context}.width"), positive_int(height, f"{context}.height")


def frame_rectangle(value: Any, context: str) -> tuple[int, int, int, int]:
    record = require_object(value, context)
    x = record.get("x")
    y = record.get("y")
    width = record.get("width", record.get("w"))
    height = record.get("height", record.get("h"))
    if not isinstance(x, int) or x < 0 or not isinstance(y, int) or y < 0:
        raise ValueError(f"{context} origin must be non-negative integers")
    return x, y, positive_int(width, f"{context}.width"), positive_int(height, f"{context}.height")


def source_frames(document: dict[str, Any]) -> list[tuple[str, dict[str, Any]]]:
    frames = document.get("frames")
    if isinstance(frames, dict):
        return [(str(name), require_object(value, f"frame {name}")) for name, value in frames.items()]
    if isinstance(frames, list):
        result: list[tuple[str, dict[str, Any]]] = []
        for index, value in enumerate(frames):
            frame = require_object(value, f"frame {index}")
            name = frame.get("filename", frame.get("name"))
            if not isinstance(name, str) or not name:
                raise ValueError(f"frame {index} is missing filename")
            result.append((name, frame))
        return result
    raise ValueError("atlas frames must be an object or array")


def atlas_size(document: dict[str, Any]) -> tuple[int, int]:
    if document.get("schema") == "sprite-atlas.v1":
        return dimensions(document.get("size"), "atlas size")
    meta = require_object(document.get("meta"), "TexturePacker meta")
    return dimensions(meta.get("size"), "TexturePacker meta.size")


def frame_pivot(frame: dict[str, Any], width: int, height: int) -> tuple[int, int]:
    pivot = frame.get("pivot")
    if not isinstance(pivot, dict):
        return 0, 0
    pivot_x = pivot.get("x", 0)
    pivot_y = pivot.get("y", 0)
    if not isinstance(pivot_x, (int, float)) or not math.isfinite(pivot_x):
        raise ValueError("frame pivot.x must be finite")
    if not isinstance(pivot_y, (int, float)) or not math.isfinite(pivot_y):
        raise ValueError("frame pivot.y must be finite")
    if 0 <= pivot_x <= 1 and 0 <= pivot_y <= 1 and isinstance(frame.get("sourceSize"), dict):
        source_width, source_height = dimensions(frame["sourceSize"], "frame sourceSize")
        source_rect = frame.get("spriteSourceSize", {})
        source_x = source_rect.get("x", 0) if isinstance(source_rect, dict) else 0
        source_y = source_rect.get("y", 0) if isinstance(source_rect, dict) else 0
        return (
            max(0, min(width, round(pivot_x * source_width - source_x))),
            max(0, min(height, round(pivot_y * source_height - source_y))),
        )
    return max(0, min(width, round(pivot_x))), max(0, min(height, round(pivot_y)))


def compile_manifest(config_path: Path) -> dict[str, Any]:
    config = require_object(json.loads(config_path.read_text(encoding="utf-8")), "import config")
    if config.get("schemaVersion") != 1:
        raise ValueError("import config schemaVersion must be 1")
    provenance = require_object(config.get("provenance"), "provenance")
    for field in ("source", "license", "generator", "palette"):
        if not isinstance(provenance.get(field), str) or not provenance[field]:
            raise ValueError(f"provenance.{field} must be non-empty text")

    base_name = config.get("baseManifest")
    if base_name is not None and (not isinstance(base_name, str) or not base_name):
        raise ValueError("baseManifest must be a non-empty relative path")
    if base_name:
        result = require_object(
            json.loads((config_path.parent / base_name).read_text(encoding="utf-8")),
            "base manifest",
        )
        if result.get("schemaVersion") != 1:
            raise ValueError("base manifest schemaVersion must be 1")
        result = {
            "schemaVersion": 1,
            "provenance": {},
            "atlases": list(result.get("atlases", [])),
            "sprites": list(result.get("sprites", [])),
            "animations": list(result.get("animations", [])),
            "tileSets": list(result.get("tileSets", [])),
        }
    else:
        result = {
            "schemaVersion": 1,
            "provenance": {},
            "atlases": [],
            "sprites": [],
            "animations": [],
            "tileSets": [],
        }
    result["provenance"] = {
        field: provenance[field] for field in ("source", "license", "generator", "palette")
    }
    configured_tile_sets = config.get("tileSets", [])
    if not isinstance(configured_tile_sets, list):
        raise ValueError("tileSets must be an array")
    result["tileSets"].extend(configured_tile_sets)
    atlas_entries = config.get("atlases")
    if not isinstance(atlas_entries, list) or not atlas_entries:
        raise ValueError("import config must contain at least one atlas")

    atlas_ids = {
        require_object(atlas, "base atlas").get("id") for atlas in result["atlases"]
    }
    sprite_ids = {
        require_object(sprite, "base sprite").get("id") for sprite in result["sprites"]
    }
    animation_ids = {
        require_object(animation, "base animation").get("id") for animation in result["animations"]
    }
    for atlas_index, raw_entry in enumerate(atlas_entries):
        entry = require_object(raw_entry, f"atlas {atlas_index}")
        atlas_id = entry.get("id")
        manifest_name = entry.get("manifest")
        image_name = entry.get("image")
        prefix = entry.get("prefix", "")
        rename = entry.get("rename", {})
        if not isinstance(atlas_id, str) or not atlas_id:
            raise ValueError(f"atlas {atlas_index}.id must be non-empty text")
        if atlas_id in atlas_ids:
            raise ValueError(f"duplicate Sprout atlas id: {atlas_id}")
        if not isinstance(manifest_name, str) or not manifest_name:
            raise ValueError(f"atlas {atlas_index}.manifest must be non-empty text")
        if not isinstance(image_name, str) or not image_name.endswith(".png"):
            raise ValueError(f"atlas {atlas_index}.image must be a relative PNG path")
        if not isinstance(prefix, str) or not isinstance(rename, dict):
            raise ValueError(f"atlas {atlas_index} prefix/rename is invalid")
        source = require_object(
            json.loads((config_path.parent / manifest_name).read_text(encoding="utf-8")),
            f"atlas {atlas_id}",
        )
        width, height = atlas_size(source)
        output_atlas = {"id": atlas_id, "image": image_name, "size": [width, height]}
        sampling = entry.get("sampling", "linear")
        if sampling not in ("nearest", "linear"):
            raise ValueError(f"atlas {atlas_index}.sampling must be nearest or linear")
        output_atlas["sampling"] = sampling
        mips = entry.get("mips", [])
        if not isinstance(mips, list):
            raise ValueError(f"atlas {atlas_index}.mips must be an array")
        if mips:
            output_atlas["mips"] = mips
        result["atlases"].append(output_atlas)
        atlas_ids.add(atlas_id)
        name_map: dict[str, str] = {}
        semantic_name_map: dict[str, str] = {}
        source_frame_entries = source_frames(source)
        grid_order = entry.get("gridOrder", "row-major")
        if grid_order not in ("row-major", "column-major"):
            raise ValueError(f"atlas {atlas_index}.gridOrder must be row-major or column-major")
        columns = entry.get("columns")
        if grid_order == "column-major":
            columns = positive_int(columns, f"atlas {atlas_index}.columns", 64)
            if len(source_frame_entries) % columns != 0:
                raise ValueError(f"atlas {atlas_index} frame count is not divisible by columns")
        rows = len(source_frame_entries) // columns if grid_order == "column-major" else 0
        semantic_names = [name for name, _ in source_frame_entries]
        for frame_index, (source_name, frame) in enumerate(source_frame_entries):
            frame_data = frame.get("frame")
            x, y, frame_width, frame_height = frame_rectangle(frame_data, f"frame {source_name}")
            semantic_name = source_name
            if grid_order == "column-major":
                semantic_index = (frame_index % columns) * rows + frame_index // columns
                semantic_name = semantic_names[semantic_index]
            target_name = rename.get(semantic_name, f"{prefix}{semantic_name}")
            if not isinstance(target_name, str) or not target_name or target_name in sprite_ids:
                raise ValueError(f"duplicate or invalid Sprout sprite id: {target_name}")
            if x + frame_width > width or y + frame_height > height:
                raise ValueError(f"frame {source_name} escapes atlas {atlas_id}")
            pivot_x, pivot_y = frame_pivot(frame, frame_width, frame_height)
            result["sprites"].append({
                "id": target_name,
                "atlas": atlas_id,
                "rect": [x, y, frame_width, frame_height],
                "pivot": [pivot_x, pivot_y],
            })
            sprite_ids.add(target_name)
            name_map[source_name] = target_name
            semantic_name_map[semantic_name] = target_name

        animations = source.get("animations", {})
        if isinstance(animations, dict):
            for animation_name, raw_animation in animations.items():
                animation = require_object(raw_animation, f"animation {animation_name}")
                names = animation.get("frames")
                if not isinstance(names, list) or not names:
                    continue
                fps = animation.get("fps", 12)
                if not isinstance(fps, (int, float)) or not math.isfinite(fps) or fps <= 0 or fps > 60:
                    raise ValueError(f"animation {animation_name} fps is invalid")
                ticks = max(1, round(60 / fps))
                target_animation = entry.get("animationPrefix", prefix) + str(animation_name)
                if target_animation in animation_ids:
                    raise ValueError(f"duplicate Sprout animation id: {target_animation}")
                result["animations"].append({
                    "id": target_animation,
                    "loop": animation.get("loop", True) is not False,
                    "frames": [{
                        "sprite": (semantic_name_map if grid_order == "column-major" else name_map)[str(name)],
                        "ticks": ticks,
                    } for name in names],
                })
                animation_ids.add(target_animation)

    return result


def encoded_manifest(manifest: dict[str, Any]) -> str:
    return json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    encoded = encoded_manifest(compile_manifest(args.config.resolve()))
    if args.check:
        if not args.output.is_file() or args.output.read_text(encoding="utf-8") != encoded:
            raise SystemExit(f"atlas import output is stale: {args.output}")
        return 0
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(encoded, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
