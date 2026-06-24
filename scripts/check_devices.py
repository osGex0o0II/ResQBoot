#!/usr/bin/env python3
"""Validate IPQ device metadata against defconfigs, README and DTS files."""

from __future__ import annotations

import csv
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEVICES_CSV = ROOT / "docs" / "devices.csv"
README = ROOT / "README.md"
CONFIGS_DIR = ROOT / "configs"

TESTED_TO_README = {
    "yes": "✓",
    "no": "×",
    "unknown": "?",
}


def error(errors: list[str], message: str) -> None:
    errors.append(message)


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

        tested = row.get("tested", "")
        if tested not in TESTED_TO_README:
            error(errors, f"{config}: tested must be one of {', '.join(TESTED_TO_README)}")

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


def read_readme_table(errors: list[str]) -> dict[str, dict[str, str]]:
    if not README.exists():
        error(errors, "Missing README.md")
        return {}

    rows: dict[str, dict[str, str]] = {}
    for line in README.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.startswith("| IPQ"):
            continue

        parts = [part.strip() for part in line.split("|")]
        if len(parts) < 7:
            error(errors, f"Malformed README device row: {line}")
            continue

        platform, config, device, machid, tested, command = parts[1:7]
        if config in rows:
            error(errors, f"Duplicate README config: {config}")
        rows[config] = {
            "platform": platform,
            "device": device,
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


def validate_readme(devices: list[dict[str, str]], errors: list[str]) -> None:
    expected = {row["config"]: row for row in devices}
    actual = read_readme_table(errors)

    missing = sorted(set(expected) - set(actual))
    extra = sorted(set(actual) - set(expected))
    for config in missing:
        error(errors, f"{config}: listed in devices.csv but missing from README device table")
    for config in extra:
        error(errors, f"{config}: README device table row is missing from devices.csv")

    for config, row in expected.items():
        readme = actual.get(config)
        if not readme:
            continue

        if readme["platform"] != row["platform"]:
            error(errors, f"{config}: README platform is {readme['platform']!r}, expected {row['platform']!r}")

        if readme["device"] != row["readme_name"]:
            error(errors, f"{config}: README device name is {readme['device']!r}, expected {row['readme_name']!r}")

        expected_machid = normalize_machid_for_readme(row["machid"])
        if readme["machid"] != expected_machid:
            error(errors, f"{config}: README machid is {readme['machid']!r}, expected {expected_machid!r}")

        expected_tested = TESTED_TO_README[row["tested"]]
        if readme["tested"] != expected_tested:
            error(errors, f"{config}: README tested is {readme['tested']!r}, expected {expected_tested!r}")

        expected_command = f"`./build.sh {config}`"
        if readme["command"] != expected_command:
            error(errors, f"{config}: README command is {readme['command']!r}, expected {expected_command!r}")


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
        validate_readme(devices, errors)
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
