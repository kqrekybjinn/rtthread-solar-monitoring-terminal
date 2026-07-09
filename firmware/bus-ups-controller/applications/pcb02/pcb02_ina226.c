#include "pcb02_ina226.h"

#define INA226_REG_CONFIG        0x00
#define INA226_REG_SHUNT_VOLT   0x01
#define INA226_REG_BUS_VOLT     0x02
#define INA226_REG_POWER        0x03
#define INA226_REG_CURRENT      0x04
#define INA226_REG_CALIBRATION  0x05

#define INA226_CONFIG_AVG4_BUS588US_SHUNT588US_CONT  0x4127

static rt_err_t ina226_write_u16(pcb02_ina226_t *dev, rt_uint8_t reg, rt_uint16_t value)
{
    rt_uint8_t buf[3];

    buf[0] = reg;
    buf[1] = (rt_uint8_t)(value >> 8);
    buf[2] = (rt_uint8_t)(value & 0xff);

    return (rt_i2c_master_send(dev->bus, dev->addr, 0, buf, sizeof(buf)) == sizeof(buf)) ?
           RT_EOK : -RT_ERROR;
}

static rt_err_t ina226_read_u16(pcb02_ina226_t *dev, rt_uint8_t reg, rt_uint16_t *value)
{
    rt_uint8_t data[2];
    struct rt_i2c_msg msgs[2];

    msgs[0].addr = dev->addr;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = &reg;
    msgs[0].len = 1;

    msgs[1].addr = dev->addr;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = data;
    msgs[1].len = sizeof(data);

    if (rt_i2c_transfer(dev->bus, msgs, 2) != 2)
    {
        return -RT_ERROR;
    }

    *value = ((rt_uint16_t)data[0] << 8) | data[1];
    return RT_EOK;
}

rt_err_t pcb02_ina226_init(pcb02_ina226_t *dev,
                           const char *bus_name,
                           rt_uint8_t addr,
                           float shunt_ohm,
                           float max_current_a)
{
    float calibration;

    if (dev == RT_NULL || bus_name == RT_NULL || shunt_ohm <= 0.0f || max_current_a <= 0.0f)
    {
        return -RT_EINVAL;
    }

    rt_memset(dev, 0, sizeof(*dev));
    dev->bus = (struct rt_i2c_bus_device *)rt_device_find(bus_name);
    if (dev->bus == RT_NULL)
    {
        return -RT_ENOSYS;
    }

    dev->addr = addr;
    dev->shunt_ohm = shunt_ohm;
    dev->max_current_a = max_current_a;
    dev->current_lsb_a = max_current_a / 32768.0f;
    dev->power_lsb_w = 25.0f * dev->current_lsb_a;

    calibration = 0.00512f / (dev->current_lsb_a * shunt_ohm);
    if (calibration > 65535.0f)
    {
        calibration = 65535.0f;
    }

    if (ina226_write_u16(dev, INA226_REG_CONFIG, INA226_CONFIG_AVG4_BUS588US_SHUNT588US_CONT) != RT_EOK)
    {
        return -RT_ERROR;
    }

    return ina226_write_u16(dev, INA226_REG_CALIBRATION, (rt_uint16_t)calibration);
}

rt_err_t pcb02_ina226_read_bus_voltage(pcb02_ina226_t *dev, float *voltage_v)
{
    rt_uint16_t raw;
    rt_err_t ret = ina226_read_u16(dev, INA226_REG_BUS_VOLT, &raw);

    if (ret == RT_EOK)
    {
        *voltage_v = (float)raw * 0.00125f;
    }

    return ret;
}

rt_err_t pcb02_ina226_read_shunt_current(pcb02_ina226_t *dev, float *current_a)
{
    rt_uint16_t raw;
    rt_err_t ret = ina226_read_u16(dev, INA226_REG_CURRENT, &raw);

    if (ret == RT_EOK)
    {
        *current_a = (float)((rt_int16_t)raw) * dev->current_lsb_a;
    }

    return ret;
}

rt_err_t pcb02_ina226_read_bus_power(pcb02_ina226_t *dev, float *power_w)
{
    rt_uint16_t raw;
    rt_err_t ret = ina226_read_u16(dev, INA226_REG_POWER, &raw);

    if (ret == RT_EOK)
    {
        *power_w = (float)raw * dev->power_lsb_w;
    }

    return ret;
}
