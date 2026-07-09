# BUS/UPS Controller Firmware Open Source Notes

## 定位

`bus_prj` 是电池/UPS/直流母线控制固件，目标平台为 STM32G474RET6 + RT-Thread。它用于验证外部输入、电池电压、受控负载输出、状态统计和安全态控制。

## 当前核心目录

| 路径 | 作用 |
| --- | --- |
| `applications/ups/` | UPS/直流母线业务模块，包含配置、IO、INA226、服务层和 MSH 命令 |
| `applications/main.c` | RT-Thread 应用入口 |
| `board/CubeMX_Config/` | CubeMX 引脚和外设参考配置 |
| `UPS_PINMAP.md` | 当前 UPS 扩展引脚表 |
| `README_MIGRATION.md` | 从原 ESP32/UPS 逻辑迁移到 RT-Thread 的说明 |
| `VSCODE_WORKFLOW.md` | VS Code/命令行开发流程说明 |

## 已实现能力

- RT-Thread 线程化 UPS 服务循环。
- 输入电压、电池电压和扩展 ADC 采样接口。
- INA226 电压/电流/功率统计接口。
- 受控输出 GPIO 管理和安全态关闭。
- CSS、UVCH1、UVCH2、UVCH3 扩展输出。
- DAC CC/CV 模拟参考输出接口。
- EC11 编码器输入资源预留。
- RT-Thread MSH 命令用于上板调试。

## 暂不声明为已完成

- 高功率电池充放电闭环控制。
- 完整 CAN/FDCAN 协议栈和上位主控联动。
- SOC 估算、电池内阻估算、复杂 BMS 算法。
- 高压/大电流场景的长期可靠性验证。

## 发布建议

- 公开仓库中保留 `.config`、`rtconfig.h`、`SConstruct`、`SConscript`，这些文件对 RT-Thread 构建可复现有用。
- 不发布 `build/`、`*.elf`、`*.bin`、`*.map`、`.sconsign.dblite`、`*.pyc` 和 `*.bak_before_*`。
- `applications/pcb02/` 是历史/共享 MPPT 模块副本，纯 BUS/UPS 发布时应在 README 中说明它不是该工程主线；如不需要，可在发布分支中单独移除。
- 上电测试前必须接入限流电源、保险丝、BMS 和必要硬件保护。
