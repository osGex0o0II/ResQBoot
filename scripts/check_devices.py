#!/usr/bin/env python3
"""Validate IPQ device metadata against defconfigs, docs and DTS files."""

from __future__ import annotations

import csv
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEVICES_CSV = ROOT / "docs" / "devices.csv"
DEVICES_MD = ROOT / "docs" / "devices.md"
README = ROOT / "README.md"
CONFIGS_DIR = ROOT / "configs"

TESTED_TO_README = {
    "yes": "✓",
    "no": "×",
    "unknown": "?",
}
RELEASE_ELF_PLATFORMS = {"IPQ40xx", "IPQ806x"}
RELEASE_SLUG_RE = re.compile(r"^[A-Za-z0-9]+(?:-[A-Za-z0-9]+)*$")
ASCII_NAME_COLUMNS = ("device_name", "readme_name", "display_name", "aliases", "release_slug")


def error(errors: list[str], message: str) -> None:
    errors.append(message)


def release_extension(platform: str) -> str:
    return "elf" if platform in RELEASE_ELF_PLATFORMS else "mbn"


def release_asset_name(row: dict[str, str]) -> str:
    return f"{row.get('platform', '')}-{row.get('release_slug', '')}.{release_extension(row.get('platform', ''))}"


def read_devices(errors: list[str]) -> list[dict[str, str]]:
    if not DEVICES_CSV.exists():
        error(errors, f"Missing {DEVICES_CSV.relative_to(ROOT)}")
        return []

    with DEVICES_CSV.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))

    required = {
        "platform",
        "config",
        "device_name",
        "readme_name",
        "display_name",
        "release_slug",
        "machid",
        "tested",
        "dts",
    }
    missing = required - set(rows[0].keys() if rows else [])
    if missing:
        error(errors, f"Missing devices.csv columns: {', '.join(sorted(missing))}")

    seen: set[str] = set()
    for row in rows:
        config = row.get("config", "")
        if not config:
            error(errors, "devices.csv has a row without config")
        elif config in seen:
            error(errors, f"Duplicate devices.csv config: {config}")
        seen.add(config)

        for column in ASCII_NAME_COLUMNS:
            value = row.get(column, "")
            if value and not value.isascii():
                error(errors, f"{config}: {column} must use ASCII/English text for README and release search")

        release_slug = row.get("release_slug", "")
        if not release_slug:
            error(errors, f"{config}: release_slug is required")
        elif not RELEASE_SLUG_RE.fullmatch(release_slug):
            error(errors, f"{config}: release_slug {release_slug!r} must use letters, numbers and hyphens only")

        tested = row.get("tested", "")
        if tested not in TESTED_TO_README:
            error(errors, f"{config}: tested must be one of {', '.join(TESTED_TO_README)}")

    release_assets: dict[str, str] = {}
    for row in rows:
        config = row.get("config", "")
        if not config or not row.get("release_slug"):
            continue
        asset = release_asset_name(row)
        previous = release_assets.get(asset)
        if previous:
            error(errors, f"{config}: release asset {asset} duplicates {previous}")
        release_assets[asset] = config

    return rows


def read_defconfig_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.match(r"^(CONFIG_[A-Z0-9_]+)=(.*)$", line)
        if not match:
            continue
        key, value = match.groups()
        if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
            value = value[1:-1]
        values[key] = value
    return values


def read_device_doc_table(errors: list[str]) -> dict[str, dict[str, str]]:
    if not DEVICES_MD.exists():
        error(errors, f"Missing {DEVICES_MD.relative_to(ROOT)}")
        return {}

    rows: dict[str, dict[str, str]] = {}
    for line in DEVICES_MD.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.startswith("| IPQ"):
            continue

        parts = [part.strip() for part in line.split("|")]
        if len(parts) < 9:
            error(errors, f"Malformed device docs row: {line}")
            continue

        platform, config, device, asset, machid, tested, command = parts[1:8]
        config_name = config.removeprefix("`").removesuffix("`")
        if config_name in rows:
            error(errors, f"Duplicate docs/devices.md config: {config}")
        rows[config_name] = {
            "platform": platform,
            "config": config,
            "device": device,
            "asset": asset,
            "machid": machid,
            "tested": tested,
            "command": command,
        }
    return rows


def normalize_machid_for_readme(value: str) -> str:
    return value.replace(";", "<br>") if value else "---"


def read_dts_machid(path: Path) -> str | None:
    text = path.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"machid\s*=\s*<([^>]+)>", text)
    if not match:
        return None
    return match.group(1).strip().lower()


