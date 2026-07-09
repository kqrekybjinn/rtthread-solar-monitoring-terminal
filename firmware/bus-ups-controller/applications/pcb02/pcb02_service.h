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

#endif
