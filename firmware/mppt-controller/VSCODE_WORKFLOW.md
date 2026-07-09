# VS Code 开发流程

本工程的唯一可信构建入口是根目录 `Makefile`，实际调用 RT-Thread 官方 SCons 构建。VS Code 只作为编辑器、任务入口和调试入口，不再依赖 RT-Thread Studio 的 managed build。

## 推荐工作流

1. 用 VS Code 打开本目录：

```powershell
code E:\toolchain\RT-Thread\RT-ThreadStudio\workspace\ele1
```

2. 编译：

```text
Terminal -> Run Build Task -> RT-Thread: build
```

等价命令：

```powershell
make all
```

3. 修改 RT-Thread 配置：

```text
Terminal -> Run Task -> RT-Thread: menuconfig
```

配置变更后执行：

```text
Terminal -> Run Task -> RT-Thread: pkgs update
Terminal -> Run Build Task -> RT-Thread: build
```

4. 烧录：

```text
Terminal -> Run Task -> ST-Link: flash bin
```

该任务调用 `STM32_Programmer_CLI.exe`。如果提示找不到程序，安装 STM32CubeProgrammer，或在 `.vscode/settings.json` 中把 `rtthread.stm32ProgrammerCli` 改成实际绝对路径。

5. 调试：

安装 VS Code 插件 `Cortex-Debug` 后，使用：

```text
Run and Debug -> ST-Link: debug rt-thread.elf
```

调试入口使用 `rt-thread.elf`，烧录二进制使用 `rtthread.bin`。

## 文件职责

- `applications/`：业务逻辑，例如流水灯、MPPT 状态机、CAN 协议处理。
- `board/`：板级初始化、CubeMX 配置、时钟和引脚配置。
- `libraries/HAL_Drivers/`：RT-Thread STM32 HAL 适配层。
- `packages/`：RT-Thread 包管理下载的 HAL/CMSIS 依赖。
- `rt-thread/`：RT-Thread 内核源码。
- `.vscode/tasks.json`：VS Code 编译、清理、menuconfig、pkgs、烧录任务。
- `.vscode/launch.json`：ST-Link 调试配置。

## 约束

- 不建议使用 WSL 作为主链路，避免 ST-Link USB、CubeMX、Windows 路径和 RT-Thread Env 混用。
- 不手改 `Debug/` 下生成物。`Debug/rtthread.elf` 只是为了兼容旧调试配置自动复制出来的镜像。
- 如果需要新增 ADC、PWM、FDCAN 驱动，优先改 `.config/Kconfig` 和 `board/CubeMX_Config/CubeMX_Config.ioc`，然后重新构建。
