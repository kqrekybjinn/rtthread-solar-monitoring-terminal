#ifndef PCB02_ADC_H
#define PCB02_ADC_H

#include <rtthread.h>

#include "pcb02_config.h"

rt_err_t pcb02_adc_init(const pcb02_config_t *cfg);
rt_err_t pcb02_adc_read_ntc_temperature(const pcb02_config_t *cfg, float *temperature_c);

#endif
