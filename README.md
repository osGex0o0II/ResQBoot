<h1 align="center">ResQBoot</h1>

<p align="center">
  <a href="https://github.com/osGex0o0II/ResQBoot/actions/workflows/build-ipq-uboot.yml"><img src="https://img.shields.io/github/actions/workflow/status/osGex0o0II/ResQBoot/build-ipq-uboot.yml?branch=main&amp;label=build&amp;style=flat-square" alt="Build status" /></a>
  <img src="https://img.shields.io/badge/QSDK-12.5-555555?style=flat-square" alt="QSDK 12.5" />
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPLv2-555555?style=flat-square" alt="GPLv2" /></a>
</p>

基于 QSDK 12.5 的 Qualcomm IPQ 系列定制 U-Boot，提供 Web 恢复和固件刷写。

## 功能

- Web 恢复界面、DHCP 服务与命令终端
- 固件和 U-Boot 刷写，以及 ART、CDT、MIBIB、GPT 等分区操作
- 通过 Web 页面配置 `reset_key` 和 `config_name`
- FIT 格式 initramfs 内存启动

具体功能取决于设备配置、闪存类型和分区布局。

## 下载

[最新 Release](https://github.com/osGex0o0II/ResQBoot/releases/latest) · [设备与构建配置](docs/devices.md)

按设备型号选择对应固件，并使用 Release 中的 `SHA256SUMS` 核对 SHA-256 校验值。

## Web 恢复

在 U-Boot 控制台执行 `httpd`，默认访问 `http://192.168.1.1/`。部分设备支持通过复位按键进入恢复模式。

支持 DHCP 的构建可自动分配电脑 IP；若分配失败，可手动设置为 `192.168.1.2/24`。

详细步骤见 [Web 恢复指南](docs/recovery.md)。

## 源码构建

依赖、工具链与构建命令见 [构建指南](docs/build.md)。

## 使用须知

刷写 U-Boot、ART、CDT、MIBIB、GPT 等关键分区前，请备份原始数据，并核对设备型号、闪存类型和分区布局。操作不当可能导致设备无法启动或校准数据丢失。

并非所有构建配置均经过本版本实机验证。固件按现状提供，不保证适用于特定设备或无故障运行。

## 许可证与来源

项目以 [GPLv2](LICENSE) 为主要许可证，部分文件另有许可声明；复制、修改和分发须遵守源文件及[许可说明](Licenses/README)中的适用条款。

源自 [1980490718/u-boot-2016](https://github.com/1980490718/u-boot-2016)。`README_ORIG` 为历史 U-Boot 文档，不代表当前支持范围。
