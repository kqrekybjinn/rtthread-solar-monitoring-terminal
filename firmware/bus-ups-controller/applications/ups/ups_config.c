#include "ups_config.h"

static const ups_config_t g_ups_config =
{
    .adc_device_name = "adc1",
    .input_adc_channel = 1,
    .battery_adc_channel = 2,
    .adc_reference_mv = 3300.0f,
    .input_voltage_div_ratio = 8.26f,
    .battery_voltage_div_ratio = 9.40090566f,
    .input_power_fail_threshold_v = 9.0f,
    .input_power_recover_threshold_v = 9.5f,

    .i2c_bus_name = "i2c1",
    .ina226_addr = 0x40,
    .ina226_shunt_ohm = 0.002f,
    .ina226_max_current_a = 40.0f,

    .sample_interval_ms = 300,
};

const ups_config_t *ups_get_config(void)
{
    return &g_ups_config;
}




