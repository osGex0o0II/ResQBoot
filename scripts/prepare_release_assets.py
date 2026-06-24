#!/usr/bin/env python3
"""Prepare short, searchable release asset names from build artifacts."""

from __future__ import annotations

import argparse
import csv
import os
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEVICES_CSV = ROOT / "docs" / "devices.csv"
ELF_PLATFORMS = {"IPQ40xx", "IPQ806x"}


def artifact_extension(platform: str) -> str:
    return "elf" if platform in ELF_PLATFORMS else "mbn"


def original_artifact_name(row: dict[str, str]) -> str:
    ext = artifact_extension(row["platform"])
    return f"openwrt-{row['config']}-u-boot.{ext}"


def release_artifact_name(row: dict[str, str]) -> str:
    ext = artifact_extension(row["platform"])
    return f"{row['platform']}-{row['release_slug']}.{ext}"


def read_devices() -> list[dict[str, str]]:
    with DEVICES_CSV.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def clean_directory(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True)


def markdown_value(value: str) -> str:
    return value or "-"


def tested_label(value: str) -> str:
    return {
        "yes": "yes",
        "no": "no",
        "unknown": "unknown",
    }.get(value, value)


def write_release_body(path: Path, devices: list[dict[str, str]]) -> None:
    build_time = os.environ.get("FULL_BUILD_TIME", "")
    timezone = os.environ.get("TZ", "Asia/Shanghai")

    lines = [
        "Compiled U-Boot IPQ platforms.",
        "",
    ]
    if build_time:
        lines.extend([f"Build Time: {build_time} {timezone}", ""])

    lines.extend(
        [
            "## Device Asset Index",
            "",
            "| Platform | Asset | Device Model | Config | Aliases | Tested |",
            "| --- | --- | --- | --- | --- | --- |",
        ]
    )

    for row in devices:
        asset = release_artifact_name(row)
        lines.append(
            "| {platform} | `{asset}` | {model} | `{config}` | {aliases} | {tested} |".format(
                platform=row["platform"],
                asset=asset,
                model=row["readme_name"],
                config=row["config"],
                aliases=markdown_value(row.get("aliases", "")),
                tested=tested_label(row["tested"]),
            )
        )

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def prepare_assets(artifacts_dir: Path, output_dir: Path, devices: list[dict[str, str]]) -> list[str]:
    if artifacts_dir.resolve() == output_dir.resolve():
        return ["artifacts-dir and output-dir must be different directories"]

    clean_directory(output_dir)

    errors: list[str] = []
    used_names: dict[str, str] = {}
    for row in devices:
        source = artifacts_dir / original_artifact_name(row)
        target_name = release_artifact_name(row)
        target = output_dir / target_name

        previous = used_names.get(target_name)
        if previous:
            errors.append(f"{row['config']}: release asset {target_name} duplicates {previous}")
            continue
        used_names[target_name] = row["config"]

        if not source.exists():
            errors.append(f"{row['config']}: missing build artifact {source}")
            continue

        shutil.copy2(source, target)

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts-dir", default="artifacts", type=Path)
    parser.add_argument("--output-dir", default="release-assets", type=Path)
    parser.add_argument("--body", default="release-notes.md", type=Path)
    args = parser.parse_args()

    devices = read_devices()
    errors = prepare_assets(args.artifacts_dir, args.output_dir, devices)
    write_release_body(args.body, devices)

    if errors:
        print("Release asset preparation failed:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print(f"Prepared {len(devices)} release assets in {args.output_dir}")
    print(f"Release body written to {args.body}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
