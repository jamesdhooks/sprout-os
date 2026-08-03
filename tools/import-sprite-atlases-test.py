#!/usr/bin/env python3
"""Focused compatibility tests for the Sprout atlas importer."""

from __future__ import annotations

import importlib.util
import json
import tempfile
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("import-sprite-atlases.py")
SPEC = importlib.util.spec_from_file_location("sprout_atlas_importer", MODULE_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value), encoding="utf-8")


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="sprout-atlas-test-") as directory:
        root = Path(directory)
        write_json(root / "base.json", {
            "schemaVersion": 1,
            "provenance": {"source": "base", "license": "test", "generator": "test", "palette": "test"},
            "atlases": [{"id": "base", "image": "base.png", "size": [8, 8]}],
            "sprites": [{"id": "base.sprite", "atlas": "base", "rect": [0, 0, 8, 8], "pivot": [0, 0]}],
            "animations": [],
            "tileSets": [],
        })
        write_json(root / "atlas.json", {
            "schema": "sprite-atlas.v1",
            "size": {"width": 64, "height": 32},
            "frames": {
                "walk-01": {
                    "frame": {"x": 2, "y": 3, "width": 12, "height": 14},
                    "spriteSourceSize": {"x": 4, "y": 6, "width": 12, "height": 14},
                    "sourceSize": {"width": 24, "height": 24},
                    "pivot": {"x": 0.5, "y": 1.0},
                }
            },
            "animations": {"walk": {"frames": ["walk-01"], "fps": 12, "loop": True}},
        })
        write_json(root / "config.json", {
            "schemaVersion": 1,
            "baseManifest": "base.json",
            "provenance": {
                "source": "original-project-artwork",
                "license": "test",
                "generator": "test",
                "palette": "test",
            },
            "atlases": [{
                "id": "rich",
                "manifest": "atlas.json",
                "image": "rich.png",
                "prefix": "rich.",
                "animationPrefix": "rich.",
            }],
        })
        manifest = MODULE.compile_manifest(root / "config.json")
        assert [atlas["id"] for atlas in manifest["atlases"]] == ["base", "rich"]
        assert manifest["sprites"][1] == {
            "id": "rich.walk-01",
            "atlas": "rich",
            "rect": [2, 3, 12, 14],
            "pivot": [8, 14],
        }
        assert manifest["animations"][0] == {
            "id": "rich.walk",
            "loop": True,
            "frames": [{"sprite": "rich.walk-01", "ticks": 5}],
        }

        write_json(root / "texture-packer.json", {
            "frames": [{
                "filename": "icon",
                "frame": {"x": 0, "y": 0, "w": 16, "h": 16},
                "pivot": {"x": 0.5, "y": 0.5},
                "sourceSize": {"w": 16, "h": 16},
                "spriteSourceSize": {"x": 0, "y": 0, "w": 16, "h": 16},
            }],
            "meta": {"size": {"w": 16, "h": 16}},
        })
        config = json.loads((root / "config.json").read_text(encoding="utf-8"))
        config.pop("baseManifest")
        config["atlases"][0]["manifest"] = "texture-packer.json"
        write_json(root / "config.json", config)
        texture_packer = MODULE.compile_manifest(root / "config.json")
        assert texture_packer["sprites"][0]["pivot"] == [8, 8]

        write_json(root / "column.json", {
            "schema": "sprite-atlas.v1",
            "size": {"width": 16, "height": 4},
            "frames": {
                name: {
                    "frame": {"x": index * 4, "y": 0, "width": 4, "height": 4},
                    "sourceSize": {"width": 4, "height": 4},
                    "spriteSourceSize": {"x": 0, "y": 0, "width": 4, "height": 4},
                }
                for index, name in enumerate(("a", "b", "c", "d"))
            },
            "animations": {"top": {"frames": ["a", "b"], "fps": 10}},
        })
        config["atlases"][0].update({
            "manifest": "column.json",
            "gridOrder": "column-major",
            "columns": 2,
        })
        write_json(root / "config.json", config)
        column_major = MODULE.compile_manifest(root / "config.json")
        positions = {sprite["id"]: sprite["rect"][0] for sprite in column_major["sprites"]}
        assert positions == {"rich.a": 0, "rich.c": 4, "rich.b": 8, "rich.d": 12}
        assert [frame["sprite"] for frame in column_major["animations"][0]["frames"]] == ["rich.a", "rich.b"]
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
