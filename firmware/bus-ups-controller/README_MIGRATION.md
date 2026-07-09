# STM32G474 RT-Thread MPPT Port

This directory is the standalone RT-Thread/SCons project for the PCB02 MPPT controller port.

## Project Type

- MCU: STM32G474RET6
- RTOS: RT-Thread
- Build system: SCons
- Source baseline: `mppt_prj/original-esp32-mppt/ARDUINO_MPPT_FIRMWARE_V2.1`
- Main application directory: `applications/pcb02`

This is not an `rtthread-cmake` project. It is the current working RT-Thread BSP project copied from the tested RT-Thread Studio workspace and made self-contained under this repository.

## Important Directories

- `applications/pcb02`: STM32G474 MPPT application modules.
- `board/CubeMX_Config`: STM32G474RET6 CubeMX pin and peripheral configuration reference.
- `libraries/HAL_Drivers`: RT-Thread STM32 HAL driver glue.
- `packages/stm32g4_hal_driver-latest`: STM32G4 HAL package.
- `rt-thread`: RT-Thread kernel and components.

## PCB02 Module Split

- `pcb02_app`: RT-Thread task entry, periodic service call and low-rate status log.
- `pcb02_service`: external control API for future CAN/UART/RK3506 control.
- `pcb02_sensors`: INA226 and ADC aggregation.
- `pcb02_control`: MPPT, CC/CV and protection logic.
- `pcb02_hw`: GPIO/PWM safe state and output application.
- `pcb02_shell`: MSH test commands.
- `pcb02_ina226`: INA226 I2C driver.
- `pcb02_adc`: NTC ADC driver.
- `pcb02_config`: default runtime parameters and pin mapping.

## Build

Set the toolchain path, then build with SCons:

```bat
set PYTHONUTF8=1
set RTT_ROOT=%CD%\rt-thread
set RTT_CC=gcc
set RTT_EXEC_PATH=E:\toolchain\RT-Thread\RT-ThreadStudio\platform\env_released\env\tools\gnu_gcc\arm_gcc\mingw\bin
set PATH=C:\Users\Amu\.cache\codex-runtimes\codex-primary-runtime\dependencies\python;C:\Users\Amu\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\Scripts;%RTT_EXEC_PATH%;%PATH%
scons -j16
```

Expected output files after a successful build:

- `rt-thread.elf`
- `rt-thread.map`
- `rtthread.bin`

## MSH Test Commands

The CAN/FDCAN driver is intentionally not enabled yet. Use RT-Thread MSH to test the service layer first:

```text
pcb02 status
pcb02 enable 1
pcb02 enable 0
pcb02 mode mppt
pcb02 mode cv
pcb02 output charger
pcb02 output supply
pcb02 limits 12.6 5.0
pcb02 clear_fault
pcb02 safe
```

Future CAN support should parse CAN frames and call the same `pcb02_service_*` APIs used by these MSH commands.

## Current Limitation

RT-Thread generic FDCAN is disabled because the BSP `drv_fdcan.c` API does not match the currently copied STM32G4 HAL package. The PA11/PA12 FDCAN1 pin plan remains in CubeMX. The recommended next step is a small PCB02-specific FDCAN adapter instead of patching the generic BSP driver.
