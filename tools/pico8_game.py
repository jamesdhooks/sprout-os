#!/usr/bin/env python3
"""Create, check, run, capture, and deploy standalone Sprout PICO-8 games."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
PICO_ROOT = REPO / "pico8"
TEMPLATE_ROOT = PICO_ROOT / "templates" / "basic-game"
SLUG_PATTERN = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
SAVE_PATTERN = re.compile(r"^[a-z0-9_]{1,64}$")
SSH_TARGET_PATTERN = re.compile(r"^[A-Za-z0-9_.@:-]+$")
REMOTE_ROOT_PATTERN = re.compile(r"^/[A-Za-z0-9_./-]+$")


def game_dir(value: str | Path) -> Path:
    candidate = Path(value)
    if candidate.suffix == ".p8":
        return candidate.resolve().parent
    if candidate.is_dir():
        return candidate.resolve()
    return PICO_ROOT / str(value)


def cart_path(value: str | Path) -> Path:
    candidate = Path(value)
    if candidate.suffix == ".p8":
        return candidate.resolve()
    directory = game_dir(value)
    manifest_path = directory / "manifest.json"
    if not manifest_path.is_file():
        raise ValueError(f"missing manifest: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    return directory / manifest["cart"]


def pico_lines(title: str) -> tuple[str, str, str]:
    words = title.upper().split()
    if len(title) <= 20:
        first, second = title.upper(), ""
    else:
        split = max(1, len(words) // 2)
        first, second = " ".join(words[:split]), " ".join(words[split:])
    return first[:24], second[:24], title.upper()[:18]


def render_template(path: Path, replacements: dict[str, str]) -> str:
    source = path.read_text(encoding="utf-8")
    for key, value in replacements.items():
        source = source.replace("{{" + key + "}}", value)
    unresolved = re.findall(r"{{[A-Z0-9_]+}}", source)
    if unresolved:
        raise ValueError(f"unresolved template values in {path}: {unresolved}")
    return source


def scaffold(slug: str, title: str, destination: Path | None = None) -> Path:
    if not SLUG_PATTERN.fullmatch(slug):
        raise ValueError("slug must use lowercase letters, digits, and single hyphens")
    target = destination or (PICO_ROOT / slug)
    if target.exists():
        raise FileExistsError(f"refusing to overwrite {target}")
    save_id = f"sprout_{slug.replace('-', '_')}_dev"
    save_template = f"sprout_{slug.replace('-', '_')}_v1_{{profileHash}}"
    if not SAVE_PATTERN.fullmatch(save_id) or len(save_template.replace("{profileHash}", "0" * 16)) > 64:
        raise ValueError("slug is too long for a legal PICO-8 cartdata identifier")
    line1, line2, short = pico_lines(title)
    values = {
        "GAME_SLUG": slug,
        "GAME_TITLE": title,
        "DEV_SAVE_ID": save_id,
        "PICO_TITLE_LINE_1": line1,
        "PICO_TITLE_LINE_2": line2,
        "PICO_SHORT_TITLE": short,
    }
    target.mkdir(parents=True)
    (target / f"{slug}.p8").write_text(
        render_template(TEMPLATE_ROOT / "cart.p8.tmpl", values),
        encoding="utf-8",
        newline="\n",
    )
    (target / "README.md").write_text(
        render_template(TEMPLATE_ROOT / "README.md.tmpl", values),
        encoding="utf-8",
        newline="\n",
    )
    manifest = {
        "schemaVersion": 1,
        "id": f"pico8:{slug}",
        "title": title,
        "version": "0.1.0",
        "cart": f"{slug}.p8",
        "onionPlatform": "PICO",
        "saveTemplate": save_template,
        "devSaveId": save_id,
        "license": "GPL-3.0-or-later",
        "backend": "fake-08",
        "capabilities": ["profile-scoped-progress"],
    }
    (target / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    return target


def validate(value: str | Path) -> tuple[Path, dict[str, object]]:
    directory = game_dir(value)
    manifest_path = directory / "manifest.json"
    if not manifest_path.is_file():
        raise ValueError(f"missing manifest: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    required = ("schemaVersion", "id", "title", "version", "cart", "saveTemplate")
    missing = [key for key in required if not manifest.get(key)]
    if missing:
        raise ValueError(f"{manifest_path}: missing {missing}")
    cart = directory / str(manifest["cart"])
    if not cart.is_file() or cart.suffix != ".p8":
        raise ValueError(f"missing text cartridge: {cart}")
    source = cart.read_text(encoding="utf-8")
    tokens = (
        "pico-8 cartridge",
        "__lua__",
        "function _update60()",
        "function _draw()",
        '__gfx__',
    )
    absent = [token for token in tokens if token not in source]
    if absent:
        raise ValueError(f"{cart}: missing cartridge contract {absent}")
    if re.search(r"{{[A-Z0-9_]+}}", source):
        raise ValueError(f"{cart}: unresolved template marker")
    save_template = str(manifest["saveTemplate"])
    generated_save = save_template.replace("{profileHash}", "0" * 16)
    if save_template.count("{profileHash}") != 1 or not SAVE_PATTERN.fullmatch(generated_save):
        raise ValueError(f"{manifest_path}: invalid saveTemplate")
    dev_save = str(manifest.get("devSaveId", ""))
    if dev_save:
        markers = (f'cartdata("{dev_save}")', f'arc_boot("{dev_save}")')
        if not SAVE_PATTERN.fullmatch(dev_save) or sum(source.count(marker) for marker in markers) != 1:
            raise ValueError(f"{cart}: devSaveId must identify one cartdata() or arc_boot() call")
    elif source.count("cartdata(") + source.count("arc_boot(") < 1:
        raise ValueError(f"{cart}: no persistent-data initialization found")
    return cart, manifest


def find_pico8(explicit: Path | None) -> Path:
    candidates = [
        explicit,
        Path(os.environ["PICO8_EXE"]) if os.environ.get("PICO8_EXE") else None,
        Path(r"C:\Program Files (x86)\PICO-8\pico8.exe"),
        Path(r"C:\Program Files\PICO-8\pico8.exe"),
    ]
    for candidate in candidates:
        if candidate and candidate.is_file():
            return candidate
    raise FileNotFoundError("PICO-8 executable not found; pass --pico8 or set PICO8_EXE")


def run_game(value: str, executable: Path | None, scale: int) -> None:
    cart, _ = validate(value)
    pico8 = find_pico8(executable)
    size = 128 * scale
    subprocess.Popen(
        [str(pico8), "-windowed", "1", "-width", str(size), "-height", str(size), "-run", str(cart)]
    )


def capture(value: str, state: str, output: Path | None, executable: Path | None) -> Path:
    cart, _ = validate(value)
    destination = output or (REPO / "out" / "pico8-captures" / f"{cart.stem}-{state}.png")
    command = [sys.executable, str(REPO / "tools" / "capture-pico8-arcade.py"), str(cart), state, str(destination)]
    if executable:
        command.extend(["--pico8", str(executable)])
    subprocess.run(command, check=True)
    return destination.resolve()


def deploy(value: str, sd_root: Path, profile_id: str | None) -> Path:
    cart, manifest = validate(value)
    pico_dir = sd_root / "Roms" / "PICO"
    if not pico_dir.is_dir():
        raise ValueError(f"{sd_root} is not an Onion card with Roms/PICO")
    slug = str(manifest["id"]).removeprefix("pico8:")
    device_dir = "".join(part.capitalize() for part in slug.split("-"))
    public_dir = pico_dir / "Sprout" / device_dir
    public_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(cart, public_dir / cart.name)

    catalogue_dir = sd_root / "Sprout" / "catalogue"
    art_dir = catalogue_dir / "art"
    catalogue_dir.mkdir(parents=True, exist_ok=True)
    catalogue_path = catalogue_dir / "pico8.json"
    catalogue = {"schemaVersion": 1, "entries": []}
    if catalogue_path.is_file():
        catalogue = json.loads(catalogue_path.read_text(encoding="utf-8-sig"))
    entry = {
        "id": manifest["id"],
        "title": manifest["title"],
        "cart": f"Sprout/{device_dir}/{cart.name}",
        "version": manifest["version"],
        "profileScoped": True,
    }
    cover = cart.parent / str(manifest.get("libraryArtwork", "library-cover.png"))
    if cover.is_file():
        art_dir.mkdir(parents=True, exist_ok=True)
        art_name = f"pico8-{slug}.png"
        shutil.copy2(cover, art_dir / art_name)
        entry["cover"] = f"Sprout/catalogue/art/{art_name}"
    entries = [item for item in catalogue.get("entries", []) if item.get("id") != entry["id"]]
    entries.append(entry)
    catalogue["entries"] = entries
    catalogue_path.write_text(json.dumps(catalogue, indent=2) + "\n", encoding="utf-8")

    if profile_id:
        dev_save = str(manifest.get("devSaveId", ""))
        if not dev_save:
            raise ValueError("profile deployment requires manifest.devSaveId")
        profile_hash = hashlib.sha256(profile_id.encode()).hexdigest()[:16]
        save_id = str(manifest["saveTemplate"]).replace("{profileHash}", profile_hash)
        source = cart.read_text(encoding="utf-8")
        markers = (f'cartdata("{dev_save}")', f'arc_boot("{dev_save}")')
        matches = [marker for marker in markers if source.count(marker) == 1]
        if len(matches) != 1:
            raise ValueError(f"expected one profile marker for {dev_save}")
        marker = matches[0]
        replacement = marker.replace(dev_save, save_id)
        managed = pico_dir / ".sprout-profiles" / profile_hash
        managed.mkdir(parents=True, exist_ok=True)
        (managed / cart.name).write_text(
            source.replace(marker, replacement), encoding="utf-8", newline="\n"
        )
    return public_dir


def ssh_base(target: str, port: int, identity: Path | None) -> list[str]:
    if target.startswith("-") or not SSH_TARGET_PATTERN.fullmatch(target):
        raise ValueError("SSH target must be [user@]host without spaces")
    command = ["ssh", "-p", str(port)]
    if identity:
        command.extend(["-i", str(identity)])
    command.append(target)
    return command


def read_remote_catalogue(
    target: str, remote_root: str, port: int, identity: Path | None
) -> bytes | None:
    catalogue = f"{remote_root.rstrip('/')}/Sprout/catalogue/pico8.json"
    remote = (
        f"if test -f {shlex.quote(catalogue)}; then "
        f"cat {shlex.quote(catalogue)}; else exit 3; fi"
    )
    result = subprocess.run(
        ssh_base(target, port, identity) + [remote], capture_output=True, check=False
    )
    if result.returncode == 3:
        return None
    if result.returncode:
        raise RuntimeError(result.stderr.decode("utf-8", "replace").strip())
    return result.stdout


def remote_install_script(
    remote_root: str,
    slug: str,
    cart_name: str,
    has_cover: bool,
    profile_hash: str | None,
) -> str:
    if not REMOTE_ROOT_PATTERN.fullmatch(remote_root) or ".." in remote_root.split("/"):
        raise ValueError("remote root must be a simple absolute POSIX path")
    device_dir = "".join(part.capitalize() for part in slug.split("-"))
    root = remote_root.rstrip("/")
    stage = f"{root}/.sprout-staging/pico8-{slug}"
    source_cart = f"{stage}/Roms/PICO/Sprout/{device_dir}/{cart_name}"
    final_dir = f"{root}/Roms/PICO/Sprout/{device_dir}"
    catalogue_source = f"{stage}/Sprout/catalogue/pico8.json"
    catalogue_dir = f"{root}/Sprout/catalogue"
    commands = [
        "set -eu",
        f"test -d {shlex.quote(root + '/Roms/PICO')}",
        f"rm -rf {shlex.quote(stage)}",
        f"mkdir -p {shlex.quote(stage)}",
        f"tar -xf - -C {shlex.quote(stage)}",
        f"mkdir -p {shlex.quote(final_dir)} {shlex.quote(catalogue_dir)}",
        f"cp {shlex.quote(source_cart)} {shlex.quote(final_dir + '/' + cart_name + '.tmp')}",
        f"mv -f {shlex.quote(final_dir + '/' + cart_name + '.tmp')} {shlex.quote(final_dir + '/' + cart_name)}",
        f"cp {shlex.quote(catalogue_source)} {shlex.quote(catalogue_dir + '/pico8.json.tmp')}",
        f"mv -f {shlex.quote(catalogue_dir + '/pico8.json.tmp')} {shlex.quote(catalogue_dir + '/pico8.json')}",
    ]
    if has_cover:
        art_name = f"pico8-{slug}.png"
        art_dir = f"{catalogue_dir}/art"
        art_source = f"{stage}/Sprout/catalogue/art/{art_name}"
        commands.extend(
            [
                f"mkdir -p {shlex.quote(art_dir)}",
                f"cp {shlex.quote(art_source)} {shlex.quote(art_dir + '/' + art_name + '.tmp')}",
                f"mv -f {shlex.quote(art_dir + '/' + art_name + '.tmp')} {shlex.quote(art_dir + '/' + art_name)}",
            ]
        )
    if profile_hash:
        profile_dir = f"{root}/Roms/PICO/.sprout-profiles/{profile_hash}"
        profile_source = f"{stage}/Roms/PICO/.sprout-profiles/{profile_hash}/{cart_name}"
        commands.extend(
            [
                f"mkdir -p {shlex.quote(profile_dir)}",
                f"cp {shlex.quote(profile_source)} {shlex.quote(profile_dir + '/' + cart_name + '.tmp')}",
                f"mv -f {shlex.quote(profile_dir + '/' + cart_name + '.tmp')} {shlex.quote(profile_dir + '/' + cart_name)}",
            ]
        )
    commands.extend(
        [
            f"rm -f {shlex.quote(root + '/Roms/PICO/PICO_cache6.db')}",
            f"rm -rf {shlex.quote(stage)}",
            "sync",
            f"test -s {shlex.quote(final_dir + '/' + cart_name)}",
            f"printf '%s\\n' {shlex.quote(final_dir + '/' + cart_name)}",
        ]
    )
    return "\n".join(commands)


def deploy_ssh(
    value: str,
    target: str,
    remote_root: str,
    port: int,
    identity: Path | None,
    profile_id: str | None,
    dry_run: bool,
) -> None:
    cart, manifest = validate(value)
    slug = str(manifest["id"]).removeprefix("pico8:")
    profile_hash = hashlib.sha256(profile_id.encode()).hexdigest()[:16] if profile_id else None
    cover = cart.parent / str(manifest.get("libraryArtwork", "library-cover.png"))
    install = remote_install_script(
        remote_root, slug, cart.name, cover.is_file(), profile_hash
    )
    if dry_run:
        print(f"target: {target}:{remote_root}")
        print(f"cart: {cart}")
        print(install)
        return

    with tempfile.TemporaryDirectory(prefix="sprout-pico-ssh-") as temporary:
        root = Path(temporary)
        (root / "Roms" / "PICO").mkdir(parents=True)
        remote_catalogue = read_remote_catalogue(target, remote_root, port, identity)
        if remote_catalogue:
            catalogue = root / "Sprout" / "catalogue" / "pico8.json"
            catalogue.parent.mkdir(parents=True, exist_ok=True)
            catalogue.write_bytes(remote_catalogue)
        deploy(value, root, profile_id)
        archive = root / "pico8-deploy.tar"
        with tarfile.open(archive, "w") as bundle:
            bundle.add(root / "Roms", arcname="Roms")
            bundle.add(root / "Sprout", arcname="Sprout")
        with archive.open("rb") as payload:
            subprocess.run(
                ssh_base(target, port, identity) + [install], stdin=payload, check=True
            )


def self_test() -> None:
    with tempfile.TemporaryDirectory(prefix="sprout-pico-kit-") as temporary:
        target = Path(temporary) / "cloud-hop"
        scaffold("cloud-hop", "Cloud Hop", target)
        cart, manifest = validate(target)
        assert cart.name == "cloud-hop.p8"
        assert manifest["id"] == "pico8:cloud-hop"
        assert 'cartdata("sprout_cloud_hop_dev")' in cart.read_text(encoding="utf-8")
        sd_root = Path(temporary) / "sd"
        (sd_root / "Roms" / "PICO").mkdir(parents=True)
        public = deploy(target, sd_root, "child-one")
        assert (public / "cloud-hop.p8").is_file()
        catalogue = json.loads(
            (sd_root / "Sprout" / "catalogue" / "pico8.json").read_text(encoding="utf-8")
        )
        assert catalogue["entries"][0]["id"] == "pico8:cloud-hop"
        profile_hash = hashlib.sha256(b"child-one").hexdigest()[:16]
        profile_cart = sd_root / "Roms" / "PICO" / ".sprout-profiles" / profile_hash / "cloud-hop.p8"
        assert f'sprout_cloud_hop_v1_{profile_hash}' in profile_cart.read_text(encoding="utf-8")
        install = remote_install_script(
            "/mnt/SDCARD", "cloud-hop", "cloud-hop.p8", False, "0" * 16
        )
        assert "/mnt/SDCARD/Roms/PICO/Sprout/CloudHop/cloud-hop.p8" in install
        assert "/mnt/SDCARD/Roms/PICO/.sprout-profiles/0000000000000000" in install
    print("PICO-8 starter kit self-test passed")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    new = commands.add_parser("new", help="create a game from the starter cart")
    new.add_argument("slug")
    new.add_argument("--title", required=True)
    check = commands.add_parser("validate", help="check cart and manifest structure")
    check.add_argument("game")
    run = commands.add_parser("run", help="open the cart in desktop PICO-8")
    run.add_argument("game")
    run.add_argument("--pico8", type=Path)
    run.add_argument("--scale", type=int, choices=range(2, 9), default=4)
    shot = commands.add_parser("capture", help="capture a deterministic QA state")
    shot.add_argument("game")
    shot.add_argument("--state", choices=("title", "gameplay", "win", "fail"), default="gameplay")
    shot.add_argument("--output", type=Path)
    shot.add_argument("--pico8", type=Path)
    copy = commands.add_parser("deploy", help="copy one cart and metadata to an Onion card")
    copy.add_argument("game")
    copy.add_argument("--sd-root", type=Path, required=True)
    copy.add_argument("--profile-id")
    remote = commands.add_parser("deploy-ssh", help="deploy one cart over SSH")
    remote.add_argument("game")
    remote.add_argument("--host", required=True, help="[user@]host, for example onion@192.168.2.72")
    remote.add_argument("--remote-root", default="/mnt/SDCARD")
    remote.add_argument("--port", type=int, default=22)
    remote.add_argument("--identity", type=Path)
    remote.add_argument("--profile-id")
    remote.add_argument("--dry-run", action="store_true")
    commands.add_parser("self-test", help="exercise scaffolding and validation in a temporary directory")
    args = parser.parse_args()

    if args.command == "new":
        print(scaffold(args.slug, args.title).resolve())
    elif args.command == "validate":
        print(f"valid: {validate(args.game)[0]}")
    elif args.command == "run":
        run_game(args.game, args.pico8, args.scale)
    elif args.command == "capture":
        print(capture(args.game, args.state, args.output, args.pico8))
    elif args.command == "deploy":
        print(deploy(args.game, args.sd_root.resolve(), args.profile_id))
    elif args.command == "deploy-ssh":
        deploy_ssh(
            args.game,
            args.host,
            args.remote_root,
            args.port,
            args.identity,
            args.profile_id,
            args.dry_run,
        )
    else:
        self_test()


if __name__ == "__main__":
    main()
