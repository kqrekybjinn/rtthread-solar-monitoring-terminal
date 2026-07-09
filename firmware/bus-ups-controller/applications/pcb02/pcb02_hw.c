#include "pcb02_hw.h"

#include <rtdevice.h>

#ifdef RT_USING_PWM
#include <drivers/dev_pwm.h>
static struct rt_device_pwm *s_pwm_dev;
static const pcb02_config_t *s_cfg;
#endif

void pcb02_hw_safe_state(void)
{
    rt_pin_write(PCB02_PIN_DRV_EN, PIN_LOW);
    rt_pin_write(PCB02_PIN_BACKFLOW_EN, PIN_LOW);
    rt_pin_write(PCB02_PIN_FAN_EN, PIN_LOW);
    rt_pin_write(PCB02_PIN_LED_RUN, PIN_LOW);
    rt_pin_write(PCB02_PIN_LED_FAULT, PIN_HIGH);
    rt_pin_write(PCB02_PIN_CAN_STB, PIN_LOW);
    rt_pin_write(PCB02_PIN_FAULT_N, PIN_LOW);
}

rt_err_t pcb02_hw_init(const pcb02_config_t *cfg)
{
    rt_pin_mode(PCB02_PIN_DRV_EN, PIN_MODE_OUTPUT);
    rt_pin_mode(PCB02_PIN_BACKFLOW_EN, PIN_MODE_OUTPUT);
    rt_pin_mode(PCB02_PIN_FAN_EN, PIN_MODE_OUTPUT);
    rt_pin_mode(PCB02_PIN_LED_RUN, PIN_MODE_OUTPUT);
    rt_pin_mode(PCB02_PIN_LED_FAULT, PIN_MODE_OUTPUT);
    rt_pin_mode(PCB02_PIN_CAN_STB, PIN_MODE_OUTPUT);
    rt_pin_mode(PCB02_PIN_FAULT_N, PIN_MODE_OUTPUT_OD);

    rt_pin_mode(PCB02_PIN_CHG_EN, PIN_MODE_INPUT_PULLDOWN);
    rt_pin_mode(PCB02_PIN_INA_PV_ALERT, PIN_MODE_INPUT_PULLUP);
    rt_pin_mode(PCB02_PIN_INA_OUT_ALERT, PIN_MODE_INPUT_PULLUP);

    pcb02_hw_safe_state();

#ifdef RT_USING_PWM
    s_cfg = cfg;
    s_pwm_dev = (struct rt_device_pwm *)rt_device_find(cfg->pwm_device_name);
    if (s_pwm_dev == RT_NULL)
    {
        rt_kprintf("pcb02: pwm device %s not found, pwm output disabled\r\n", cfg->pwm_device_name);
    }
#else
    RT_UNUSED(cfg);
    rt_kprintf("pcb02: RT_USING_PWM is disabled, pwm output is simulated only\r\n");
#endif

    return RT_EOK;
}

void pcb02_hw_apply_outputs(rt_uint8_t buck_enable,
                            rt_uint8_t bypass_enable,
                            rt_uint8_t fan_enable,
                            rt_uint8_t fault_active,
                            rt_uint16_t pwm_counts,
                            rt_uint16_t pwm_max_counts)
{
    rt_pin_write(PCB02_PIN_DRV_EN, buck_enable ? PIN_HIGH : PIN_LOW);
    rt_pin_write(PCB02_PIN_BACKFLOW_EN, bypass_enable ? PIN_HIGH : PIN_LOW);
    rt_pin_write(PCB02_PIN_FAN_EN, fan_enable ? PIN_HIGH : PIN_LOW);
    rt_pin_write(PCB02_PIN_LED_RUN, buck_enable ? PIN_HIGH : PIN_LOW);
    rt_pin_write(PCB02_PIN_LED_FAULT, fault_active ? PIN_HIGH : PIN_LOW);
    rt_pin_write(PCB02_PIN_FAULT_N, fault_active ? PIN_LOW : PIN_HIGH);

#ifdef RT_USING_PWM
    if (s_pwm_dev != RT_NULL && s_cfg != RT_NULL && pwm_max_counts > 0)
    {
        rt_uint32_t period_ns = 1000000000UL / s_cfg->pwm_frequency_hz;
        rt_uint32_t pulse_ns = ((rt_uint64_t)period_ns * pwm_counts) / pwm_max_counts;

        rt_pwm_set(s_pwm_dev, s_cfg->pwm_channel, period_ns, pulse_ns);
        if (buck_enable && pwm_counts > 0)
        {
            rt_pwm_enable(s_pwm_dev, s_cfg->pwm_channel);
        }
        else
        {
            rt_pwm_disable(s_pwm_dev, s_cfg->pwm_channel);
        }
    }
#else
    RT_UNUSED(pwm_counts);
    RT_UNUSED(pwm_max_counts);
#endif
}
