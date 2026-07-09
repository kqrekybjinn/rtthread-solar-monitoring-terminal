# Release Checklist

## 已整理

- `mppt-controller` 和 `bus-ups-controller` 已作为两个子工程放入 `firmware/`。
- 已排除 `build/`、`.vscode/`、`__pycache__/`。
- 已排除 `*.elf`、`*.bin`、`*.hex`、`*.map`、`*.pyc`、`.sconsign.dblite`。
- 已排除 `*.bak`、`*.bak_*`、`*.bak_before_*` 历史备份。
- 已添加根目录 README、架构说明、构建说明和发布检查清单。

## 发布前检查

- 确认没有个人 WiFi、Token、云端密钥、私有服务器地址。
- 确认 `.config`、`rtconfig.h`、`SConstruct`、`SConscript` 已保留。
- 确认 `board/CubeMX_Config/CubeMX_Config.ioc` 与当前硬件引脚一致。
- 确认 README 没有宣称尚未实测的高功率能力。
- 确认第三方组件自带的必要声明保留在对应组件目录内。

## 后续建议

- 补充 CAN 协议文档。
- 补充 MSH 命令示例输出。
- 补充上板测试记录。
- 补充与硬件仓库的版本对应关系。
