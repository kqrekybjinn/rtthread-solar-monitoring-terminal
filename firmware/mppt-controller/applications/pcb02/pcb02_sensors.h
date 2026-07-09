#ifndef PCB02_SENSORS_H
#define PCB02_SENSORS_H

#include "pcb02_config.h"
#include "pcb02_control.h"

rt_err_t pcb02_sensors_init(const pcb02_config_t *cfg);
rt_err_t pcb02_sensors_read(const pcb02_config_t *cfg, pcb02_measurements_t *meas);

#endif
