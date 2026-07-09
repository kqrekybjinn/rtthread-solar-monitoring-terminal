#include "ups_service.h"

#include <rtdevice.h>
#ifdef RT_USING_ADC
#include <drivers/adc.h>
#endif

#include "ups_ina226.h"

static ups_status_t s_status;
static struct rt_mutex s_lock;
static rt_uint8_t s_lock_ready;
static rt_adc_device_t s_adc;
static ups_ina226_t s_ina226;
static rt_tick_t s_last_energy_tick;
static rt_uint8_t s_last_key_level = PIN_HIGH;

static void ups_lock(void)
{
    if (s_lock_ready)
    {
        rt_mutex_take(&s_lock, RT_WAITING_FOREVER);
    }
}

static void ups_unlock(void)
{
    if (s_lock_ready)
    {
        rt_mutex_release(&s_lock);
    }
}

static float ups_adc_voltage(const ups_config_t *cfg, rt_int8_t channel, float ratio)
{
#ifdef RT_USING_ADC
    rt_uint32_t raw;

    if (s_adc == RT_NULL)
    {
        return 0.0f;
    }

    raw = rt_adc_read(s_adc, channel);
    return ((float)raw * cfg->adc_reference_mv / 4095.0f / 1000.0f) * ratio;
#else
    RT_UNUSED(cfg);
    RT_UNUSED(channel);
    RT_UNUSED(ratio);
    return 0.0f;
#endif
}

static void ups_apply_load(rt_uint8_t index, rt_uint8_t on)
{
    rt_base_t pin;

    if (index == 0)
    {
        pin = UPS_PIN_LOAD1_EN;
    }
    else if (index == 1)
    {
        pin = UPS_PIN_LOAD2_EN;
    }
    else
    {
        pin = UPS_PIN_LOAD3_EN;
    }

    rt_pin_write(pin, on ? UPS_LOAD_ON_LEVEL : UPS_LOAD_OFF_LEVEL);
    s_status.load_on[index] = on ? 1 : 0;
}

rt_err_t ups_service_init(void)
{
    const ups_config_t *cfg = ups_get_config();
    rt_uint8_t i;

    rt_memset(&s_status, 0, sizeof(s_status));
    if (rt_mutex_init(&s_lock, "upssvc", RT_IPC_FLAG_PRIO) == RT_EOK)
    {
        s_lock_ready = 1;
    }

    rt_pin_mode(UPS_PIN_LOAD1_EN, PIN_MODE_OUTPUT);
    rt_pin_mode(UPS_PIN_LOAD2_EN, PIN_MODE_OUTPUT);
    rt_pin_mode(UPS_PIN_LOAD3_EN, PIN_MODE_OUTPUT);
    rt_pin_mode(UPS_PIN_STATUS_LED, PIN_MODE_OUTPUT);
    rt_pin_mode(UPS_PIN_USER_KEY, PIN_MODE_INPUT_PULLUP);

    for (i = 0; i < 3; i++)
    {
        ups_apply_load(i, 0);
    }

#ifdef RT_USING_ADC
    s_adc = (rt_adc_device_t)rt_device_find(cfg->adc_device_name);
    if (s_adc != RT_NULL)
    {
        if (rt_adc_enable(s_adc, cfg->input_adc_channel) == RT_EOK &&
            rt_adc_enable(s_adc, cfg->battery_adc_channel) == RT_EOK)
        {
            s_status.adc_ready = 1;
        }
    }
#endif

    if (ups_ina226_init(&s_ina226,
                        cfg->i2c_bus_name,
                        cfg->ina226_addr,
                        cfg->ina226_shunt_ohm,
                        cfg->ina226_max_current_a) == RT_EOK)
    {
        s_status.ina226_ready = 1;
    }

    s_last_energy_tick = rt_tick_get();

    rt_kprintf("ups: service ready adc=%u ina226=%u oled=removed\r\n", s_status.adc_ready, s_status.ina226_ready);
    return RT_EOK;
}

void ups_service_step(void)
{
    const ups_config_t *cfg = ups_get_config();
    float vin;
    float vbat;
    float vout = 0.0f;
    float iout = 0.0f;
    float pout = 0.0f;
    rt_uint8_t input_present;
    rt_tick_t now;
    float elapsed_h;
    rt_uint8_t key_level;

    vin = ups_adc_voltage(cfg, cfg->input_adc_channel, cfg->input_voltage_div_ratio);
    vbat = ups_adc_voltage(cfg, cfg->battery_adc_channel, cfg->battery_voltage_div_ratio);

    if (s_status.ina226_ready)
    {
        (void)ups_ina226_read_bus_voltage(&s_ina226, &vout);
        (void)ups_ina226_read_current(&s_ina226, &iout);
        (void)ups_ina226_read_power(&s_ina226, &pout);
    }

    if (s_status.power_fail_active)
    {
        input_present = (vin >= cfg->input_power_recover_threshold_v) ? 1 : 0;
    }
    else
    {
        input_present = (vin >= cfg->input_power_fail_threshold_v) ? 1 : 0;
    }

    ups_lock();
    if (!input_present && !s_status.power_fail_active)
    {
        s_status.power_fail_active = 1;
        rt_kprintf("ups: input power lost vin=%d.%02dV\r\n",
                   (int)vin, (int)((vin - (int)vin) * 100.0f));
    }
    else if (input_present && s_status.power_fail_active)
    {
        s_status.power_fail_active = 0;
        s_status.power_fail_count++;
        rt_kprintf("ups: input power recovered vin=%d.%02dV count=%u\r\n",
                   (int)vin, (int)((vin - (int)vin) * 100.0f), s_status.power_fail_count);
    }

    now = rt_tick_get();
    elapsed_h = (float)(now - s_last_energy_tick) / (float)RT_TICK_PER_SECOND / 3600.0f;
    s_last_energy_tick = now;

    s_status.input_voltage_v = vin;
    s_status.battery_voltage_v = vbat;
    s_status.output_voltage_v = vout;
    s_status.output_current_a = iout;
    s_status.output_power_w = pout;
    s_status.output_energy_wh += pout * elapsed_h;
    s_status.input_present = input_present;
    s_status.uptime_s = rt_tick_get() / RT_TICK_PER_SECOND;
    ups_unlock();

    rt_pin_write(UPS_PIN_STATUS_LED, input_present ? PIN_HIGH : PIN_LOW);

    key_level = rt_pin_read(UPS_PIN_USER_KEY);
    if (s_last_key_level == PIN_HIGH && key_level == PIN_LOW)
    {
        ups_service_toggle_page();
    }
    s_last_key_level = key_level;
}

void ups_service_get_status(ups_status_t *status)
{
    if (status == RT_NULL)
    {
        return;
    }

    ups_lock();
    *status = s_status;
    ups_unlock();
}

void ups_service_set_load(rt_uint8_t index, rt_uint8_t on)
{
    if (index >= 3)
    {
        return;
    }

    ups_lock();
    ups_apply_load(index, on);
    ups_unlock();
}

void ups_service_toggle_page(void)
{
    rt_kprintf("ups: oled removed, no display page to toggle\r\n");
}

void ups_service_safe_outputs(void)
{
    rt_uint8_t i;

    ups_lock();
    for (i = 0; i < 3; i++)
    {
        ups_apply_load(i, 0);
    }
    ups_unlock();
}
