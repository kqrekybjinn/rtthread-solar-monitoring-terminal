# RK3506 Linux Software

本目录保存 Linux 侧可公开的关键源码，不包含模型权重、音频样本、已编译二进制和私有运行配置。

```text
rk3506-linux/
├── device-core/      # rpmsg/AMP 设备通信测试
├── mqtt-gateway/     # MQTT 心跳与设备状态上报示例
├── voice-assistant/  # 录音、唤醒、ASR/TTS 调用和语音交互脚本
└── edge-agent/       # 受限智能体动作分发与白名单控制
```

`*.env.example` 文件仅作为运行配置模板，部署到板端时需要按实际音频设备、模型路径和云服务密钥重新填写。
