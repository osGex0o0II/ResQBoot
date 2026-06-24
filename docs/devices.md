# 支持设备

本页列出每个构建配置对应的实际设备型号和 Release 资产名。

Release 资产使用以下命名格式：

```text
<Platform>-<Device>.ext
```

`IPQ40xx` 和 `IPQ806x` 使用 `.elf` 文件，其它 IPQ 平台使用 `.mbn` 文件。

测试状态说明：

- `✓`：已测试
- `×`：未测试
- `?`：未知

| 平台 | 配置 | 实际设备型号 | Release 资产 | Mach ID | 测试状态 | 编译命令 |
| --- | --- | --- | --- | --- | --- | --- |
| IPQ40xx | `ipq40xx_aliyun_ap4220` | Aliyun AP4220 | `IPQ40xx-Aliyun-AP4220.elf` | 0x9000010 | ✓ | `./build.sh ipq40xx_aliyun_ap4220` |
| IPQ40xx | `ipq40xx_standard` | QCA Standard Common | `IPQ40xx-QCA-Standard.elf` | --- | ✓ | `./build.sh ipq40xx_standard` |
| IPQ40xx | `ipq40xx_p2w_r619ac` | P&W R619AC | `IPQ40xx-PW-R619AC.elf` | 0x8010006 | ✓ | `./build.sh ipq40xx_p2w_r619ac` |
| IPQ40xx | `ipq40xx_thinkplus_fogpod800` | ThinkPlus FogPod800 | `IPQ40xx-ThinkPlus-FogPod800.elf` | 0x8010100 | ✓ | `./build.sh ipq40xx_thinkplus_fogpod800` |
| IPQ40xx | `ipq40xx` | QCA Common | `IPQ40xx-QCA-Common.elf` | --- | ✓ | `./build.sh ipq40xx` |
| IPQ5018 | `ipq5018_ctcc_ap301_l` | CTCC AP301-L | `IPQ5018-CTCC-AP301-L.mbn` | 0x8040002 | ✓ | `./build.sh ipq5018_ctcc_ap301_l` |
| IPQ5018 | `ipq5018_cmcc_rax3000q` | CMCC RAX3000Q | `IPQ5018-CMCC-RAX3000Q.mbn` | 0x8040000 | × | `./build.sh ipq5018_cmcc_rax3000q` |
| IPQ5018 | `ipq5018_cucc_vs010` | CUCC VS010 | `IPQ5018-CUCC-VS010.mbn` | 0x8040000 | ✓ | `./build.sh ipq5018_cucc_vs010` |
| IPQ5018 | `ipq5018_gl_b3000` | GL.iNet GL-B3000 | `IPQ5018-GLiNet-GL-B3000.mbn` | 0x8040004 | × | `./build.sh ipq5018_gl_b3000` |
| IPQ5018 | `ipq5018_jdcloud_ax3000` | JDCloud AX3000 (RE-OS-03U) | `IPQ5018-JDCloud-AX3000.mbn` | 0x8040104 | ✓ | `./build.sh ipq5018_jdcloud_ax3000` |
| IPQ5018 | `ipq5018_mr3000d_04` | CMCC MR3000D-04 | `IPQ5018-CMCC-MR3000D-04.mbn` | 0x8040702 | ✓ | `./build.sh ipq5018_mr3000d_04` |
| IPQ5018 | `ipq5018_mr3000d_ci` | CMCC MR3000D-CI | `IPQ5018-CMCC-MR3000D-CI.mbn` | 0x8040802 | ✓ | `./build.sh ipq5018_mr3000d_ci` |
| IPQ5018 | `ipq5018_pz_l8` | CMCC PZ-L8 | `IPQ5018-CMCC-PZ-L8.mbn` | 0x8040000 | × | `./build.sh ipq5018_pz_l8` |
| IPQ5018 | `ipq5018_ruijie_ma3063` | Ruijie RG-MA3063 | `IPQ5018-Ruijie-RG-MA3063.mbn` | 0x8040000<br>0x8040004 | × | `./build.sh ipq5018_ruijie_ma3063` |
| IPQ5018 | `ipq5018_skspruce_ap8330c` | SKSPRUCE AP8330C | `IPQ5018-SKSPRUCE-AP8330C.mbn` | 0x8040202 | ✓ | `./build.sh ipq5018_skspruce_ap8330c` |
| IPQ5018 | `ipq5018_tiny` | QCA Tiny Common | `IPQ5018-QCA-Tiny.mbn` | --- | ✓ | `./build.sh ipq5018_tiny` |
| IPQ5018 | `ipq5018_tiny_debug` | QCA Tiny Debug Common | `IPQ5018-QCA-Tiny-Debug.mbn` | --- | ✓ | `./build.sh ipq5018_tiny_debug` |
| IPQ5018 | `ipq5018` | QCA Standard Common | `IPQ5018-QCA-Standard.mbn` | --- | ✓ | `./build.sh ipq5018` |
| IPQ5332 | `ipq5332_h3c_ne36pro` | H3C NE36PRO | `IPQ5332-H3C-NE36PRO.mbn` | 0x8060004 | ✓ | `./build.sh ipq5332_h3c_ne36pro` |
| IPQ5332 | `ipq5332_jdcloud_be6500` | JDCloud BE6500 (JDBox RE-CS-06) | `IPQ5332-JDCloud-BE6500.mbn` | 0x8060000 | ✓ | `./build.sh ipq5332_jdcloud_be6500` |
| IPQ5332 | `ipq5332_xiaomi_be306` | Xiaomi BE306 | `IPQ5332-Xiaomi-BE306.mbn` | 0x8060004 | × | `./build.sh ipq5332_xiaomi_be306` |
| IPQ5332 | `ipq5332_xiaomi_be6500` | Xiaomi BE6500 (RN02) | `IPQ5332-Xiaomi-BE6500.mbn` | 0x8060001 | ✓ | `./build.sh ipq5332_xiaomi_be6500` |
| IPQ5332 | `ipq5332_tiny` | QCA Tiny Common | `IPQ5332-QCA-Tiny.mbn` | --- | ✓ | `./build.sh ipq5332_tiny` |
| IPQ5332 | `ipq5332_tiny_nor` | QCA Tiny NOR Common | `IPQ5332-QCA-Tiny-NOR.mbn` | --- | ✓ | `./build.sh ipq5332_tiny_nor` |
| IPQ5332 | `ipq5332_tiny_debug` | QCA Tiny Debug Common | `IPQ5332-QCA-Tiny-Debug.mbn` | --- | ✓ | `./build.sh ipq5332_tiny_debug` |
| IPQ5332 | `ipq5332_tiny2` | QCA Tiny2 Common | `IPQ5332-QCA-Tiny2.mbn` | --- | ✓ | `./build.sh ipq5332_tiny2` |
| IPQ5332 | `ipq5332` | QCA Standard Common | `IPQ5332-QCA-Standard.mbn` | --- | ✓ | `./build.sh ipq5332` |
| IPQ6018 | `ipq6018_360v6` | Qihoo 360 V6 | `IPQ6018-Qihoo-360-V6.mbn` | 0x8030200 | ✓ | `./build.sh ipq6018_360v6` |
| IPQ6018 | `ipq6018_ax1800pro` | JDCloud AX1800Pro | `IPQ6018-JDCloud-AX1800Pro.mbn` | 0x8030201 | ✓ | `./build.sh ipq6018_ax1800pro` |
| IPQ6018 | `ipq6018_ax5_jdcloud` | JDCloud AX5 | `IPQ6018-JDCloud-AX5.mbn` | 0x8030200 | ✓ | `./build.sh ipq6018_ax5_jdcloud` |
| IPQ6018 | `ipq6018_dptech_ap3000_2c` | DPtech AP3000-2C | `IPQ6018-DPtech-AP3000-2C.mbn` | 0x8030200 | ✓ | `./build.sh ipq6018_dptech_ap3000_2c` |
| IPQ6018 | `ipq6018_gl_axt1800` | GL.iNet GL-AXT1800 / GL-AX1800 | `IPQ6018-GLiNet-GL-AXT1800.mbn` | 0x8030200 | ✓ | `./build.sh ipq6018_gl_axt1800` |
| IPQ6018 | `ipq6018_philips_ly1800` | Philips LY1800 / SY Y6010 | `IPQ6018-Philips-LY1800.mbn` | 0x8030000 | ✓ | `./build.sh ipq6018_philips_ly1800` |
| IPQ6018 | `ipq6018_jdcloud_ax6600` | JDCloud AX6600 | `IPQ6018-JDCloud-AX6600.mbn` | 0x8030202 | ✓ | `./build.sh ipq6018_jdcloud_ax6600` |
| IPQ6018 | `ipq6018_jdcloud_er1` | JDCloud ER1 | `IPQ6018-JDCloud-ER1.mbn` | 0x8030203 | ✓ | `./build.sh ipq6018_jdcloud_er1` |
| IPQ6018 | `ipq6018_m2` | ZN M2 / CMIOT AX18 | `IPQ6018-ZN-M2.mbn` | 0x8030200 | ✓ | `./build.sh ipq6018_m2` |
| IPQ6018 | `ipq6018_nn6000` | Link NN6000 | `IPQ6018-Link-NN6000.mbn` | 0x8030200 | ✓ | `./build.sh ipq6018_nn6000` |
| IPQ6018 | `ipq6018_xiaomi_ax1800` | Xiaomi AX1800 | `IPQ6018-Xiaomi-AX1800.mbn` | 0x8030200 | ✓ | `./build.sh ipq6018_xiaomi_ax1800` |
| IPQ6018 | `ipq6018_tiny` | QCA Tiny Common | `IPQ6018-QCA-Tiny.mbn` | --- | ✓ | `./build.sh ipq6018_tiny` |
| IPQ6018 | `ipq6018` | QCA Common | `IPQ6018-QCA-Common.mbn` | --- | ✓ | `./build.sh ipq6018` |
| IPQ6018 | `ipq6018_zxelink_w212x` | ZXeLink ZXWL W212X | `IPQ6018-ZXeLink-ZXWL-W212X.mbn` | 0x8030200 | ✓ | `./build.sh ipq6018_zxelink_w212x` |
| IPQ806x | `ipq806x_standard` | QCA Standard Common | `IPQ806x-QCA-Standard.elf` | --- | ? | `./build.sh ipq806x_standard` |
| IPQ806x | `ipq806x` | QCA Common | `IPQ806x-QCA-Common.elf` | --- | ? | `./build.sh ipq806x` |
| IPQ807x | `ipq807x_aliyun_ap8220` | Aliyun AP8220 | `IPQ807x-Aliyun-AP8220.mbn` | 0x0801000A | ✓ | `./build.sh ipq807x_aliyun_ap8220` |
| IPQ807x | `ipq807x_redmi_ax6` | Redmi AX6 / Xiaomi AX3600 | `IPQ807x-Redmi-AX6.mbn` | 0x08010010 | ✓ | `./build.sh ipq807x_redmi_ax6` |
| IPQ807x | `ipq807x_tiny` | QCA Tiny Common | `IPQ807x-QCA-Tiny.mbn` | --- | ✓ | `./build.sh ipq807x_tiny` |
| IPQ807x | `ipq807x_xglink_5gcpe` | XGlink 5GCPE | `IPQ807x-XGlink-5GCPE.mbn` | 0x08010008 | ✓ | `./build.sh ipq807x_xglink_5gcpe` |
| IPQ807x | `ipq807x` | QCA Common | `IPQ807x-QCA-Common.mbn` | --- | ✓ | `./build.sh ipq807x` |
| IPQ9574 | `ipq9574` | QCA Common | `IPQ9574-QCA-Common.mbn` | --- | ? | `./build.sh ipq9574` |
