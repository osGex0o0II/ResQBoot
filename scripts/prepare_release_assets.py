#!/usr/bin/env python3
"""Prepare short, searchable release asset names from build artifacts."""

from __future__ import annotations

import argparse
import csv
import hashlib
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


def write_release_body(path: Path, devices: list[dict[str, str]]) -> None:
    build_time = os.environ.get("FULL_BUILD_TIME", "")
    timezone = os.environ.get("TZ", "Asia/Shanghai")
    platform_count = len({row["platform"] for row in devices})

    lines = ["ResQBoot U-Boot 构建", ""]
    if build_time:
        lines.extend([f"构建时间：{build_time} {timezone}", ""])

    commit = os.environ.get("GITHUB_SHA", "main")
    index_url = f"https://github.com/osGex0o0II/ResQBoot/blob/{commit}/docs/devices.md"

    lines.extend([
        "## 下载与校验",
        "",
        f"本次包含 {platform_count} 个平台、{len(devices)} 个构建配置（含通用配置）。",
        f"根据 [设备与构建配置]({index_url}) 选择对应资产，并使用 `SHA256SUMS` 校验 SHA-256。",
        "",
        "## 验证范围",
        "",
        "CI 已完成设备元数据检查及跨平台编译。",
        "设备测试标记不代表本次发布已完成实机启动、刷写或恢复验证。",
        "",
    ])

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def write_checksums(output_dir: Path) -> None:
    lines = []
    for asset in sorted(output_dir.iterdir()):
        if not asset.is_file() or asset.name == "SHA256SUMS":
            continue
        digest = hashlib.sha256()
        with asset.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(chunk)
        lines.append(f"{digest.hexdigest()}  {asset.name}")
    (output_dir / "SHA256SUMS").write_text("\n".join(lines) + "\n", encoding="utf-8")


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
    if not errors:
        write_checksums(args.output_dir)

    if errors:
        print("Release asset preparation failed:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print(f"Prepared {len(devices)} release assets plus SHA256SUMS in {args.output_dir}")
    print(f"Release body written to {args.body}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
