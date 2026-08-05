#!/usr/bin/env python3
"""Validate the deployable Onion Sprout vertical-slice package."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_arm_elf(path: Path) -> None:
    header = path.read_bytes()[:20]
    assert header[:4] == b"\x7fELF", f"{path} is not ELF"
    assert header[4] == 1, f"{path} is not 32-bit ELF"
    assert header[5] == 1, f"{path} is not little-endian ELF"
    assert int.from_bytes(header[18:20], "little") == 40, f"{path} is not ARM"


def validate(package: Path) -> None:
    app = package / "App" / "Sprout"
    required = [
        app / "launch.sh",
        app / "config.json",
        app / "bin" / "sprout-launcher",
        app / "bin" / "sprout-runtime",
        app / "lib" / "libSDL2-2.0.so.0",
        app / "lib" / "libSDL2_image-2.0.so.0",
        app / "games" / "blocks-buttons" / "manifest.json",
        app / "games" / "blocks-buttons" / "game.lua",
        app / "games" / "blocks-buttons" / "asset-manifest.json",
        app / "config" / "household-seed.json",
        package / "deployment-manifest.sha256",
        package / "deployment-manifest.json",
    ]
    missing = [str(path.relative_to(package)) for path in required if not path.is_file()]
    assert not missing, f"missing required package files: {missing}"

    require_arm_elf(app / "bin" / "sprout-launcher")
    require_arm_elf(app / "bin" / "sprout-runtime")

    launch = (app / "launch.sh").read_bytes()
    assert launch.startswith(b"#!/bin/sh\n"), "launch.sh must use an LF shell shebang"
    assert b"\r\n" not in launch, "launch.sh must not contain CRLF"
    assert b"LD_LIBRARY_PATH" in launch, "launch.sh must set its private library path"
    assert b"sprout-launcher" in launch, "launch.sh must start the launcher"

    app_config = json.loads((app / "config.json").read_text(encoding="utf-8"))
    assert app_config["label"] == "Sprout", "Onion app label must be Sprout"

    seed = json.loads((app / "config" / "household-seed.json").read_text(encoding="utf-8"))
    assert {profile["id"] for profile in seed["profiles"]} == {
        "dad",
        "mom",
        "son",
        "daughter",
    }

    manifest_path = package / "deployment-manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert manifest["schemaVersion"] == 1
    assert manifest["appRoot"] == "App/Sprout"
    recorded = manifest["files"]
    actual_files = sorted(
        path.relative_to(package).as_posix()
        for path in package.rglob("*")
        if path.is_file() and path != manifest_path
    )
    assert sorted(recorded) == actual_files, "deployment manifest file set differs from package"
    for relative in actual_files:
        record = recorded[relative]
        path = package / relative
        assert record["size"] == path.stat().st_size, f"size mismatch for {relative}"
        assert record["sha256"] == sha256(path), f"hash mismatch for {relative}"

    checksum_path = package / "deployment-manifest.sha256"
    payload_files = [
        relative
        for relative in actual_files
        if not relative.startswith("deployment-manifest.")
    ]
    expected_checksums = [
        f"{sha256(package / relative)}  {relative}" for relative in payload_files
    ]
    assert checksum_path.read_text(encoding="utf-8").splitlines() == expected_checksums


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--package",
        type=Path,
        default=Path("out/package/onion-sprout"),
    )
    args = parser.parse_args()
    validate(args.package.resolve())
    print("Onion Sprout package contract passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
