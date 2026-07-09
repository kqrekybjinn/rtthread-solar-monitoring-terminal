# Firmware Architecture

## 整机分层

```text
Linux 智能服务层
    - 数据存储
    - 无线通信
    - MQTT 网关
    - Supabase 后台页面
    - 语音助手
    - HMI / 本地模型推理

RT-Thread AMP 实时协调层
    - 传感器采集
    - 电源状态监测
    - 异常触发
    - 与功率节点通信

RT-Thread 功率节点固件
    - MPPT 控制
    - 电池/直流母线管理
    - 采样、保护、风扇、使能、故障锁存
```

本仓库当前整理的是最底层的 RT-Thread 功率节点固件。

## mppt-controller

路径：

```text
firmware/mppt-controller/applications/pcb02/
```

主要模块：

| 模块 | 作用 |
| --- | --- |
| `pcb02_app.*` | RT-Thread 线程入口 |
| `pcb02_service.*` | 对外服务层，MSH/CAN 都应调用这一层 |
| `pcb02_control.*` | MPPT、CC-CV、保护逻辑 |
| `pcb02_sensors.*` | 传感数据聚合 |
| `pcb02_ina226.*` | INA226 输入/输出电压电流采样 |
| `pcb02_adc.*` | ADC/NTC 采样 |
| `pcb02_hw.*` | GPIO、PWM、安全态硬件抽象 |
| `pcb02_shell.c` | RT-Thread MSH 调试命令 |

推荐接口分层：

```text
MSH/CAN command
    -> pcb02_service
        -> pcb02_control
        -> pcb02_sensors
        -> pcb02_hw
```

## bus-ups-controller

路径：

```text
firmware/bus-ups-controller/applications/ups/
```

主要模块：

| 模块 | 作用 |
| --- | --- |
| `ups_app.*` | RT-Thread 线程入口 |
| `ups_service.*` | UPS/直流母线状态服务 |
| `ups_io.*` | GPIO、DAC、ADC、EC11 等 IO 抽象 |
| `ups_ina226.*` | INA226 电压/电流/功率采集 |
| `ups_config.*` | 引脚和运行参数配置 |
| `ups_shell.c` | RT-Thread MSH 调试命令 |

## 设计原则

- 功率板本地必须具备安全默认态。
- MSH/CAN 不直接操作 PWM、ADC、GPIO，而是通过 service 层下发目标参数。
- 软件保护不能替代硬件保护。
- 上级 Linux/AMP 主控可以下发策略，但功率节点必须能在通信丢失时安全停机。
