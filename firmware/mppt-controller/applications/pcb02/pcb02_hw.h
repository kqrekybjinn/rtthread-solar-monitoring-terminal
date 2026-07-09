#ifndef PCB02_HW_H
#define PCB02_HW_H

#include "pcb02_config.h"

rt_err_t pcb02_hw_init(const pcb02_config_t *cfg);
void pcb02_hw_safe_state(void);
void pcb02_hw_apply_outputs(rt_uint8_t buck_enable,
                            rt_uint8_t bypass_enable,
                            rt_uint8_t fan_enable,
                            rt_uint8_t fault_active,
                            rt_uint16_t pwm_counts,
                            rt_uint16_t pwm_max_counts);

#endif
