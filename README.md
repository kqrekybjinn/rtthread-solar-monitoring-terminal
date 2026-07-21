# RT-Thread AMP 光伏自供能智能监测终端

本仓库为“全国大学生嵌入式芯片与系统设计竞赛 2026”芯片应用赛道睿赛德赛题参赛项目资料仓库，作品名称为“基于 Linux + RT-Thread AMP 混合架构的户外光伏自供能智能监测终端”。仓库内容覆盖能源侧硬件资料、RT-Thread 功率控制固件、RK3506 Linux 端边缘服务代码、构建说明和发布文档。

## 项目简介

在实际科研、工程项目中，野外边坡、基坑、挡墙、塔架等场景的无人值守监测面临基站建设成本高，传感器布设困难、协议异构、拓扑复杂、数据量大、能源供给不稳定等痛点，难以满足临时监测、应急排查或短期科研观测的灵活需求。本项目设计一种基于 Linux + RT-Thread AMP 混合架构的光伏自供能智能监测终端：RT-Thread 作为实时控制层，负责传感器采集、电源状态监测、异常触发与电源控制等硬实时任务；Linux 作为智能服务层，负责数据存储、无线通信、边缘计算、图形交互、网关转发及本地模型推理等非实时任务，双系统通过 rpmsg 实现高速核间通信。能源侧融合 MPPT、高效 DC-DC 变换及逆变器控制技术，构建稳定自供能平台；网络侧支持多节点数据汇聚、协议转换、本地缓存与远程转发，形成完整的边缘网关能力。

相比传统方案，本项目支持快速布设、随用随迁。以 AMP 双系统协同为核心，将光伏电源精细化控制、边缘智能网关与野外便携部署有机统一，在功能性、可用性与便携性之间取得平衡。依托 RT-Thread 工业开发平台，进一步探索本地轻量化模型在异常识别、事件摘要与人机交互中的边缘部署价值，响应大赛以及赛题号召。

## 仓库目录

```text
.
├── README.md
├── docs/
│   └── firmware/
├── firmware/
│   ├── mppt-controller/
│   └── bus-ups-controller/
├── hardware/
│   └── mppt-power-node/
└── software/
    └── rk3506-linux/
```

## 仓库内容

| 路径 | 内容 |
| --- | --- |
| `hardware/mppt-power-node/` | PCB-02 光伏 MPPT 功率控制节点硬件资料，包含原理图 PDF、Gerber/Drill 生产文件和接口说明 |
| `firmware/mppt-controller/` | STM32G474RET6 + RT-Thread 的 MPPT 功率控制固件 |
| `firmware/bus-ups-controller/` | STM32G474RET6 + RT-Thread 的电池/UPS/直流母线管理固件 |
| `software/rk3506-linux/` | RK3506 Linux 侧关键代码，包含 rpmsg/设备通信、MQTT 心跳、语音助手和 PowerClaw 边缘智能体 |
| `docs/firmware/` | 固件架构、构建说明和发布检查清单 |
| Web Dashboard | 独立仓库：[kqrekybjinn/energy-dashboard](https://github.com/kqrekybjinn/energy-dashboard)，用于浏览器端状态展示、通道控制和曲线查看 |

## 关联仓库

- Web Dashboard：[kqrekybjinn/energy-dashboard](https://github.com/kqrekybjinn/energy-dashboard)

## 核心能力

- Linux + RT-Thread AMP 分层架构：Linux 负责智能服务，RT-Thread 负责实时控制。
- MPPT 光伏充电：双 INA226 采样、NTC/ADC 温度采样、CC-CV 控制和故障锁存。
- BUS/UPS 能量管理：外部输入、电池电压、受控输出、DAC CC/CV、EC11、状态统计。
- RK3506 Linux 边缘服务：rpmsg 通信测试、MQTT 心跳上报、语音助手脚本和 PowerClaw 受限边缘智能体。
- PowerClaw 飞书入口：基于 ZeroClaw ARMv7 和专用 MCP，仅允许通过本地 device-core 查询四项 L0 状态，阻断模型直接访问硬件与系统写接口。
- 面向上级主控的 CAN 协同控制接口预留。
- RT-Thread MSH 命令行调试接口，便于上板测试和课程/竞赛演示。

## 构建说明

固件构建说明见：

```text
docs/firmware/BUILD.md
```

两个固件子工程可直接导入 RT-Thread Studio，也可使用 RT-Thread Env/SCons 构建。

## 安全说明

本项目涉及光伏输入、电池充放电、大电流 DC-DC、直流母线和后级逆变扩展。打样和上电前必须配合限流电源、保险丝、BMS、电子负载、隔离调试和必要的硬件过压/过流/过温保护。软件保护不能替代硬件保护。
