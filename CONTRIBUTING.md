# 贡献指南

## 分支与提交

- 从 `main` 创建分支，使用 `fix/...`、`feat/...`、`docs/...` 或 `ci/...` 前缀。
- 一个 Pull Request 聚焦一个问题，避免混入无关格式化或重构。
- 提交信息使用英文，首行简洁描述变更，正文说明动机、影响和验证方式。
- 不要提交生成文件，例如 `failsafe/fsdata.c`、`bin/` 产物或本地工具链。
- 不要强制推送已被他人使用的公共分支。

## 开发环境

构建依赖、交叉工具链和平台命令见 [构建指南](docs/build.md)。常用检查命令：

```bash
python scripts/check_devices.py
python scripts/test_recovery_safety.py
python scripts/test_clean_safety.py
./build.sh ipq6018
```

`test_recovery_safety.py` 需要 C 编译器（CI 使用 `cc`）和 Python 3.9 或更新版本；`test_clean_safety.py` 需要 Bash。Windows 本地缺少这些工具时，以 CI 结果为准。

## 代码风格

- C 代码遵循 U-Boot 既有风格：制表符缩进、短行、局部变量声明尽量靠前。
- 修改 Web 资源时保持 `failsafe/html/pig/` 的现有结构；构建阶段会压缩 HTML、CSS、JS 和 SVG。
- 不要手工编辑生成的 `failsafe/fsdata.c`。
- 修改设备配置时同步更新 `configs/`、`docs/devices.csv`、`docs/devices.md` 和 DTS。
- 新增设备必须通过 `scripts/check_devices.py`，并说明实机测试状态。

## 文档

- 文档使用中文，标点使用全角符号，避免表情符号。
- 新增文档后更新 [文档索引](docs/README.md)。
- 相对链接必须指向仓库内真实文件；外部链接优先使用项目官方地址。
- 命令、路径和变量名使用行内代码格式。
- 涉及刷写、分区或安全边界的内容必须明确风险，不写未经验证的操作步骤。

## 测试与验证

提交前至少完成：

- 运行与变更相关的脚本检查。
- 修改 C 代码时完成对应平台编译。
- 修改 Web 页面时在真实浏览器中检查明暗主题、窄屏布局和刷写表单。
- 修改发布流程时在分支上手动触发目标平台的 CI。
- 在 PR 描述中注明测试设备、固件版本和结果；没有实机验证时明确说明。

CI 构建成功不代表设备启动、刷写或恢复流程已验证。

## 安全问题

不要通过公开 issue 报告未修复的安全漏洞。请按照 [安全策略](SECURITY.md) 使用 GitHub Security Advisories 私下报告。

## Pull Request 检查

- [ ] 变更范围与 PR 描述一致
- [ ] 相关测试或构建已通过
- [ ] 新增或修改的文档链接有效
- [ ] 没有提交生成文件、密钥或设备敏感信息
- [ ] 涉及刷写或安全边界时已说明风险和验证条件
