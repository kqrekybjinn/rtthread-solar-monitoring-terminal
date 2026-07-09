# Build Guide

## 开发环境

推荐使用：

- RT-Thread Studio
- RT-Thread Env/SCons
- ARM GCC toolchain

两个子工程均基于 STM32G474RET6 + RT-Thread BSP 整理，可按 RT-Thread Studio 工程导入，也可使用 SCons 命令行构建。

## 子工程路径

```text
firmware/mppt-controller
firmware/bus-ups-controller
```

## 命令行构建参考

进入任一子工程目录后，根据本机 RT-Thread Studio/Env 安装路径设置环境变量：

```powershell
$env:PYTHONUTF8 = "1"
$env:RTT_ROOT = "$PWD\rt-thread"
$env:RTT_CC = "gcc"
$env:RTT_EXEC_PATH = "<RT-Thread Studio>\platform\env_released\env\tools\gnu_gcc\arm_gcc\mingw\bin"
scons -j16
```

如果需要清理：

```powershell
scons -c
```

## RT-Thread Studio

建议使用 RT-Thread Studio 导入子工程目录：

```text
firmware/mppt-controller
firmware/bus-ups-controller
```

导入后检查：

- MCU 型号是否为 STM32G474RET6。
- Console 串口配置是否匹配板卡。
- `.config`、`rtconfig.h` 是否随工程加载。
- `board/CubeMX_Config/CubeMX_Config.ioc` 是否与实际引脚一致。

## 当前限制

- CAN/FDCAN 驱动接口已为上级协同控制预留，具体协议和实测仍需补充。
- 高功率长时间运行、温升、保护阈值和电池适配参数需结合实物测试整定。
- 固件必须配合保险丝、BMS、驱动硬件保护和限流调试。
