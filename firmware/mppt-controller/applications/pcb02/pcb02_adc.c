#include "pcb02_adc.h"

#include <rtdevice.h>

#ifdef RT_USING_ADC
#include <drivers/adc.h>
#endif

typedef struct ntc_point
{
    float temperature_c;
    float resistance_ohm;
} ntc_point_t;

static const ntc_point_t s_ntc_10k_3950_table[] =
{
    {-20.0f, 105384.0f},
    {-10.0f, 58245.0f},
    {0.0f, 33620.0f},
    {10.0f, 20174.0f},
    {20.0f, 12535.0f},
    {25.0f, 10000.0f},
    {30.0f, 8037.0f},
    {40.0f, 5301.0f},
    {50.0f, 3588.0f},
    {60.0f, 2486.0f},
    {70.0f, 1759.0f},
    {80.0f, 1270.0f},
    {90.0f, 933.0f},
    {100.0f, 697.0f},
};

#ifdef RT_USING_ADC
static rt_adc_device_t s_ntc_adc;
#endif

static float pcb02_ntc_temperature_from_resistance(float resistance_ohm)
{
    rt_size_t i;
    const rt_size_t count = sizeof(s_ntc_10k_3950_table) / sizeof(s_ntc_10k_3950_table[0]);

    if (resistance_ohm >= s_ntc_10k_3950_table[0].resistance_ohm)
    {
        return s_ntc_10k_3950_table[0].temperature_c;
    }

    if (resistance_ohm <= s_ntc_10k_3950_table[count - 1].resistance_ohm)
    {
        return s_ntc_10k_3950_table[count - 1].temperature_c;
    }

    for (i = 1; i < count; i++)
    {
        const ntc_point_t *hot = &s_ntc_10k_3950_table[i];
        const ntc_point_t *cold = &s_ntc_10k_3950_table[i - 1];

        if (resistance_ohm <= cold->resistance_ohm && resistance_ohm >= hot->resistance_ohm)
        {
            float span_r = hot->resistance_ohm - cold->resistance_ohm;
            float ratio = (resistance_ohm - cold->resistance_ohm) / span_r;
            return cold->temperature_c + ratio * (hot->temperature_c - cold->temperature_c);
        }
    }

    return 25.0f;
}

rt_err_t pcb02_adc_init(const pcb02_config_t *cfg)
{
#ifdef RT_USING_ADC
    s_ntc_adc = (rt_adc_device_t)rt_device_find(cfg->ntc_adc_device_name);
    if (s_ntc_adc == RT_NULL)
    {
        rt_kprintf("pcb02: ADC device %s not found, temperature fallback enabled\r\n",
                   cfg->ntc_adc_device_name);
        return -RT_ERROR;
    }

    if (rt_adc_enable(s_ntc_adc, cfg->ntc_adc_channel) != RT_EOK)
    {
        rt_kprintf("pcb02: ADC %s channel %d enable failed\r\n",
                   cfg->ntc_adc_device_name, cfg->ntc_adc_channel);
        s_ntc_adc = RT_NULL;
        return -RT_ERROR;
    }

    rt_kprintf("pcb02: NTC ADC ready on %s ch%d\r\n",
               cfg->ntc_adc_device_name, cfg->ntc_adc_channel);
    return RT_EOK;
#else
    RT_UNUSED(cfg);
    return -RT_ENOSYS;
#endif
}

rt_err_t pcb02_adc_read_ntc_temperature(const pcb02_config_t *cfg, float *temperature_c)
{
#ifdef RT_USING_ADC
    rt_uint32_t raw;
    float adc_max;
    float voltage_mv;
    float resistance_ohm;

    if (temperature_c == RT_NULL || s_ntc_adc == RT_NULL)
    {
        return -RT_ERROR;
    }

    raw = rt_adc_read(s_ntc_adc, cfg->ntc_adc_channel);
    adc_max = 4095.0f;
    voltage_mv = ((float)raw * cfg->adc_reference_mv) / adc_max;

    if (voltage_mv <= 1.0f || voltage_mv >= (cfg->adc_reference_mv - 1.0f))
    {
        return -RT_ERROR;
    }

    resistance_ohm = cfg->ntc_pullup_ohm * voltage_mv / (cfg->adc_reference_mv - voltage_mv);
    *temperature_c = pcb02_ntc_temperature_from_resistance(resistance_ohm);
    return RT_EOK;
#else
    RT_UNUSED(cfg);
    RT_UNUSED(temperature_c);
    return -RT_ENOSYS;
#endif
}
