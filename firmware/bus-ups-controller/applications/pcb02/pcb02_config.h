#ifndef PCB02_CONFIG_H
#define PCB02_CONFIG_H

#include <rtthread.h>
#include <drv_gpio.h>

/* STM32G474 pin mapping migrated from the ESP32 MPPT firmware. */
#define PCB02_PIN_BACKFLOW_EN      GET_PIN(C, 6)   /* ESP32 GPIO27: backflow_MOSFET */
#define PCB02_PIN_PWM_BUCK         GET_PIN(A, 8)   /* ESP32 GPIO33: buck_IN, TIM1_CH1 */
#define PCB02_PIN_DRV_EN           GET_PIN(B, 12)  /* ESP32 GPIO32: buck_EN */
#define PCB02_PIN_LED_RUN          GET_PIN(B, 1)   /* ESP32 GPIO2: LED */
#define PCB02_PIN_FAN_EN           GET_PIN(B, 0)   /* ESP32 GPIO16: FAN */
#define PCB02_PIN_INA_PV_ALERT     GET_PIN(C, 0)   /* ESP32 GPIO35: INA1_ALERT */
#define PCB02_PIN_INA_OUT_ALERT    GET_PIN(B, 9)   /* ESP32 GPIO34: INA2_ALERT */
#define PCB02_PIN_NTC_MOS_ADC      GET_PIN(A, 0)   /* ESP32 GPIO39/SENSOR_VN: TempSensor */

/* New PCB-02 hardwired controls. */
#define PCB02_PIN_CHG_EN           GET_PIN(B, 5)   /* PCB-03 -> PCB-02 charge enable */
#define PCB02_PIN_FAULT_N          GET_PIN(A, 9)   /* PCB-02 -> PCB-03 low-active fault, open-drain */
#define PCB02_PIN_CAN_STB          GET_PIN(A, 10)  /* TCAN3413 STB, low = normal mode */
#define PCB02_PIN_LED_FAULT        GET_PIN(B, 2)   /* Local fault LED */

typedef struct pcb02_config
{
    rt_uint8_t mppt_mode;
    rt_uint8_t output_mode;
    rt_uint8_t enable_predictive_pwm;
    rt_uint8_t enable_fan;

    rt_uint16_t pwm_resolution_bits;
    rt_uint32_t pwm_frequency_hz;
    rt_uint32_t routine_interval_ms;
    rt_uint32_t error_time_limit_ms;
    rt_uint8_t error_count_limit;

    float voltage_battery_max;
    float voltage_battery_min;
    float current_charging_max;
    float temperature_fan_c;
    float temperature_max_c;
    float ntc_resistance_ohm;
    float voltage_dropout;
    float voltage_battery_thresh;
    float current_in_absolute;
    float current_out_absolute;
    float input_voltage_div_ratio;
    float output_voltage_div_ratio;
    float ina_pv_shunt_ohm;
    float ina_out_shunt_ohm;
    float ina_pv_max_current_a;
    float ina_out_max_current_a;
    float ppwm_margin_percent;
    float pwm_max_duty_percent;
    float vin_system_min;

    const char *i2c_bus_name;
    rt_uint8_t ina_pv_addr;
    rt_uint8_t ina_out_addr;
    const char *pwm_device_name;
    rt_uint8_t pwm_channel;

    const char *ntc_adc_device_name;
    rt_int8_t ntc_adc_channel;
    float adc_reference_mv;
    float ntc_pullup_ohm;
    float ntc_nominal_ohm;
    float ntc_beta;
    float ntc_nominal_temperature_c;
} pcb02_config_t;

const pcb02_config_t *pcb02_get_config(void);

#endif
