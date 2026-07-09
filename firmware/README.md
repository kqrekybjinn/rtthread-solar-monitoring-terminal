# Firmware Projects

| 路径 | 说明 |
| --- | --- |
| `mppt-controller/` | PCB-02 光伏 MPPT 功率控制节点固件 |
| `bus-ups-controller/` | 电池/UPS/直流母线管理固件 |

两个工程均为自包含 RT-Thread 工程，保留了 RT-Thread、STM32 HAL/CMSIS、CubeMX 配置和工程文件，便于直接导入 RT-Thread Studio 或使用 SCons 构建。
