# 构建指南

## 环境

CI 使用 Ubuntu 26.04、Node.js 24 和 Python 3，具体配置见 [构建工作流](../.github/workflows/build-ipq-uboot.yml)。其他 Linux 环境未全面验证；设备元数据检查需要 Python 3.9 或更新版本。

安装构建依赖：

```bash
sudo apt-get update
sudo apt-get install -y build-essential libncurses5-dev gawk git gettext libssl-dev python3 wget cpio flex bison bc rsync zopfli device-tree-compiler
```

安装 Node.js 24（含 npm）后，安装 Web 资源压缩工具：

```bash
npm install -g html-minifier-terser clean-css terser svgo
```

## 源码与工具链

将源码仓库与交叉编译工具链放在同一父目录：

```bash
git clone https://github.com/osGex0o0II/ResQBoot.git
git clone https://github.com/1980490718/toolchain-arm_cortex-a7_gcc-5.2.0.git staging_dir
cd ResQBoot
```

`build.sh` 自动配置 `ARCH=arm`、`CROSS_COMPILE=arm-openwrt-linux-` 和 `STAGING_DIR=../staging_dir/`。

## 构建与清理

构建 IPQ6018 平台全部配置：

```bash
./build.sh ipq6018
```

构建指定配置：

```bash
./build.sh ipq6018_jdcloud_ax6600
```

清理构建中间文件：`./build.sh clean`；仅清理输出文件：`./build.sh clean_all`。执行前请备份需要保留的构建产物。

编译产物位于 `bin/`。IPQ40xx、IPQ806x 输出 `.elf`，其他 IPQ 平台输出 `.mbn`；发布资产按 [设备索引](devices.md) 统一命名。

## 常见问题

若出现 `failsafe/fsdata.c: No such file or directory`，请检查 Node.js、npm 及上述四个压缩工具是否可用。

## 自动构建

GitHub Actions 支持推送、Pull Request 和手动触发。仅 `main` 分支推送或手动选择全部平台，且各平台构建成功时，才会发布按日期命名的 Release。

Release 包含构建产物及 `SHA256SUMS`。CI 构建成功不代表已通过设备启动、刷写或恢复测试。
