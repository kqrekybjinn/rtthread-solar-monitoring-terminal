.PHONY: all clean rebuild

PYTHON3 := C:/Users/Amu/.cache/codex-runtimes/codex-primary-runtime/dependencies/python
GCC_BIN := E:/toolchain/RT-Thread/RT-ThreadStudio/platform/env_released/env/tools/gnu_gcc/arm_gcc/mingw/bin

export PYTHONUTF8 := 1
export RTT_ROOT := E:/toolchain/RT-Thread/RT-ThreadStudio/workspace/ele1/rt-thread
export RTT_CC := gcc
export RTT_EXEC_PATH := $(GCC_BIN)
export PATH := $(PYTHON3);$(PYTHON3)/Scripts;$(GCC_BIN);$(PATH)

all:
	scons -j16
	python -c "import os, shutil; os.makedirs('Debug', exist_ok=True); shutil.copyfile('rt-thread.elf', 'Debug/rtthread.elf'); shutil.copyfile('rt-thread.elf', 'Debug/rt-thread.elf'); shutil.copyfile('rtthread.bin', 'Debug/rtthread.bin')"

clean:
	scons -c

rebuild: clean all