def validate_defconfigs(devices: list[dict[str, str]], errors: list[str]) -> None:
    expected = {row["config"]: row for row in devices}
    actual = {
        path.name.removesuffix("_defconfig"): path
        for path in sorted(CONFIGS_DIR.glob("ipq*_defconfig"))
    }

    missing = sorted(set(expected) - set(actual))
    extra = sorted(set(actual) - set(expected))
    for config in missing:
        error(errors, f"{config}: listed in devices.csv but missing configs/{config}_defconfig")
    for config in extra:
        error(errors, f"{config}: defconfig exists but is missing from devices.csv")

    for config, row in expected.items():
        path = actual.get(config)
        if not path:
            continue

        values = read_defconfig_values(path)
        display = values.get("CONFIG_BOARD_DISPLAY_NAME")
        if display != row["display_name"]:
            error(
                errors,
                f"{config}: CONFIG_BOARD_DISPLAY_NAME is {display!r}, expected {row['display_name']!r}",
            )


def validate_readme_links(errors: list[str]) -> None:
    if not README.exists():
        error(errors, "Missing README.md")
        return

    text = README.read_text(encoding="utf-8", errors="replace")
    for link in ("docs/devices.md", "docs/build.md", "docs/recovery.md", "releases/latest"):
        if link not in text:
            error(errors, f"README.md is missing link/reference to {link}")


def validate_device_docs(devices: list[dict[str, str]], errors: list[str]) -> None:
    expected = {row["config"]: row for row in devices}
    actual = read_device_doc_table(errors)

    missing = sorted(set(expected) - set(actual))
    extra = sorted(set(actual) - set(expected))
    for config in missing:
        error(errors, f"{config}: listed in devices.csv but missing from docs/devices.md table")
    for config in extra:
        error(errors, f"{config}: docs/devices.md table row is missing from devices.csv")

    for config, row in expected.items():
        docs = actual.get(config)
        if not docs:
            continue

        expected_config = f"`{config}`"
        if docs["config"] != expected_config:
            error(errors, f"{config}: docs config is {docs['config']!r}, expected {expected_config!r}")

        if docs["platform"] != row["platform"]:
            error(errors, f"{config}: docs platform is {docs['platform']!r}, expected {row['platform']!r}")

        if docs["device"] != row["readme_name"]:
            error(errors, f"{config}: docs device name is {docs['device']!r}, expected {row['readme_name']!r}")

        expected_asset = f"`{release_asset_name(row)}`"
        if docs["asset"] != expected_asset:
            error(errors, f"{config}: docs release asset is {docs['asset']!r}, expected {expected_asset!r}")

        expected_machid = normalize_machid_for_readme(row["machid"])
        if docs["machid"] != expected_machid:
            error(errors, f"{config}: docs machid is {docs['machid']!r}, expected {expected_machid!r}")

        expected_tested = TESTED_TO_README[row["tested"]]
        if docs["tested"] != expected_tested:
            error(errors, f"{config}: docs tested is {docs['tested']!r}, expected {expected_tested!r}")

        expected_command = f"`./build.sh {config}`"
        if docs["command"] != expected_command:
            error(errors, f"{config}: docs command is {docs['command']!r}, expected {expected_command!r}")


def validate_dts(devices: list[dict[str, str]], errors: list[str]) -> None:
    for row in devices:
        config = row["config"]
        dts_value = row.get("dts", "")
        if not dts_value:
            continue

        expected_machids = [item.lower() for item in row.get("machid", "").split(";") if item]
        actual_machids: list[str] = []
        for item in dts_value.split(";"):
            path = ROOT / item
            if not path.exists():
                error(errors, f"{config}: DTS path does not exist: {item}")
                continue

            machid = read_dts_machid(path)
            if machid:
                actual_machids.append(machid)

        if expected_machids and actual_machids and expected_machids != actual_machids:
            error(
                errors,
                f"{config}: devices.csv machid {expected_machids!r} does not match DTS machid {actual_machids!r}",
            )


def main() -> int:
    errors: list[str] = []
    devices = read_devices(errors)

    if devices:
        validate_defconfigs(devices, errors)
        validate_readme_links(errors)
        validate_device_docs(devices, errors)
        validate_dts(devices, errors)

    if errors:
        print("Device metadata check failed:")
        for item in errors:
            print(f"  - {item}")
        return 1

    print(f"Device metadata check passed ({len(devices)} devices).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
