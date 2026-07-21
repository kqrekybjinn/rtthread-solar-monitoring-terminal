# RK3506 Linux Software

本目录保存 Linux 侧可公开的关键源码，不包含模型权重、音频样本、已编译二进制和私有运行配置。

```text
rk3506-linux/
├── device-core/      # rpmsg/AMP 设备通信测试
├── mqtt-gateway/     # MQTT 心跳与设备状态上报示例
├── voice-assistant/  # 录音、唤醒、ASR/TTS 调用和语音交互脚本
├── edge-agent/       # 早期受限智能体动作分发与白名单控制
└── powerclaw/        # 基于 ZeroClaw 的 ARMv7 飞书边缘智能体
```

`*.env.example` 文件仅作为运行配置模板，部署到板端时需要按实际音频设备、模型路径和云服务密钥重新填写。

## PowerClaw

PowerClaw 是本项目面向 RK3506 实现的轻量级边缘智能体发行层。它基于固定版本的 ZeroClaw ARMv7 运行时，通过独立 MCP 适配器访问本地 `device-core`，当前仅开放系统、MQTT、AMP 和能力清单四项只读查询。模型不能直接调用 Shell、文件写入、通用 HTTP、GPIO、I2C、SPI、RPMsg 或固件工具。

源码、构建方式、飞书长连接配置及安全边界见 [`powerclaw/README.md`](powerclaw/README.md)。仓库不包含模型密钥、飞书密钥、板端密码、编译产物或上游预编译二进制。
