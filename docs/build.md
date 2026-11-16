# 构建指南

## 环境

CI 当前使用 Ubuntu 26.04、Node.js 24 和 Python 3。其他 Linux 环境尚未全面验证；校验脚本要求 Python 3.9 或更新版本。

安装构建依赖：

```bash
sudo apt-get update
sudo apt-get install -y build-essential libncurses5-dev gawk git gettext libssl-dev python3 wget cpio flex bison bc rsync zopfli device-tree-compiler
```

另行安装 Node.js 24（含 npm），然后安装 Web 资源压缩工具：

```bash
npm install -g html-minifier-terser clean-css terser svgo
```

## 源码与工具链

将仓库和交叉编译工具链放在同一级目录：

```bash
git clone https://github.com/osGex0o0II/ResQBoot.git
git clone https://github.com/1980490718/toolchain-arm_cortex-a7_gcc-5.2.0.git staging_dir
cd ResQBoot
```

`build.sh` 自动设置 `ARCH=arm`、`CROSS_COMPILE=arm-openwrt-linux-` 和 `STAGING_DIR=../staging_dir/`。

## 构建

构建平台内全部配置：

```bash
./build.sh ipq6018
```

构建指定配置：

```bash
./build.sh ipq6018_jdcloud_ax6600
```

清理构建中间文件使用 `./build.sh clean`；清理输出文件使用 `./build.sh clean_all`。清理命令会删除生成文件，请勿在含有未备份产物的工作目录中执行。

输出文件位于 `bin/`：IPQ40xx、IPQ806x 使用 `.elf`，其余 IPQ 平台使用 `.mbn`。正式发布时，CI 按 [设备索引](devices.md) 中的命名规则整理资产。

## 常见问题

若出现 `failsafe/fsdata.c: No such file or directory`，检查 Node.js、npm 和上述四个压缩工具是否安装且可执行。

## 自动构建

GitHub Actions 在推送、Pull Request 或手动触发时执行构建。只有 `main` 分支的推送，或手动选择全部平台构建成功后，才会发布按日期命名的 Release。

Release 包含编译产物与 `SHA256SUMS`；CI 验证不能替代实际设备的启动、刷写和恢复测试。
