# 构建指南

本文说明如何在本地构建 ResQBoot。

## 系统要求

推荐使用 Ubuntu 20.04 LTS 或更新版本。

安装构建依赖：

```bash
sudo apt-get update
sudo apt-get install -y build-essential libncurses5-dev gawk git gettext libssl-dev python3 wget cpio flex bison bc rsync nodejs npm gzip zopfli device-tree-compiler
```

安装 `makefsdatac` 处理 Web 页面时需要的压缩工具：

```bash
sudo npm install -g html-minifier-terser clean-css terser
```

## 源码和工具链

将本仓库和工具链克隆到同一级目录：

```bash
git clone https://github.com/osGex0o0II/ResQBoot.git
git clone https://github.com/1980490718/toolchain-arm_cortex-a7_gcc-5.2.0.git staging_dir
cd ResQBoot
```

构建脚本默认使用以下环境：

```text
STAGING_DIR=../staging_dir/
CROSS_COMPILE=arm-openwrt-linux-
ARCH=arm
```

这些值会由构建脚本自动设置。

## 构建命令

构建某个平台的全部设备：

```bash
./build.sh ipq6018
```

构建单个设备配置：

```bash
./build.sh ipq6018_jdcloud_ax6600
```

深度清理生成文件：

```bash
./build.sh clean
```

仅清理输出文件：

```bash
./build.sh clean_all
```

## 输出文件

- `IPQ40xx` 和 `IPQ806x`：`bin/*.elf`
- 其它 IPQ 平台：`bin/*.mbn`

CI 会将 Release 资产重命名为 [devices.md](devices.md) 中展示的短英文文件名，方便搜索和下载。

## 常见错误

如果出现类似错误：

```text
httpd/fs.c:54:20: fatal error: fsdata.c: No such file or directory
```

请检查是否已经安装 Node.js 以及以下 npm 包：

```bash
html-minifier-terser clean-css terser
```

## CI 构建

每次推送到 `main` 时，GitHub Actions 会构建全部 IPQ 平台，并创建当天的 Release 标签，例如 `v2026-06-24`。

Release 任务会：

- 下载各平台构建产物；
- 根据 `docs/devices.csv` 准备短英文资产名；
- 将设备资产索引写入 Release 说明；
- 上传新资产前删除同一天已有的 Release 标签。
