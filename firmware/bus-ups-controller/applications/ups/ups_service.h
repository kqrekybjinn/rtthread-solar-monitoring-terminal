#ifndef UPS_SERVICE_H
#define UPS_SERVICE_H

#include "ups_config.h"

typedef struct ups_status
{
    float input_voltage_v;
    float battery_voltage_v;
    float output_voltage_v;
    float output_current_a;
    float output_power_w;
    float output_energy_wh;
    rt_uint8_t input_present;
    rt_uint8_t power_fail_active;
    rt_uint32_t power_fail_count;
    rt_uint32_t uptime_s;
    rt_uint8_t load_on[3];
    rt_uint8_t ina226_ready;
    rt_uint8_t adc_ready;
} ups_status_t;

rt_err_t ups_service_init(void);
void ups_service_step(void);
void ups_service_get_status(ups_status_t *status);
void ups_service_set_load(rt_uint8_t index, rt_uint8_t on);
void ups_service_toggle_page(void);
void ups_service_safe_outputs(void);

#endif
