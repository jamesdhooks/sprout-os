#!/usr/bin/env python3
"""Validate the deployable Onion Sprout vertical-slice package."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import subprocess
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


def png_dimensions(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    assert header.startswith(b"\x89PNG\r\n\x1a\n"), f"{path} is not PNG"
    return struct.unpack(">II", header[16:24])


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
        app / ".containment-enabled",
        app / "integration" / "runtime.sh",
        app / "integration" / "runtime.json",
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
    assert b"SDL_VIDEODRIVER=mmiyoo" in launch
    assert b"SDL_RENDER_DRIVER" not in launch
    assert b"SDL_AUDIODRIVER=mmiyoo" in launch
    assert b"EGL_VIDEODRIVER=mmiyoo" in launch
    assert b"SPROUT_DIRECT_FRAMEBUFFER=/dev/fb0" in launch
    assert b"kill -STOP" in launch
    assert b"kill -CONT" in launch
    assert b"stop_audioserver.sh" in launch
    assert b"killall -2 l" not in launch
    assert b"killall -9 disp_init" not in launch
    assert b"killall disp_init" not in launch
    assert b"disp_init >/dev/null" not in launch
    assert b"disp_init &" not in launch
    assert b"sprout-launcher" in launch, "launch.sh must start the launcher"
    assert b'SPROUT_CONTAINED=1' in launch
    assert b'ONION_RUNTIME_ROOT="${SPROUT_ONION_RUNTIME_ROOT:-/mnt/SDCARD/.tmp_update}"' in launch
    assert b'export SPROUT_ONION_RUNTIME_ROOT="$ONION_RUNTIME_ROOT"' in launch
    assert b'SPROUT_LOCK_DIR="${SPROUT_LOCK_DIR:-/tmp/sprout-launcher.lock}"' in launch
    assert b"SPROUT_EXIT_MARKER=" in launch
    assert b"SPROUT_LOCK_DIR=" in launch, "launch.sh must define a card-local single-instance lock"
    assert b"mkdir \"$SPROUT_LOCK_DIR\"" in launch, "launch.sh must atomically acquire its lock"
    assert b"duplicate-launch-refused" in launch, "launch.sh must log a refused duplicate launch"
    lock_acquisition = launch.index(b"mkdir \"$SPROUT_LOCK_DIR\"")
    launcher_start = launch.index(b'"$APP_ROOT/bin/sprout-launcher"')
    onion_pause = launch.index(b"kill -STOP")
    assert lock_acquisition < onion_pause < launcher_start, (
        "single-instance lock must be acquired before Onion pause or Sprout framebuffer access"
    )
    assert b"rmdir \"$SPROUT_LOCK_DIR\"" in launch, "launch.sh must release its lock on exit"
    assert b"unapproved-exit-restarting" in launch
    assert b'"$STATUS" -eq 75' in launch
    assert b"sprout-stock-onion-session" in launch
    assert b'exec "$APP_ROOT/bin/sprout-launcher"' not in launch, (
        "wrapper must remain alive so EXIT/INT/TERM cleanup can resume Onion"
    )
    subprocess.run(
        [
            "sh",
            str(Path(__file__).with_name("onion_wrapper_lifecycle_test.sh")),
            str(app / "launch.sh"),
        ],
        check=True,
    )

    runtime_path = app / "integration" / "runtime.sh"
    runtime = runtime_path.read_bytes()
    assert runtime.startswith(b"#!/bin/sh\n")
    assert b"\r\n" not in runtime
    assert b"sprout_containment_enabled" in runtime
    assert b".sprout-handoff" in runtime
    assert b"queue_sprout" in runtime
    assert b"allow-stock-onion" in runtime
    assert b"/tmp/run_advmenu" in runtime
    auto_launch = runtime.index(b"    # Auto launch.")
    startup_app = runtime.index(b"    state_change check_switcher\n", auto_launch)
    boot_router = runtime[auto_launch:startup_app]
    assert b"if sprout_containment_enabled; then" in boot_router
    assert b'rm -f "$sysdir/cmd_to_run.sh"' in boot_router
    assert b'rm -f "$sysdir/.runGameSwitcher"' in boot_router
    assert b"rm -f /tmp/quick_switch /tmp/run_advmenu" in boot_router

    integration = json.loads(
        (app / "integration" / "runtime.json").read_text(encoding="utf-8")
    )
    assert integration["schemaVersion"] == 1
    assert integration["onionVersion"] == "v4.3.1-1"
    assert integration["target"] == "/mnt/SDCARD/.tmp_update/runtime.sh"
    assert integration["baseSha256"] == (
        "a8d77dcd316bc2a323b1e015aaf4b7682d2fed677af9cdadbc00e48881425d6e"
    )
    assert integration["containedSha256"] == sha256(runtime_path)
    assert integration["previousContainedSha256"] == [
        "de44ce5c9c55671049510ea43cc9e0358d82f5071b5a9f3be0a478341b9b2b6e"
    ]
    assert integration["maintenanceFlag"] == (
        "/mnt/SDCARD/sprout-dev/maintenance/allow-stock-onion"
    )
    assert integration["bootSessionParentExitMarker"] == (
        "/tmp/sprout-stock-onion-session"
    )

    for relative in (
        "sprout-startup-storybook.png",
        "backgrounds/firefly-evening.png",
        "backgrounds/garden-morning.png",
        "backgrounds/sunny-cove.png",
        "backgrounds/treehouse-library.png",
        "fonts/nunito-extrabold.png",
        "fonts/nunito-semibold.png",
    ):
        width, height = png_dimensions(app / "bin" / "assets" / relative)
        assert width <= 800 and height <= 600, (
            f"Miyoo launcher texture exceeds 800x600: {relative} ({width}x{height})"
        )

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
