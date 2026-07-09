# Hardware Overview

## 功能模块

从当前原理图可识别出以下主要模块：

| 模块 | 主要器件/信号 | 功能 |
| --- | --- | --- |
| 主控 | STM32G474RET6 + RT-Thread | MPPT/CC-CV 控制、采样管理、GPIO 控制、CAN 通信、故障处理 |
| 光伏输入 | PV、VIN+、VBUS1、保险丝/TVS/采样电阻 | 接入光伏板或外部 DC 输入 |
| 电池/输出侧 | BAT、VBAT、VBUS2、保险丝/采样电阻 | 接入电池、BMS 或后级直流母线 |
| 功率变换 | MOSFET、EG2104S、L3 60 uH、续流/保护器件 | DC-DC 同步 Buck/Boost 功率级 |
| 电压电流采样 | INA226 x2 | 分别采集输入侧和输出侧电压/电流/功率 |
| 辅助电源 | MP9486AGN-Z、ST13470、B1212S | 板载 12 V/3.3 V/隔离辅助电源 |
| 通信 | TCAN3413DR、CAN_H/CAN_L | CAN 通信接口 |
| 调试/下载 | SWD、UART TX/RX、KEY | 程序下载、串口调试、按键输入 |
| 温度/散热 | NTC、FAN | 温度采样和风扇控制 |
| 状态/控制 | CHG_EN、FAULT_N、LED | 充电使能、故障状态、运行状态显示 |

## 当前硬件定位

该板是整机能源侧的一个 RT-Thread 实时功率控制节点，不应被描述为完整电源系统。它更准确的定位是：

```text
光伏输入/外部 DC 输入 -> MPPT 可调充电 DC-DC 功率板 -> 电池/BMS/直流母线
```

它需要与电池保护板、UPS/直流母线管理板或上级 Linux + RT-Thread AMP 主控配合使用。

## 与固件的关系

推荐配套固件工程：

```text
../../mppt_prj/stm32g474-rtthread-mppt/
```

RT-Thread 固件应负责：

- 读取双 INA226 采样值。
- 读取 NTC/ADC 温度和辅助采样。
- 根据电池类型设置目标充电电压和限流值。
- 执行 MPPT 或恒压/恒流控制。
- 处理 `CHG_EN`、`FAULT_N`、风扇、反灌控制和 PWM 安全态。
- 通过 CAN 接收上级控制命令并上报状态。

## 当前资料限制

- 当前只有 PDF 原理图和 Gerber/Drill 生产文件。
- 未包含可编辑 EDA 工程源文件。
- 未包含 BOM、贴片坐标、装配图和完整测试报告。
- 未确认最终功率等级、散热条件和长期可靠性。
