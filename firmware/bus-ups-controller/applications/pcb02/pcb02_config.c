#include "pcb02_config.h"

static const pcb02_config_t g_pcb02_config =
{
    .mppt_mode = 1,
    .output_mode = 1,
    .enable_predictive_pwm = 1,
    .enable_fan = 1,

    .pwm_resolution_bits = 11,
    .pwm_frequency_hz = 39000,
    .routine_interval_ms = 250,
    .error_time_limit_ms = 1000,
    .error_count_limit = 5,

    .voltage_battery_max = 12.6000f,
    .voltage_battery_min = 3.0000f,
    .current_charging_max = 1.0000f,
    .temperature_fan_c = 60.0f,
    .temperature_max_c = 90.0f,
    .ntc_resistance_ohm = 10000.0f,
    .voltage_dropout = 1.0000f,
    .voltage_battery_thresh = 1.5000f,
    .current_in_absolute = 31.0000f,
    .current_out_absolute = 50.0000f,
    .input_voltage_div_ratio = 1.0000f,
    .output_voltage_div_ratio = 1.0000f,
    .ina_pv_shunt_ohm = 0.0020f,
    .ina_out_shunt_ohm = 0.0020f,
    .ina_pv_max_current_a = 40.0000f,
    .ina_out_max_current_a = 40.0000f,
    .ppwm_margin_percent = 99.5000f,
    .pwm_max_duty_percent = 97.0000f,
    .vin_system_min = 8.0000f,
    .i2c_bus_name = "i2c3",
    .ina_pv_addr = 0x40,
    .ina_out_addr = 0x41,
    .pwm_device_name = "pwm1",
    .pwm_channel = 1,
    .ntc_adc_device_name = "adc1",
    .ntc_adc_channel = 1,
    .adc_reference_mv = 3300.0f,
    .ntc_pullup_ohm = 10000.0f,
    .ntc_nominal_ohm = 10000.0f,
    .ntc_beta = 3950.0f,
    .ntc_nominal_temperature_c = 25.0f,
};

const pcb02_config_t *pcb02_get_config(void)
{
    return &g_pcb02_config;
}
