# ResQBoot

ResQBoot is a custom U-Boot recovery build for Qualcomm IPQ devices, based on QSDK 12.5.

It focuses on web recovery and maintenance workflows for supported IPQ routers and CPE devices.

## Quick Links

- [Download the latest Release](https://github.com/osGex0o0II/ResQBoot/releases/latest)
- [Supported devices and Release asset names](docs/devices.md)
- [Build from source](docs/build.md)
- [Web recovery and network access](docs/recovery.md)
- [Original project](https://github.com/1715173329/IPQ_UBOOT)

## Features

- Web failsafe recovery page in U-Boot.
- Built-in DHCP server for recovery access.
- Web UI for editing U-Boot environment variables.
- Upgrade and recovery flows for U-Boot, firmware, CDT, MIBIB, GPT, ART and initramfs images.
- Optional `reset_key=<GPIO_NUM>` override for devices without a built-in reset key definition.
- Optional `config_name=<config@xxx>` override for cross-model testing.

## Downloading Firmware

Release assets use short searchable English names:

```text
<Platform>-<Device>.ext
```

Examples:

- `IPQ6018-JDCloud-AX6600.mbn`
- `IPQ5332-JDCloud-BE6500.mbn`
- `IPQ40xx-Aliyun-AP4220.elf`

Use `.elf` assets for `IPQ40xx` and `IPQ806x`. Use `.mbn` assets for other IPQ platforms.

See [docs/devices.md](docs/devices.md) for the full mapping between device model, build config and Release asset name.

## Recovery Access

Default web recovery address:

```text
http://192.168.1.1/
```

The U-Boot recovery environment uses:

- Device IP: `192.168.1.1`
- Netmask: `255.255.255.0`
- Server IP / host side: `192.168.1.2`

Connect your computer directly to the device LAN port. If DHCP does not assign an address, set your computer manually to an address such as `192.168.1.2/24`, then open `http://192.168.1.1/`.

More details are in [docs/recovery.md](docs/recovery.md).

## Build Preview

Install dependencies and clone the toolchain first, then build a platform or a single config:

```bash
./build.sh ipq6018
./build.sh ipq6018_jdcloud_ax6600
```

Detailed setup instructions are in [docs/build.md](docs/build.md).

## Supported Platforms

| Platform | Devices |
| --- | ---: |
| IPQ40xx | 5 |
| IPQ5018 | 13 |
| IPQ5332 | 9 |
| IPQ6018 | 14 |
| IPQ806x | 2 |
| IPQ807x | 5 |
| IPQ9574 | 1 |

The complete device index is maintained in [docs/devices.csv](docs/devices.csv) and rendered in [docs/devices.md](docs/devices.md).

## License

This project is licensed under GPLv2. See [LICENSE](LICENSE) for details.

Any borrowed, forked or redistributed work must retain the original copyright notices, license notices and source attribution.

## Disclaimer

This project is not fully tested on every listed device. Use it at your own risk.

The author and contributors are not responsible for hardware damage, data loss, device failure or other direct or indirect damage caused by use of this code or generated firmware.
