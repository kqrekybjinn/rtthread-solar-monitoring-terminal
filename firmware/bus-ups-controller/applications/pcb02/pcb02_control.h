#ifndef PCB02_CONTROL_H
#define PCB02_CONTROL_H

#include "pcb02_config.h"

typedef struct pcb02_measurements
{
    float voltage_input;
    float voltage_output;
    float current_input;
    float current_output;
    float power_input;
    float power_output;
    float battery_percent;
    float buck_efficiency;
    float temperature_c;
} pcb02_measurements_t;

typedef struct pcb02_faults
{
    rt_uint8_t over_temperature;
    rt_uint8_t input_over_current;
    rt_uint8_t output_over_current;
    rt_uint8_t output_over_voltage;
    rt_uint8_t fatal_low_voltage;
    rt_uint8_t input_under_voltage;
    rt_uint8_t battery_not_connected;
    rt_uint8_t recovery_requested;
    rt_uint8_t error_count;
} pcb02_faults_t;

typedef struct pcb02_control_state
{
    rt_uint8_t buck_enable;
    rt_uint8_t bypass_enable;
    rt_uint8_t recovery_requested;
    rt_uint8_t charging_pause;
    rt_uint16_t pwm_counts;
    rt_uint16_t predictive_pwm_counts;
    float power_input_prev;
    float voltage_input_prev;
    float watt_hours;
} pcb02_control_state_t;

rt_uint16_t pcb02_pwm_max_counts(const pcb02_config_t *cfg);
rt_uint16_t pcb02_pwm_limited_counts(const pcb02_config_t *cfg);
rt_uint16_t pcb02_predictive_pwm_counts(const pcb02_config_t *cfg,
                                        const pcb02_measurements_t *meas);
void pcb02_update_derived_measurements(const pcb02_config_t *cfg,
                                       pcb02_measurements_t *meas);
rt_uint8_t pcb02_eval_protection(const pcb02_config_t *cfg,
                                 const pcb02_measurements_t *meas,
                                 pcb02_faults_t *faults);
rt_uint8_t pcb02_should_enable_backflow(const pcb02_config_t *cfg,
                                        const pcb02_measurements_t *meas);
void pcb02_control_state_init(pcb02_control_state_t *state);
rt_uint16_t pcb02_charging_step(const pcb02_config_t *cfg,
                                const pcb02_measurements_t *meas,
                                const pcb02_faults_t *faults,
                                pcb02_control_state_t *state);

#endif
