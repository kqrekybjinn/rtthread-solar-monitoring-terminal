#ifndef UPS_CONFIG_H
#define UPS_CONFIG_H

#include <rtthread.h>
#include <drv_gpio.h>

#define UPS_PIN_LOAD1_EN        GET_PIN(C, 6)
#define UPS_PIN_LOAD2_EN        GET_PIN(C, 7)
#define UPS_PIN_LOAD3_EN        GET_PIN(C, 8)
#define UPS_PIN_STATUS_LED      GET_PIN(B, 0)
#define UPS_PIN_USER_KEY        GET_PIN(C, 13)

#define UPS_LOAD_ON_LEVEL       PIN_LOW
#define UPS_LOAD_OFF_LEVEL      PIN_HIGH

typedef struct ups_config
{
    const char *adc_device_name;
    rt_int8_t input_adc_channel;
    rt_int8_t battery_adc_channel;
    float adc_reference_mv;
    float input_voltage_div_ratio;
    float battery_voltage_div_ratio;
    float input_power_fail_threshold_v;
    float input_power_recover_threshold_v;

    const char *i2c_bus_name;
    rt_uint8_t ina226_addr;
    float ina226_shunt_ohm;
    float ina226_max_current_a;

    rt_uint32_t sample_interval_ms;
} ups_config_t;

const ups_config_t *ups_get_config(void);

#endif




