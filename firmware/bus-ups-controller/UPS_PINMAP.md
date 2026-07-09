# UPS extension pin map without OLED

Target MCU: STM32G474RET6, LQFP64.

OLED has been removed from the UPS firmware and pin plan. The original UPS power-management functions remain, and the requested expansion signals are added directly.

## Original UPS resources kept

| Function | MCU pin | Notes |
| --- | --- | --- |
| LOAD1_EN | PC6 | Original low-active load output |
| LOAD2_EN | PC7 | Original low-active load output |
| LOAD3_EN | PC8 | Original low-active load output |
| STATUS_LED | PB0 | Original UPS status LED |
| USER_KEY | PC13 | Original key input; `ups page` now only reports OLED removed |
| I2C_SCL | PB8 | Shared I2C bus |
| I2C_SDA | PB9 | Shared I2C bus |
| INPUT_DC_ADC | PA0 | Original input voltage sampling |
| BATTERY_ADC | PA1 | Original battery voltage sampling |
| DEBUG_TX/RX | PA2/PA3 | RT-Thread shell console |
| SWDIO/SWCLK | PA13/PA14 | Download/debug |

## Added resources

| New signal | MCU pin | Peripheral | Firmware use |
| --- | --- | --- | --- |
| CSS | PB11 | GPIO output | Test with `upsio out css 1` |
| UVCH1 | PB12 | GPIO output | Test with `upsio out uvch1 1` |
| UVCH2 | PB13 | GPIO output | Test with `upsio out uvch2 1` |
| UVCH3 | PB14 | GPIO output | Test with `upsio out uvch3 1` |
| I2C_SCL | PB8 | I2C shared bus | For INA226 and future I2C peripherals |
| I2C_SDA | PB9 | I2C shared bus | For INA226 and future I2C peripherals |
| VIN_1 | PC4 | ADC2_IN5 | Extra voltage sampling input |
| NTC | PC5 | ADC2_IN11 | Extra NTC sampling input |
| CC | PA4 | DAC1_OUT1 | Analog current reference, test with `upsio dac cc 1000` |
| CV | PA5 | DAC1_OUT2 | Analog voltage reference, test with `upsio dac cv 1000` |
| EC11_A | PB6 | TIM4_CH1 | EC11 encoder A, test with `upsio enc` |
| EC11_B | PB7 | TIM4_CH2 | EC11 encoder B, test with `upsio enc` |

Hardware note: PA4/PA5 DAC outputs are raw 0-3.3 V analog signals. They should drive CC/CV control through the required RC filtering, op-amp scaling/buffering and protection network, not directly into a high-power node.
