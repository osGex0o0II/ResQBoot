# 品牌规范

本文档定义 ResQBoot 的标志、颜色、字体和应用方式，确保仓库、Release 页面和 Web 恢复界面保持一致。

## 标志

ResQBoot 的标志是一枚终端块光标。字标末尾的蓝色横条是它的水平形态，独立图标是它的垂直形态；两者方向不同，形态同源。

<p>
  <img src="images/resqboot-logo.svg" width="160" height="160" alt="ResQBoot 主标志" />
  <img src="images/resqboot-logo-dark.svg" width="160" height="160" alt="ResQBoot 深色标志" />
</p>

<p>
  <img src="images/resqboot-wordmark.svg" width="380" alt="ResQBoot 字标" />
</p>

设计只保留一个彩色元素：蓝色光标。没有盾牌、字母组合、渐变、阴影或立体效果。光标对应 U-Boot 控制台与恢复终端，强调“可操作的恢复环境”，而不是安全产品叙事。

### 资产

| 文件 | 用途 |
| --- | --- |
| [`resqboot-wordmark.svg`](images/resqboot-wordmark.svg) | 浅色背景字标，用于 README、文档页和横幅 |
| [`resqboot-wordmark-dark.svg`](images/resqboot-wordmark-dark.svg) | 深色背景字标 |
| [`resqboot-logo.svg`](images/resqboot-logo.svg) | 浅色背景徽章，README 与头像使用 |
| [`resqboot-logo-dark.svg`](images/resqboot-logo-dark.svg) | 深色背景徽章 |
| [`resqboot-mark.svg`](images/resqboot-mark.svg) | 透明背景光标，用于 favicon、页头和小尺寸场景 |
| [`resqboot-mark-dark.svg`](images/resqboot-mark-dark.svg) | 深色背景透明光标 |

### 清晰空间与最小尺寸

- 徽章四周保留不少于徽章边长 1/4 的空白；字标四周保留不少于字标中“R”高度的空白。
- 数字界面中，徽章最小显示尺寸为 24 × 24 px；独立光标最小为 16 × 16 px；字标最小宽度为 120 px。
- 小于 24 px 时只使用 `resqboot-mark.svg`，不要使用带徽章底板的版本。
- 缩放时保持原始宽高比，不要单独拉伸宽或高。

### 使用限制

- 不要改变光标的方向、粗细或相对文字的位置；字标中的光标保持在文字基线右侧。
- 不要替换标志颜色，不要添加盾牌、字母、图标、阴影、渐变、描边或发光效果。
- 不要在低对比度、复杂照片或高饱和度背景上直接放置标志。
- 不要旋转、倾斜、裁切或重新排列字标与光标。
- 不要使用已移除的盾牌或横向锁定组合资产替代当前标志。

## 颜色

品牌以墨色和蓝色光标为主，终端绿色仅用于状态提示。

| 名称 | 色值 | 用途 |
| --- | --- | --- |
| 墨色 | `#101820` | 字标、浅色背景标题 |
| 光标蓝 | `#2F6FED` | 浅色背景光标、主要操作 |
| 浅光标蓝 | `#5DADE2` | 深色背景光标、链接 |
| 浅色表面 | `#F7F7F5` | 徽章底色 |
| 浅色边框 | `#E4E4E7` | 徽章描边、分隔线 |
| 深色表面 | `#121722` | 深色页面背景、深色徽章 |
| 深色边框 | `#232A38` | 深色分隔线 |
| 深色文字 | `#F4F6F8` | 深色背景字标与正文 |
| 终端绿 | `#4ECB71` | 成功、终端提示 |
| 警告红 | `#E74C3C` | 危险操作、错误状态 |

Web 恢复界面目前使用自己的主题色（`#5dade2` / `#3498db`），完整定义在 [`failsafe/html/pig/style.css`](../failsafe/html/pig/style.css)。它属于运行时主题，不是标志资产；后续界面改版时再统一到光标蓝。

## 字体

- 字标使用 IBM Plex Sans Medium，字距收紧；仓库中的 SVG 已转为轮廓，不依赖本机字体。
- 界面与文档使用 `Arial, sans-serif`（现有 Web UI）或系统无衬线字体。
- 中文正文使用系统默认无衬线字体，不额外引入字体文件。
- 标题与正文至少保持 1.5 倍字号差，避免在同一层级混用多种字重。

## 界面应用

- Web 恢复页面沿用深海军蓝背景、蓝色主操作和绿色终端提示。
- 当前 Web UI 尚未嵌入标志；后续加入时，页头使用 `resqboot-mark.svg`，高度不超过 24 px，不添加文字。
- favicon 使用 `resqboot-mark.svg` 或徽章，不包含文字和标语。
- 明暗主题切换时使用对应版本：浅色背景用 `resqboot-logo.svg`，深色背景用 `resqboot-logo-dark.svg`。
- 状态色只表达状态，不用于装饰性大面积背景。

## 截图

文档和 Release 页面中的界面截图必须来自真实设备或真实浏览器，不得使用合成界面、占位图或修改过的状态。

截图应满足：

- 使用当前版本界面，避免展示已删除的功能。
- 隐藏或替换 MAC 地址、序列号、密码等敏感信息。
- 保留足够的界面上下文，不要只截取单个按钮。
- 使用 PNG 格式，避免多次有损压缩。
- 在截图下方注明设备型号、固件版本和测试场景。

## 标志预览

在浏览器中打开 [`images/resqboot-wordmark.svg`](images/resqboot-wordmark.svg)、[`images/resqboot-wordmark-dark.svg`](images/resqboot-wordmark-dark.svg)、[`images/resqboot-logo.svg`](images/resqboot-logo.svg)、[`images/resqboot-logo-dark.svg`](images/resqboot-logo-dark.svg)、[`images/resqboot-mark.svg`](images/resqboot-mark.svg) 或 [`images/resqboot-mark-dark.svg`](images/resqboot-mark-dark.svg) 可查看矢量原图。
