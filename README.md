# ResQBoot

ResQBoot 是基于 QSDK 12.5 的 Qualcomm IPQ 系列定制 U-Boot 恢复固件。

项目主要面向已支持的 IPQ 路由器和 CPE 设备，提供 Web 恢复、维护和救援刷写能力。

## 快速入口

- [下载最新 Release](https://github.com/osGex0o0II/ResQBoot/releases/latest)
- [支持设备和 Release 资产名](docs/devices.md)
- [从源码构建](docs/build.md)
- [Web 恢复和网络访问](docs/recovery.md)
- [原始项目](https://github.com/1715173329/IPQ_UBOOT)

## 功能特性

- 集成 U-Boot Web failsafe 恢复页面。
- 内置 DHCP 服务，方便直连设备后进入恢复环境。
- 支持通过 Web 页面修改 U-Boot 环境变量。
- 支持 U-Boot、固件、CDT、MIBIB、GPT、ART、initramfs 等镜像的升级和恢复流程。
- 支持通过 `reset_key=<GPIO_NUM>` 覆盖 reset 按键 GPIO，便于未完整适配的设备进入 Web failsafe。
- 支持通过 `config_name=<config@xxx>` 覆盖设备配置名，方便跨机型或跨固件测试。

## 下载固件

Release 资产使用简短、可搜索的英文命名：

```text
<Platform>-<Device>.ext
```

示例：

- `IPQ6018-JDCloud-AX6600.mbn`
- `IPQ5332-JDCloud-BE6500.mbn`
- `IPQ40xx-Aliyun-AP4220.elf`

`IPQ40xx` 和 `IPQ806x` 平台使用 `.elf` 文件，其它 IPQ 平台使用 `.mbn` 文件。

设备型号、构建配置和 Release 资产名的完整对应关系见 [docs/devices.md](docs/devices.md)。

## 恢复入口

默认 Web 恢复地址：

```text
http://192.168.1.1/
```

U-Boot 恢复环境默认网络参数：

- 设备 IP：`192.168.1.1`
- 子网掩码：`255.255.255.0`
- 服务器 IP / 电脑侧地址：`192.168.1.2`

电脑直连设备 LAN 口后，通常可以通过内置 DHCP 获取地址。如果没有自动获取到地址，请手动将电脑设置为 `192.168.1.2/24`，然后打开 `http://192.168.1.1/`。

详细恢复说明见 [docs/recovery.md](docs/recovery.md)。

## 构建预览

先安装依赖并克隆工具链，然后可以按平台或单个配置构建：

```bash
./build.sh ipq6018
./build.sh ipq6018_jdcloud_ax6600
```

完整构建步骤见 [docs/build.md](docs/build.md)。

## 支持平台

| 平台 | 设备数量 |
| --- | ---: |
| IPQ40xx | 5 |
| IPQ5018 | 13 |
| IPQ5332 | 9 |
| IPQ6018 | 14 |
| IPQ806x | 2 |
| IPQ807x | 5 |
| IPQ9574 | 1 |

完整设备索引维护在 [docs/devices.csv](docs/devices.csv)，并渲染到 [docs/devices.md](docs/devices.md)。

## 许可证

本项目使用 GPLv2 许可证，详情见 [LICENSE](LICENSE)。

任何借鉴、Fork、二次分发或二次开发行为，都必须保留原始版权声明、许可证声明和来源说明。

## 免责声明

本项目没有在所有列出的设备上完成充分测试，请自行承担使用风险。

作者和贡献者不对因使用本代码或生成固件导致的硬件损坏、数据丢失、设备故障或其它直接、间接损失负责。
