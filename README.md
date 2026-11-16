<p align="center">
  <img src="docs/images/resqboot-logo.svg" width="96" height="96" alt="ResQBoot 标志" />
</p>

<h1 align="center">ResQBoot</h1>

<p align="center">基于 QSDK 12.5 的 Qualcomm IPQ 系列定制 U-Boot<br />内置 Web 恢复、分区维护与固件刷写</p>

<p align="center">
  <a href="https://github.com/osGex0o0II/ResQBoot/actions/workflows/build-ipq-uboot.yml"><img src="https://img.shields.io/github/actions/workflow/status/osGex0o0II/ResQBoot/build-ipq-uboot.yml?branch=main&amp;label=build&amp;style=flat-square&amp;labelColor=333333" alt="Build status" /></a>
  <img src="https://img.shields.io/badge/QSDK-12.5-555555?style=flat-square&amp;labelColor=333333" alt="QSDK 12.5" />
  <img src="https://img.shields.io/badge/license-GPLv2-555555?style=flat-square&amp;labelColor=333333" alt="GPLv2" />
  <img src="https://img.shields.io/badge/IPQ-40xx--9574-555555?style=flat-square&amp;labelColor=333333" alt="IPQ 40xx-9574" />
</p>

<p align="center">
  <a href="https://github.com/osGex0o0II/ResQBoot/releases/latest">下载</a> ·
  <a href="docs/recovery.md">Web 恢复</a> ·
  <a href="docs/build.md">构建指南</a> ·
  <a href="docs/devices.md">设备支持</a> ·
  <a href="docs/visual-guidelines.md">品牌规范</a>
</p>

ResQBoot 面向 Qualcomm IPQ 系列路由器与网关，提供可在浏览器中完成恢复、分区维护和镜像刷写的 U-Boot 恢复环境。

## 功能

- 无需进入操作系统，通过 Web 页面恢复设备
- 刷写固件、U-Boot、ART、CDT、MIBIB、GPT 等镜像
- 以 FIT 格式 initramfs 在内存中启动临时系统
- 查看设备信息，配置 `reset_key` 与 `config_name`
- 可选 DHCP 服务、命令终端和明暗主题

具体功能取决于设备配置、闪存类型和分区布局。

## 快速开始

1. 从[最新 Release](https://github.com/osGex0o0II/ResQBoot/releases/latest) 下载与设备匹配的资产，并使用 `SHA256SUMS` 核对 SHA-256。
2. 在 U-Boot 控制台执行 `httpd`，将电脑连接到设备 LAN 口，访问 `http://192.168.1.1/`。无法自动获取 IP 时，将电脑设为 `192.168.1.2/24`。
3. 在 Web 页面备份关键分区，核对设备型号与镜像类型后刷写。写入期间切勿断电。

完整步骤见 [Web 恢复指南](docs/recovery.md)。

## 设备支持

覆盖 IPQ40xx、IPQ5018、IPQ5332、IPQ6018、IPQ806x、IPQ807x 和 IPQ9574。完整的配置表、Mach ID、Release 资产命名和测试标记见 [设备与构建配置](docs/devices.md)。

测试标记不代表对应资产已通过本次 Release 的实机验证。移植配置请先确认设备树、交换机初始化和实机刷写，例如 MD16 尚未验证 YT921X 交换机初始化。

## 源码构建

```bash
git clone https://github.com/osGex0o0II/ResQBoot.git
git clone https://github.com/1980490718/toolchain-arm_cortex-a7_gcc-5.2.0.git staging_dir
cd ResQBoot
./build.sh ipq6018
```

依赖安装、清理规则、产物命名和 CI 发布流程见 [构建指南](docs/build.md)。

## 文档

| 文档 | 内容 |
| --- | --- |
| [Web 恢复指南](docs/recovery.md) | 启动 `httpd`、网络配置、刷写与故障排查 |
| [构建指南](docs/build.md) | 依赖、工具链、构建与自动发布 |
| [设备与构建配置](docs/devices.md) | 平台、配置、Mach ID 与 Release 资产 |
| [品牌规范](docs/visual-guidelines.md) | 标志、颜色、字体与应用规范 |
| [安全策略](SECURITY.md) | 支持范围与漏洞报告流程 |
| [贡献指南](CONTRIBUTING.md) | 分支、提交、测试与文档规范 |
| [文档索引](docs/README.md) | 全部文档入口 |

## 安全与使用须知

- Web 恢复使用未认证的明文 HTTP。写入请求携带 `X-ResQBoot-Request: 1` 头，用于阻止普通跨站表单提交，但不构成身份认证；仅应在隔离的可信网络中使用。
- 刷写 U-Boot、ART、CDT、MIBIB、GPT 等关键分区前，请备份原始数据，并核对设备型号、闪存类型和分区布局。操作不当可能导致设备无法启动或校准数据丢失。
- 备分区写入必须在每次提交时显式启用。主分区可能已写入而备分区失败，请根据提示确认结果。
- 并非所有构建配置均经过实机验证。固件按现状提供，不保证适用于特定设备或无故障运行。
- 安全问题请通过 [GitHub Security Advisories](https://github.com/osGex0o0II/ResQBoot/security/advisories/new) 私下报告，不要公开提交 issue。

## 品牌

主标志是一枚终端块光标：字标末尾的蓝色横条与图标中的蓝色竖条是同一形态的两种方向。它对应 U-Boot 控制台与恢复终端的原生语境，不使用盾牌、字母组合或装饰性图形。仓库提供浅色、深色、透明标志和字标，使用规范见 [品牌规范](docs/visual-guidelines.md)。

## 许可证与来源

项目以 [GPLv2](LICENSE) 为主要许可证，部分文件另有许可声明；复制、修改和分发须遵守源文件及[许可说明](Licenses/README)中的适用条款。

源自 [1980490718/u-boot-2016](https://github.com/1980490718/u-boot-2016)。`README_ORIG` 为历史 U-Boot 文档，不代表当前支持范围。
