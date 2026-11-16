# Web 恢复指南

## 启动与访问

在支持 Web 恢复的 U-Boot 控制台执行：

```text
httpd
```

默认网络参数如下：

| 参数 | 默认值 |
| --- | --- |
| 设备 IP（`ipaddr`） | `192.168.1.1` |
| 子网掩码（`netmask`） | `255.255.255.0` |
| U-Boot 服务器地址（`serverip`） | `192.168.1.2` |
| Web 地址 | `http://192.168.1.1/` |

`serverip` 用于 U-Boot 网络命令，不代表 DHCP 必定向电脑分配该地址。

电脑直连设备可用的 LAN 口。若构建启用了 DHCP 服务，可自动获取 IP；否则手动设置：

```text
IP 地址：192.168.1.2
子网掩码：255.255.255.0
网关：留空
```

浏览器访问 `http://192.168.1.1/`（不支持 HTTPS）。

其他控制台命令：

- `httpd 192.168.10.1`：以指定设备 IP 启动 Web 恢复服务，子网掩码仍为 `/24`
- `httpd s`：停止 Web 恢复服务

部分设备可通过复位按键进入恢复模式，触发方式取决于设备配置。

## 恢复功能

Web 页面提供固件、U-Boot、ART、CDT、MIBIB、GPT 等镜像操作，以及 FIT 镜像的内存启动。内存启动会校验镜像格式，文件扩展名不能保证兼容。可用功能取决于设备配置、闪存类型和分区布局。

上传前应核对设备型号与镜像类型，并备份原始分区及校准数据。写入关键分区期间切勿断电。

## 环境变量

设备信息页的快捷设置仅支持 `reset_key`、`config_name`，且会检查环境保存结果。Web 命令终端默认关闭，需从本地串口执行 `httpd t` 为当前会话开启；开放后可执行任意 U-Boot 命令，须在隔离网络下使用。

### 复位按键 GPIO

`reset_key` 用于覆盖复位按键 GPIO。确认实际编号后，将命令中的 `N` 替换为对应数字：

```text
setenv reset_key N
saveenv
```

### 设备配置

`config_name` 用于覆盖设备配置名。将 `config@xxx` 替换为实际配置；错误配置可能导致启动失败。

```text
setenv config_name config@xxx
saveenv
```

### 清除覆盖

可在 Web 页面清空对应变量，或在控制台执行：

```text
setenv reset_key
setenv config_name
saveenv
```

重启后将按设备树及构建默认配置生效。

## 恢复默认网络参数

执行 `httpd` 时采用独立的临时恢复网络地址，不修改 `ipaddr`、`serverip`、`netmask`、`gatewayip` 环境变量；因此后续保存其他设置也不会顺带保存恢复网络地址。可查看原有环境值：

```text
printenv ipaddr serverip netmask gatewayip
```

恢复并保存编译时的默认值：

```text
env default -f ipaddr serverip netmask
setenv gatewayip
saveenv
```

## 故障排查

- 检查网线和设备 LAN 口的链路状态
- 确认电脑与设备位于同一子网，必要时暂时禁用其他网卡
- 确认访问的是 HTTP 地址，并检查控制台启动信息
- 页面显示异常时，尝试其他现代浏览器或电脑直连
- 刷写失败时，核对镜像类型、设备型号和分区布局
