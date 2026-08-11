#!/usr/bin/env python3
"""Build a deterministic, hash-manifested Onion Sprout App package."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def copy_file(source: Path, destination: Path) -> None:
    if not source.is_file():
        raise FileNotFoundError(f"Required package input is missing: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)


def source_commit(repo: Path) -> str:
    return subprocess.run(
        ["git", "-C", str(repo), "rev-parse", "HEAD"],
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    ).stdout.strip()


def package(repo: Path, output: Path, household_seed: Path) -> None:
    expected_parent = (repo / "out" / "package").resolve()
    output = output.resolve()
    if output.parent != expected_parent:
        raise RuntimeError(f"Refusing to replace package outside {expected_parent}")
    if output.exists():
        shutil.rmtree(output)

    app = output / "App" / "Sprout"
    build = repo / "out" / "build" / "onion-arm"
    sdk = repo / "out" / "onion-sdk"

    copy_file(build / "launcher" / "sprout-launcher", app / "bin" / "sprout-launcher")
    copy_file(build / "runtime" / "sprout-runtime", app / "bin" / "sprout-runtime")
    copy_file(sdk / "lib" / "libSDL2-2.0.so.0", app / "lib" / "libSDL2-2.0.so.0")
    copy_file(
        sdk / "lib" / "libSDL2_image-2.0.so.0",
        app / "lib" / "libSDL2_image-2.0.so.0",
    )
    copy_file(
        repo / "tools" / "templates" / "onion-sprout-launch.sh",
        app / "launch.sh",
    )
    copy_file(
        repo / "tools" / "templates" / "onion-sprout-config.json",
        app / "config.json",
    )
    copy_file(
        repo / "tools" / "templates" / "containment-enabled",
        app / ".containment-enabled",
    )
    copy_file(
        repo / "tools" / "templates" / "onion-runtime-sprout.sh",
        app / "integration" / "runtime.sh",
    )
    copy_file(
        repo / "tools" / "templates" / "onion-runtime-integration.json",
        app / "integration" / "runtime.json",
    )
    copy_file(household_seed, app / "config" / "household-seed.json")

    launcher_assets = build / "launcher" / "assets"
    if not launcher_assets.is_dir():
        raise FileNotFoundError(f"Launcher assets are missing: {launcher_assets}")
    shutil.copytree(launcher_assets, app / "bin" / "assets")

    # The Miyoo mmiyoo renderer cannot create textures larger than 800x600.
    # Replace only full-screen launcher art with checked-in 640x480 variants;
    # atlases keep their original coordinates and are intentionally untouched.
    miyoo_assets = repo / "launcher" / "assets" / "miyoo"
    for relative in (
        Path("sprout-startup-storybook.png"),
        Path("backgrounds/firefly-evening.png"),
        Path("backgrounds/garden-morning.png"),
        Path("backgrounds/sunny-cove.png"),
        Path("backgrounds/treehouse-library.png"),
    ):
        copy_file(miyoo_assets / relative, app / "bin" / "assets" / relative)

    game = repo / "games" / "blocks-buttons"
    if not game.is_dir():
        raise FileNotFoundError(f"Native game package is missing: {game}")
    shutil.copytree(game, app / "games" / "blocks-buttons")

    payload_files = sorted(
        (candidate for candidate in output.rglob("*") if candidate.is_file()),
        key=lambda path: path.relative_to(output).as_posix(),
    )
    checksum_path = output / "deployment-manifest.sha256"
    with checksum_path.open("w", encoding="utf-8", newline="\n") as handle:
        for path in payload_files:
            relative = path.relative_to(output).as_posix()
            handle.write(f"{sha256(path)}  {relative}\n")

    files = {}
    for path in sorted(candidate for candidate in output.rglob("*") if candidate.is_file()):
        relative = path.relative_to(output).as_posix()
        files[relative] = {
            "size": path.stat().st_size,
            "sha256": sha256(path),
        }
    manifest = {
        "schemaVersion": 1,
        "appRoot": "App/Sprout",
        "sourceCommit": source_commit(repo),
        "onionVersion": "v4.3.1-1",
        "files": files,
    }
    manifest_path = output / "deployment-manifest.json"
    with manifest_path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


def main() -> int:
    repo = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=repo / "out" / "package" / "onion-sprout",
    )
    parser.add_argument("--household-seed", type=Path)
    args = parser.parse_args()
    seed = args.household_seed
    if seed is None:
        private_seed = repo / "config" / "household-seed.json"
        seed = private_seed if private_seed.is_file() else repo / "config" / "household-seed.example.json"
    package(repo, args.output, seed.resolve())
    print(f"Onion Sprout package written to {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
