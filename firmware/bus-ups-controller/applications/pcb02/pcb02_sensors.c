#include "pcb02_sensors.h"

#include "pcb02_adc.h"
#include "pcb02_ina226.h"

static pcb02_ina226_t s_ina_pv;
static pcb02_ina226_t s_ina_out;

rt_err_t pcb02_sensors_init(const pcb02_config_t *cfg)
{
    rt_err_t ret_pv;
    rt_err_t ret_out;

    (void)pcb02_adc_init(cfg);

    ret_pv = pcb02_ina226_init(&s_ina_pv,
                               cfg->i2c_bus_name,
                               cfg->ina_pv_addr,
                               cfg->ina_pv_shunt_ohm,
                               cfg->ina_pv_max_current_a);
    ret_out = pcb02_ina226_init(&s_ina_out,
                                cfg->i2c_bus_name,
                                cfg->ina_out_addr,
                                cfg->ina_out_shunt_ohm,
                                cfg->ina_out_max_current_a);

    if (ret_pv != RT_EOK || ret_out != RT_EOK)
    {
        rt_kprintf("pcb02: INA226 init failed pv=%d out=%d, control remains safe\r\n", ret_pv, ret_out);
        return -RT_ERROR;
    }

    rt_kprintf("pcb02: INA226 ready on %s, pv=0x%02x out=0x%02x\r\n",
               cfg->i2c_bus_name, cfg->ina_pv_addr, cfg->ina_out_addr);
    return RT_EOK;
}

rt_err_t pcb02_sensors_read(const pcb02_config_t *cfg, pcb02_measurements_t *meas)
{
    rt_err_t ret = RT_EOK;
    float pv_bus = 0.0f;
    float out_bus = 0.0f;
    float pv_current = 0.0f;
    float out_current = 0.0f;
    float pv_power = 0.0f;
    float out_power = 0.0f;

    if (meas == RT_NULL)
    {
        return -RT_EINVAL;
    }

    ret |= pcb02_ina226_read_bus_voltage(&s_ina_pv, &pv_bus);
    ret |= pcb02_ina226_read_bus_voltage(&s_ina_out, &out_bus);
    ret |= pcb02_ina226_read_shunt_current(&s_ina_pv, &pv_current);
    ret |= pcb02_ina226_read_shunt_current(&s_ina_out, &out_current);
    ret |= pcb02_ina226_read_bus_power(&s_ina_pv, &pv_power);
    ret |= pcb02_ina226_read_bus_power(&s_ina_out, &out_power);

    if (ret != RT_EOK)
    {
        return -RT_ERROR;
    }

    meas->voltage_input = pv_bus * cfg->input_voltage_div_ratio;
    meas->voltage_output = out_bus * cfg->output_voltage_div_ratio;
    meas->current_input = pv_current;
    meas->current_output = out_current;
    meas->power_input = pv_power * cfg->input_voltage_div_ratio;
    meas->power_output = out_power * cfg->output_voltage_div_ratio;
    if (pcb02_adc_read_ntc_temperature(cfg, &meas->temperature_c) != RT_EOK)
    {
        meas->temperature_c = cfg->ntc_nominal_temperature_c;
    }

    pcb02_update_derived_measurements(cfg, meas);
    return RT_EOK;
}
