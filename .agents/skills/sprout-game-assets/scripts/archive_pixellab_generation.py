#!/usr/bin/env python3
"""Archive one completed PixelLab image job without overwriting history."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import tempfile
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import urlparse

from PIL import Image


REPO_ROOT = Path(__file__).resolve().parents[4]
DEFAULT_HISTORY = REPO_ROOT / ".local-work" / "pixellab-history"
SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")


def size_value(value: str) -> tuple[int, int]:
    match = re.fullmatch(r"(\d+)x(\d+)", value.strip().lower())
    if not match:
        raise argparse.ArgumentTypeError("size must use WIDTHxHEIGHT")
    width, height = (int(part) for part in match.groups())
    if width < 1 or height < 1:
        raise argparse.ArgumentTypeError("size dimensions must be positive")
    return width, height


def json_object(value: str) -> dict[str, object]:
    try:
        payload = json.loads(value)
    except json.JSONDecodeError as error:
        raise argparse.ArgumentTypeError(f"invalid metadata JSON: {error}") from error
    if not isinstance(payload, dict):
        raise argparse.ArgumentTypeError("metadata JSON must be an object")
    return payload


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--job-id", required=True)
    parser.add_argument("--download-url", required=True)
    parser.add_argument("--operation", required=True)
    prompt = parser.add_mutually_exclusive_group(required=True)
    prompt.add_argument("--prompt")
    prompt.add_argument("--prompt-file", type=Path)
    parser.add_argument("--source-size", required=True, type=size_value)
    parser.add_argument("--runtime-size", type=size_value)
    parser.add_argument("--seed", type=int)
    parser.add_argument("--metadata-json", type=json_object, default={})
    parser.add_argument("--parent-job-id")
    parser.add_argument("--date", help="UTC archive date in YYYY-MM-DD format")
    parser.add_argument("--history-root", type=Path, default=DEFAULT_HISTORY)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not SAFE_ID.fullmatch(args.job_id):
        raise SystemExit("job ID contains unsafe path characters")
    if args.parent_job_id and not SAFE_ID.fullmatch(args.parent_job_id):
        raise SystemExit("parent job ID contains unsafe path characters")

    parsed_url = urlparse(args.download_url)
    if parsed_url.scheme != "https" or parsed_url.hostname != "api.pixellab.ai":
        raise SystemExit("download URL must use https://api.pixellab.ai/")

    archive_date = args.date or datetime.now(timezone.utc).date().isoformat()
    try:
        datetime.strptime(archive_date, "%Y-%m-%d")
    except ValueError as error:
        raise SystemExit("date must use YYYY-MM-DD") from error

    history_root = args.history_root
    if not history_root.is_absolute():
        history_root = REPO_ROOT / history_root
    history_root = history_root.resolve()
    date_root = history_root / archive_date
    target = date_root / args.job_id
    if target.exists():
        raise SystemExit(f"refusing to overwrite archived job: {target}")

    if args.prompt_file:
        prompt_text = args.prompt_file.read_text(encoding="utf-8").strip()
    else:
        prompt_text = args.prompt.strip()
    if not prompt_text:
        raise SystemExit("prompt must not be empty")

    date_root.mkdir(parents=True, exist_ok=True)
    temp_dir = Path(tempfile.mkdtemp(prefix=f".{args.job_id}.", dir=date_root))
    try:
        source_width, source_height = args.source_size
        source_name = f"source-{source_width}x{source_height}.png"
        source_path = temp_dir / source_name
        request = urllib.request.Request(
            args.download_url,
            headers={"User-Agent": "Sprout-Asset-Archive/1"},
        )
        with urllib.request.urlopen(request, timeout=60) as response:
            source_path.write_bytes(response.read())

        with Image.open(source_path) as opened:
            image = opened.convert("RGBA")
        if image.size != args.source_size:
            raise SystemExit(
                f"downloaded image is {image.size[0]}x{image.size[1]}, "
                f"expected {source_width}x{source_height}"
            )

        factor = max(1, min(8, 640 // max(source_width, source_height)))
        review_size = (source_width * factor, source_height * factor)
        review_name = f"review-{review_size[0]}x{review_size[1]}.png"
        image.resize(review_size, Image.Resampling.NEAREST).save(
            temp_dir / review_name,
            format="PNG",
        )

        alpha_range = image.getextrema()[3]
        colours = image.getcolors(maxcolors=1_000_000)
        record = {
            "schemaVersion": 1,
            "provider": "PixelLab",
            "operation": args.operation,
            "jobId": args.job_id,
            "parentJobId": args.parent_job_id,
            "archivedAt": datetime.now(timezone.utc).isoformat(),
            "prompt": prompt_text,
            "seed": args.seed,
            "downloadUrl": args.download_url,
            "sourceFile": source_name,
            "sourceDimensions": list(args.source_size),
            "runtimeDimensions": list(args.runtime_size) if args.runtime_size else None,
            "reviewFile": review_name,
            "sha256": hashlib.sha256(source_path.read_bytes()).hexdigest(),
            "alphaRange": list(alpha_range),
            "rgbaColourCount": len(colours) if colours is not None else None,
            "parameters": args.metadata_json,
        }
        (temp_dir / "generation.json").write_text(
            json.dumps(record, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        temp_dir.rename(target)
    except BaseException:
        shutil.rmtree(temp_dir, ignore_errors=True)
        raise

    print(target)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
