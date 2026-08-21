"""Fast, verified UI-only deployment for an already-installed Sprout Miyoo.

Credentials are intentionally supplied from the environment, never stored in
the repository. The script is read-only with --check and changes device files
only with --deploy-ui.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import tarfile
import time
from pathlib import Path

try:
    import paramiko
except ModuleNotFoundError:
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "out" / "ssh-deps"))
    import paramiko


ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "out" / "package" / "onion-sprout"
ARCHIVE = ROOT / "out" / "miyoo-ui-refresh.tar.gz"
RELATIVE_BINARY = Path("App/Sprout/bin/sprout-launcher")


def connect() -> paramiko.SSHClient:
    host = os.environ.get("MIYOO_HOST", "192.168.2.72")
    key_path = os.environ.get("MIYOO_SSH_KEY")
    if not key_path:
        raise SystemExit("Set MIYOO_SSH_KEY to the device private key path.")
    key = paramiko.Ed25519Key.from_private_key_file(key_path)
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(host, username="root", pkey=key, timeout=10,
                   allow_agent=False, look_for_keys=False)
    return client


def remote_output(client: paramiko.SSHClient, command: str) -> str:
    _, output, errors = client.exec_command(command)
    result = output.read().decode().strip()
    error = errors.read().decode().strip()
    if error:
        raise RuntimeError(error)
    return result


def local_hash() -> str:
    binary = PACKAGE / RELATIVE_BINARY
    if not binary.is_file():
        raise SystemExit("Package the Onion build before deploying.")
    return hashlib.sha256(binary.read_bytes()).hexdigest()


def live_hash(client: paramiko.SSHClient) -> str:
    return remote_output(client, "sha256sum /mnt/SDCARD/App/Sprout/bin/sprout-launcher").split()[0]


def make_archive() -> None:
    binary = PACKAGE / RELATIVE_BINARY
    icons = PACKAGE / "App" / "Sprout" / "bin" / "assets" / "icons"
    with tarfile.open(ARCHIVE, "w:gz") as archive:
        archive.add(binary, RELATIVE_BINARY.as_posix())
        if icons.is_dir():
            archive.add(icons, "App/Sprout/bin/assets/icons")


def deploy() -> None:
    expected = local_hash()
    make_archive()
    client = connect()
    try:
        sftp = client.open_sftp()
        try:
            sftp.put(str(ARCHIVE), "/mnt/SDCARD/.tmp_update/sprout-ui-refresh.tar.gz")
        finally:
            sftp.close()
        remote_output(client, "set -eu; tar -xzf /mnt/SDCARD/.tmp_update/sprout-ui-refresh.tar.gz -C /mnt/SDCARD; sync; reboot")
    finally:
        client.close()

    deadline = time.monotonic() + 90
    while time.monotonic() < deadline:
        time.sleep(3)
        try:
            client = connect()
            try:
                actual = live_hash(client)
            finally:
                client.close()
            if actual != expected:
                raise SystemExit(f"Live launcher hash mismatch: {actual} != {expected}")
            print(f"miyoo-ui-deploy-verified {actual}")
            return
        except (OSError, paramiko.SSHException):
            continue
    raise SystemExit("Device did not return to SSH within 90 seconds.")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--deploy-ui", action="store_true")
    args = parser.parse_args()
    if args.check == args.deploy_ui:
        parser.error("Choose exactly one of --check or --deploy-ui.")
    if args.check:
        client = connect()
        try:
            print(f"miyoo-ssh-ok {live_hash(client)}")
        finally:
            client.close()
        return
    deploy()


if __name__ == "__main__":
    main()
