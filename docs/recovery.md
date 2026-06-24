# Web 恢复指南

本文说明默认网络访问方式和 Web 恢复流程。

## 默认网络

IPQ 构建默认使用以下 U-Boot 网络参数：

| 项目 | 值 |
| --- | --- |
| 设备 IP | `192.168.1.1` |
| 子网掩码 | `255.255.255.0` |
| 服务器 IP / 电脑侧地址 | `192.168.1.2` |
| Web 恢复地址 | `http://192.168.1.1/` |

电脑直连设备 LAN 口后，内置 DHCP 服务通常会自动分配地址。

如果 DHCP 没有生效，请在电脑上手动设置静态地址：

```text
IP 地址：  192.168.1.2
子网掩码： 255.255.255.0
网关：     可留空，也可填写 192.168.1.1
```

然后打开：

```text
http://192.168.1.1/
```

## 浏览器兼容性

为了减小 U-Boot 镜像体积并降低设备资源占用，恢复页面使用 gzip/zopfli 压缩。

建议使用现代浏览器：

- Chrome 60+
- Firefox 55+
- Safari 11+
- Edge 15+

不支持 Internet Explorer。

## 恢复功能

Web 恢复页面可用于：

- U-Boot 更新；
- 固件更新；
- CDT 更新；
- MIBIB 更新；
- GPT 更新；
- ART 更新；
- initramfs 启动，用于调试和救援。

请只上传与实际设备和分区布局匹配的镜像。

## 环境变量覆盖

### Reset 按键

使用 `reset_key=<GPIO_NUM>` 覆盖 reset 按键 GPIO。

TTL 控制台：

```text
setenv reset_key x
saveenv
```

Web 环境变量页面：

- 变量名：`reset_key`
- 变量值：`x`

请将 `x` 替换为已知 GPIO 编号。

### 设备配置名

使用 `config_name=<config@xxx>` 覆盖设备配置名，方便跨机型或跨固件测试。

TTL 控制台：

```text
setenv config_name config@xxx
saveenv
```

Web 环境变量页面：

- 变量名：`config_name`
- 变量值：`config@xxx`

### 清除覆盖

在 TTL 控制台清除覆盖变量：

```text
setenv reset_key
saveenv
```

```text
setenv config_name
saveenv
```

清除后重启设备，U-Boot 会重新使用 DTB 中的默认条目。

## 排查建议

- 确认电脑位于 `192.168.1.0/24` 网段。
- 如果浏览器访问到了错误路由，临时禁用其它网络适配器。
- 优先尝试电脑和设备直连，不要经过交换机。
- 使用 `http://192.168.1.1/`，不要使用 `https://`。
- 页面无法显示时，换一个现代浏览器测试。
- 确认刷入的资产文件与实际设备型号匹配。
