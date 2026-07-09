#ifndef UPS_INA226_H
#define UPS_INA226_H

#include <rtthread.h>
#include <rtdevice.h>

typedef struct ups_ina226
{
    struct rt_i2c_bus_device *bus;
    rt_uint8_t addr;
    float shunt_ohm;
    float max_current_a;
    float current_lsb_a;
    float power_lsb_w;
} ups_ina226_t;

rt_err_t ups_ina226_init(ups_ina226_t *dev,
                         const char *bus_name,
                         rt_uint8_t addr,
                         float shunt_ohm,
                         float max_current_a);
rt_err_t ups_ina226_read_bus_voltage(ups_ina226_t *dev, float *voltage_v);
rt_err_t ups_ina226_read_current(ups_ina226_t *dev, float *current_a);
rt_err_t ups_ina226_read_power(ups_ina226_t *dev, float *power_w);

#endif
