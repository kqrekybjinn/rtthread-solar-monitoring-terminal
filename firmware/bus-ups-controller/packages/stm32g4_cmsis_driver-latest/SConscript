from building import *
import os

# Import environment variables
Import('env')

# Get the current working directory
cwd = GetCurrentDir()

# Initialize include paths and source files list
path = [os.path.join(cwd, 'Include')]
src = [os.path.join(cwd, 'Source', 'Templates', 'system_stm32g4xx.c')]

# Map microcontroller units (MCUs) to their corresponding startup files
mcu_startup_files = {
    'STM32G4A1xx': 'startup_stm32g4a1xx.s',
    'STM32G431xx': 'startup_stm32g431xx.s',
    'STM32G441xx': 'startup_stm32g441xx.s',
    'STM32G471xx': 'startup_stm32g471xx.s',
    'STM32G473xx': 'startup_stm32g473xx.s',
    'STM32G474xx': 'startup_stm32g474xx.s',
    'STM32G483xx': 'startup_stm32g483xx.s',
    'STM32G484xx': 'startup_stm32g484xx.s',
    'STM32G491xx': 'startup_stm32g491xx.s',
    'STM32GBK1CB': 'startup_stm32gbk1cb.s',
}

# Check each defined MCU, match the platform and append the appropriate startup file
cpp_defines_tuple = env.get('CPPDEFINES', [])
cpp_defines_list = [item[0] if isinstance(item, tuple) else item for item in cpp_defines_tuple]
for mcu, startup_file in mcu_startup_files.items():
    if mcu in cpp_defines_list:
        if rtconfig.PLATFORM in ['gcc', 'llvm-arm']:
            src += [os.path.join(cwd, 'Source', 'Templates', 'gcc', startup_file)]
        elif rtconfig.PLATFORM in ['armcc', 'armclang']:
            src += [os.path.join(cwd, 'Source', 'Templates', 'arm', startup_file)]
        elif rtconfig.PLATFORM in ['iccarm']:
            src += [os.path.join(cwd, 'Source', 'Templates', 'iar', startup_file)]
        break

# Define the build group
group = DefineGroup('STM32G4-CMSIS', src, depend=['PKG_USING_STM32G4_CMSIS_DRIVER'], CPPPATH=path)

# Return the build group
Return('group')
