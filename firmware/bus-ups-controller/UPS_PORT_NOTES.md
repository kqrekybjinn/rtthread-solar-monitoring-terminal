# ESP32 UPS to STM32G474 + RT-Thread Port Notes

## Ported Scope

This project ports the local, non-network functions of the original ESP32 UPS firmware to STM32G474 + RT-Thread.

Implemented:

- Input DC voltage measurement.
- Battery voltage measurement.
- INA226 output voltage/current/power measurement.
- Output energy accumulation in Wh.
- Input power loss/recovery detection.
- Power loss recovery counter.
- Three low-active load outputs.
- User key page switching.
- Status LED.
- SSD1316/SSD1306-compatible 128x32 OLED status display through GPIO bit-banged SPI.
- RT-Thread MSH local control commands.

Not ported to STM32:

- WiFi.
- Web server.
- UDP status upload/control.
- SMTP email alarm.
- Arduino OTA.
- Chinese OLED font display.

These network functions should be implemented by the upper controller, such as RK3506, if needed.

## Pin Mapping

- `PA0`: input DC voltage ADC.
- `PA1`: battery voltage ADC.
- `PB8`: INA226 I2C SCL.
- `PB9`: INA226 I2C SDA.
- `PC6`: load 1 enable, low active.
- `PC7`: load 2 enable, low active.
- `PC8`: load 3 enable, low active.
- `PB0`: status LED.
- `PC13`: user key, pull-up input.
- `PA4`: OLED CS.
- `PA5`: OLED SCK.
- `PA7`: OLED MOSI.
- `PB1`: OLED DC.
- `PB10`: OLED RST.

## Runtime Commands

Use RT-Thread MSH:

```text
ups status
ups load 1 1
ups load 1 0
ups load 2 1
ups load 3 0
ups page
ups safe
```

## Behavior

- Input voltage below `9.0V` is treated as power loss.
- Input voltage above `9.5V` is treated as power recovery.
- Battery voltage is measured and displayed, but it is not yet used for automatic load cutoff.
- Three load outputs default to off because the hardware is low-active.

## Build Result

Verified with SCons:

```text
ROM: 118928 B / 512 KB
RAM: 6384 B / 128 KB
Output: rtthread.bin
```

## Next Work

- Add battery undervoltage load cutoff policy after confirming the 4S chemistry and cutoff voltage.
- Add BM3451 fault/status input if the hardware exposes it.
- Add persistent storage for power failure count and energy.
- Add communication protocol to RK3506 or another upper controller.
