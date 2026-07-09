#ifndef PCB02_SERVICE_H
#define PCB02_SERVICE_H

#include "pcb02_config.h"
#include "pcb02_control.h"

typedef struct pcb02_status
{
    pcb02_config_t cfg;
    pcb02_measurements_t meas;
    pcb02_faults_t faults;
    pcb02_control_state_t ctrl;
    rt_uint8_t software_enable;
    rt_uint8_t hardware_chg_en;
    rt_uint8_t sensor_ready;
    rt_uint8_t fault_active;
    rt_uint8_t fan_enable;
    rt_uint8_t debug_pwm_active;
    rt_uint8_t cvtest_active;
    rt_uint8_t cvpid_active;
    rt_uint8_t cvpid_algo;
    rt_uint8_t chgtest_active;
    rt_uint8_t chgtest_state;
    rt_uint8_t mppttest_active;
    rt_uint8_t mppttest_algo;
    float cvtest_target_v;
    float cvtest_current_limit_a;
    rt_uint16_t cvtest_pwm_max_counts;
    float chgtest_vbat_cv;
    float chgtest_ichg_limit;
    float chgtest_vin_min;
    rt_uint16_t chgtest_pwm_max_counts;
    float chgtest_command_counts;
    float chgtest_integral;
    float chgtest_vref;
    rt_uint16_t chgtest_feedforward_counts;
    float mppttest_vout_max;
    float mppttest_iout_limit;
    rt_uint16_t mppttest_pwm_max_counts;
    rt_uint16_t mppttest_step_counts;
    float cvpid_kp;
    float cvpid_ki;
    float cvpid_kd;
    float cvpid_integral;
    float cvpid_vref;
    float cvpid_slew_v_s;
    float cvpid_ff_scale;
    float cvpid_prev_error;
    float cvpid_prev2_error;
    float cvpid_command_counts;
    rt_uint16_t cvpid_feedforward_counts;
} pcb02_status_t;

rt_err_t pcb02_service_init(void);
rt_err_t pcb02_service_step(void);
void pcb02_service_get_status(pcb02_status_t *status);
void pcb02_service_set_enable(rt_uint8_t enable);
void pcb02_service_set_mppt_mode(rt_uint8_t mppt_enable);
void pcb02_service_set_output_mode(rt_uint8_t output_mode);
void pcb02_service_set_charge_limits(float voltage_battery_max, float current_charging_max);
void pcb02_service_clear_fault_latch(void);
const pcb02_config_t *pcb02_service_get_config(void);
void pcb02_service_fan_test(rt_int8_t enable);
void pcb02_service_debug_pwm(rt_uint16_t permille);
void pcb02_service_cvtest(float target_v, float current_limit_a, rt_uint16_t max_permille);
void pcb02_service_cvpid(float target_v,
                         float current_limit_a,
                         rt_uint16_t max_permille,
                         float kp,
                         float ki,
                         float slew_v_s,
                         float ff_scale);
void pcb02_service_cvctrl(rt_uint8_t algo,
                          float target_v,
                          float current_limit_a,
                          rt_uint16_t max_permille,
                          float kp,
                          float ki,
                          float kd,
                          float slew_v_s,
                          float ff_scale);
void pcb02_service_chgtest(float vbat_cv,
                           float current_limit_a,
                           float vin_min,
                           rt_uint16_t max_permille);
void pcb02_service_mppttest(rt_uint8_t algo,
                            float vout_max,
                            float current_limit_a,
                            rt_uint16_t max_permille,
                            rt_uint16_t step_counts);

#endif
