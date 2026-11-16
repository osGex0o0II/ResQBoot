#!/usr/bin/env python3
"""Ensure build.sh clean does not remove user-provided images and patches."""

import os
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build.sh"


def put(root: Path, name: str) -> Path:
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"fixture")
    return path


with tempfile.TemporaryDirectory(prefix="resqboot-clean-") as temp:
    directory = Path(temp)
    script = directory / "build.sh"
    shutil.copyfile(BUILD, script)
    script.chmod(0o755)
    kept = [
        put(directory, "docs/notes.patch"),
        put(directory, "user-image.bin"),
        put(directory, "firmware.elf"),
        put(directory, "u-boot-notes.txt"),
        put(directory, "docs/kept.o"),
        put(directory, ".github/example.o"),
    ]
    removed = [
        put(directory, "lib/test.o"),
        put(directory, "lib/.test.o.cmd"),
        put(directory, "bin/openwrt-ipq5332-fixture.mbn"),
        put(directory, "dtb_combined.bin.gz"),
    ]
    result = subprocess.run(
        ["bash", str(script), "clean"],
        cwd=directory, capture_output=True, text=True, check=False,
    )
    if result.returncode:
        raise SystemExit(f"clean failed: {result.stdout}\n{result.stderr}")
    if any(not p.exists() for p in kept):
        raise SystemExit("clean removed a protected input file")
    if any(p.exists() for p in removed):
        raise SystemExit("clean did not remove a generated file")

    output = put(directory, "bin/openwrt-ipq5332-fixture.mbn")
    obj = put(directory, "lib/test.o")
    result = subprocess.run(
        ["bash", str(script), "clean_all"],
        cwd=directory, capture_output=True, text=True, check=False,
    )
    if result.returncode or output.exists() or not obj.exists():
        raise SystemExit("clean_all affected files outside the output directory")

print("Build cleanup safety checks passed")
