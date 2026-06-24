# Build Guide

This document describes how to build ResQBoot locally.

## Requirements

Ubuntu 20.04 LTS or newer is recommended.

Install build dependencies:

```bash
sudo apt-get update
sudo apt-get install -y build-essential libncurses5-dev gawk git gettext libssl-dev python3 wget cpio flex bison bc rsync nodejs npm gzip zopfli device-tree-compiler
```

Install the web UI compression tools used by `makefsdatac`:

```bash
sudo npm install -g html-minifier-terser clean-css terser
```

## Source And Toolchain

Clone this repository and the toolchain side by side:

```bash
git clone https://github.com/osGex0o0II/ResQBoot.git
git clone https://github.com/1980490718/toolchain-arm_cortex-a7_gcc-5.2.0.git staging_dir
cd ResQBoot
```

The build scripts expect:

```text
STAGING_DIR=../staging_dir/
CROSS_COMPILE=arm-openwrt-linux-
ARCH=arm
```

These values are set automatically by the build script.

## Build Commands

Build all devices for one platform:

```bash
./build.sh ipq6018
```

Build one device config:

```bash
./build.sh ipq6018_jdcloud_ax6600
```

Clean generated files:

```bash
./build.sh clean
```

Clean output files only:

```bash
./build.sh clean_all
```

## Output Files

- `IPQ40xx` and `IPQ806x`: `bin/*.elf`
- Other IPQ platforms: `bin/*.mbn`

Release assets are renamed by CI to the shorter searchable format shown in [devices.md](devices.md).

## Common Errors

If you see an error like:

```text
httpd/fs.c:54:20: fatal error: fsdata.c: No such file or directory
```

Check that Node.js and these npm packages are installed:

```bash
html-minifier-terser clean-css terser
```

## CI Builds

GitHub Actions builds all IPQ platforms on pushes to `main` and creates a daily Release tag such as `v2026-06-24`.

The Release job:

- downloads platform build artifacts,
- prepares short asset names from `docs/devices.csv`,
- writes a Device Asset Index into the Release body,
- deletes the existing same-day Release tag before uploading fresh assets.
