# Web 恢复指南

## 启动与访问

在支持的 U-Boot 控制台执行：

```text
httpd
```

默认使用以下网络参数：

| 参数 | 默认值 |
| --- | --- |
| 设备 IP（`ipaddr`） | `192.168.1.1` |
| 子网掩码（`netmask`） | `255.255.255.0` |
| U-Boot 服务器地址（`serverip`） | `192.168.1.2` |
| Web 地址 | `http://192.168.1.1/` |

`serverip` 是 U-Boot 网络命令使用的服务器地址变量，并非 DHCP 对电脑分配地址的保证。

电脑直连设备的可用 LAN 口，启用 DHCP 获取地址；如果当前构建没有启用 DHCP 或分配失败，手动设置：

```text
IP 地址：192.168.1.2
子网掩码：255.255.255.0
网关：留空
```

然后访问 `http://192.168.1.1/`。请使用 HTTP，而非 HTTPS。

可在控制台使用 `httpd 192.168.10.1` 指定其他设备 IP（仍使用 /24 子网）；`httpd s` 停止恢复服务。部分设备可通过复位按键进入 Web 恢复，具体触发方式取决于设备配置。

## 恢复操作

Web 界面根据设备构建配置提供固件、U-Boot、ART、CDT、MIBIB、GPT 等镜像操作，并支持 FIT 格式 initramfs 的内存启动。各项功能并非对所有设备或闪存类型均可用。

上传前确认镜像对应实际设备、闪存类型和分区布局。刷写关键分区可能导致设备无法启动；应先备份原始分区及 ART 等校准数据。写入期间不要断电。

建议使用较新的 Chrome、Firefox、Safari 或 Edge 浏览器。

## 环境变量

Web 页面目前仅支持设置或清除 `reset_key` 和 `config_name` 两项环境变量；其他变量需要通过 U-Boot 控制台操作。

### Reset GPIO

`reset_key` 用于覆盖复位按键的 GPIO 编号。设置前必须确认实际引脚编号。

```text
setenv reset_key <GPIO_NUM>
saveenv
```

### 设备配置

`config_name` 用于覆盖设备配置名；错误配置可能影响启动，不应将其视为跨机型兼容保证。

```text
setenv config_name config@xxx
saveenv
```

### 清除覆盖

在控制台执行以下命令，并重启设备：

```text
setenv reset_key
setenv config_name
saveenv
```

Web 页面也可以将相应变量清空。清除后将重新使用设备树中的默认配置（如存在）。

## 恢复网络变量

`httpd` 无参数启动时会在当前运行环境设置默认恢复地址，但不会自动保存这些变量。查看当前值：

```text
printenv ipaddr serverip netmask gatewayip
```

如需恢复并保存编译时的默认网络设置：

```text
env default -f ipaddr serverip netmask
setenv gatewayip
saveenv
```

## 故障排查

- 确认网线连接的是支持恢复服务的接口，并检查设备与电脑链路
- 确认电脑与设备处于同一子网；必要时暂时禁用其他网卡以排除路由冲突
- 确认使用 `http://`，并检查控制台中的 HTTP 启动信息
- 页面无法加载时，更换现代浏览器或尝试直连
- 刷写失败时，优先核对固件类型、设备型号和分区布局
