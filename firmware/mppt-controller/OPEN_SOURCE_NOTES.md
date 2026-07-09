# PCB-02 MPPT Controller Firmware Open Source Notes

## 定位

本工程是 PCB-02 光伏 MPPT 可调充电功率板的 STM32G474RET6 + RT-Thread 固件。它将原 ESP32 MPPT 项目的光伏采样、输出采样、MPPT、CC-CV 和保护逻辑拆成 RT-Thread 模块，后续可通过 CAN 接收 PCB-01/RK3506 的电池类型、限流和启停命令。

## 当前核心目录

| 路径 | 作用 |
| --- | --- |
| `applications/pcb02/` | PCB-02 业务模块 |
| `applications/pcb02/pcb02_service.*` | 对外服务层，MSH/CAN 都应调用这一层 |
| `applications/pcb02/pcb02_control.*` | MPPT、CC-CV、保护逻辑 |
| `applications/pcb02/pcb02_sensors.*` | 传感数据聚合 |
| `applications/pcb02/pcb02_ina226.*` | INA226 输入/输出电压电流采样 |
| `applications/pcb02/pcb02_adc.*` | ADC/NTC 采样 |
| `applications/pcb02/pcb02_hw.*` | GPIO、PWM、安全态硬件抽象 |
| `applications/pcb02/pcb02_shell.c` | RT-Thread MSH 调试命令 |
| `board/CubeMX_Config/` | STM32G474RET6 引脚和外设参考配置 |

## 已实现能力

- RT-Thread 线程化 PCB02 控制循环。
- 双 INA226 采样驱动框架。
- NTC/ADC 采样驱动框架。
- MPPT/CC-CV 控制逻辑模块化。
- 软件使能、硬线 CHG_EN、故障锁存、安全态关闭。
- 风扇、反灌 MOS、Buck 使能、调试 PWM 等硬件抽象。
- MSH 命令用于无 CAN 状态下调试服务层。

## 暂不声明为已完成

- 高功率长时间充电可靠性验证。
- CAN/FDCAN 完整通信协议和实测。
- 参数掉电保存。
- 不同电池类型的自动识别。
- 量产级校准、温漂补偿和保护整定。

## 推荐对外接口分层

```text
MSH/CAN command
    -> pcb02_service
        -> pcb02_control
        -> pcb02_sensors
        -> pcb02_hw
```

MSH 和 CAN 不应直接操作 PWM、ADC 或 GPIO。这样后续实现 CAN 驱动时，只需要把报文映射到 `pcb02_service_*` 接口，实时控制线程不用重写。

## 安全说明

本固件用于光伏充电功率板开发验证。接入光伏板、电池、BMS 或电子负载前，必须确认保险丝、限流电源、过压/过流/过温硬件保护、BMS 和功率器件耐压余量。软件保护不能替代硬件保护。
