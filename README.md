<h1 align="center">ResQBoot</h1>

<p align="center">
  <a href="https://github.com/osGex0o0II/ResQBoot/actions/workflows/build-ipq-uboot.yml"><img src="https://github.com/osGex0o0II/ResQBoot/actions/workflows/build-ipq-uboot.yml/badge.svg?branch=main" alt="CI" /></a>
  <img src="https://img.shields.io/badge/QSDK-12.5-555555?style=flat-square" alt="QSDK 12.5" />
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPLv2-555555?style=flat-square" alt="GPLv2" /></a>
</p>

基于 Qualcomm QSDK 12.5 的 IPQ 平台 U-Boot 恢复固件，提供 Web 恢复与固件刷写功能。

## 功能

- U-Boot Web 恢复界面；支持 DHCP 的构建可为直连电脑分配地址
- 固件、U-Boot、ART、CDT、MIBIB、GPT 等镜像的刷写与恢复
- 通过 Web 页面设置 `reset_key`、`config_name` 环境变量
- FIT 格式 initramfs 的内存启动

实际功能受设备配置、闪存类型及分区布局限制。

## 下载

[最新 Release](https://github.com/osGex0o0II/ResQBoot/releases/latest) · [设备与构建配置](docs/devices.md)

根据实际设备选择对应资产，并使用 Release 附带的 `SHA256SUMS` 校验文件完整性。构建成功不代表已通过实机验证。

## Web 恢复

在支持的 U-Boot 控制台执行 `httpd`，默认访问地址为 `http://192.168.1.1/`。部分已适配设备也可通过复位按键进入。

电脑可尝试自动获取 IP；如 DHCP 未生效，可手动设置为 `192.168.1.2/24`。

操作方法与环境变量说明见 [Web 恢复指南](docs/recovery.md)。

## 源码构建

工具链、依赖和编译命令见 [构建指南](docs/build.md)。

## 使用须知

刷写 U-Boot、ART、CDT、MIBIB、GPT 等关键分区可能导致设备无法启动或丢失校准数据。操作前应备份原始分区，并核对设备型号、闪存类型及分区布局。

项目未对全部构建配置完成当前版本的实机验证。固件按现状提供，不附带适用性或无故障保证。

## 许可证与来源

本项目使用 [GPLv2](LICENSE) 许可证，复制、修改和分发须遵守相关条款，并保留适用的版权及许可证声明。

代码基于 QSDK U-Boot 及社区版本修改；相关源码可参阅 [1980490718/u-boot-2016](https://github.com/1980490718/u-boot-2016)。
