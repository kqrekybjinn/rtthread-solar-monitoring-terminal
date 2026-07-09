#include <rtthread.h>

#include "pcb02_service.h"

#define PCB02_THREAD_STACK_SIZE 2048
#define PCB02_THREAD_PRIORITY   20
#define PCB02_THREAD_TICK       10
#define PCB02_STATUS_LOG_DIV    8

static void pcb02_print_float_inline(float value, const char *unit)
{
    int whole = (int)value;
    int cent = (int)((value - (float)whole) * 100.0f);

    if (cent < 0)
    {
        cent = -cent;
    }

    rt_kprintf("%d.%02d%s", whole, cent, unit);
}

static void pcb02_log_status(void)
{
    pcb02_status_t status;

    pcb02_service_get_status(&status);
    rt_kprintf("pcb02: sw=%u chg=%u sens=%u fault=%u pwm=%u vin=",
               status.software_enable,
               status.hardware_chg_en,
               status.sensor_ready,
               status.fault_active,
               status.ctrl.pwm_counts);
    pcb02_print_float_inline(status.meas.voltage_input, "V");
    rt_kprintf(" vout=");
    pcb02_print_float_inline(status.meas.voltage_output, "V");
    rt_kprintf(" iout=");
    pcb02_print_float_inline(status.meas.current_output, "A");
    rt_kprintf(" temp=");
    pcb02_print_float_inline(status.meas.temperature_c, "C");
    rt_kprintf("\r\n");
}

static void pcb02_thread_entry(void *parameter)
{
    const pcb02_config_t *cfg = pcb02_service_get_config();
    rt_uint32_t log_div = 0;

    RT_UNUSED(parameter);

    while (1)
    {
        (void)pcb02_service_step();

        if (++log_div >= PCB02_STATUS_LOG_DIV)
        {
            log_div = 0;
            pcb02_log_status();
        }

        rt_thread_mdelay(cfg->routine_interval_ms);
    }
}

static int pcb02_app_init(void)
{
    rt_thread_t thread;

    if (pcb02_service_init() != RT_EOK)
    {
        rt_kprintf("pcb02: service init failed\r\n");
        return -RT_ERROR;
    }

    thread = rt_thread_create("pcb02",
                              pcb02_thread_entry,
                              RT_NULL,
                              PCB02_THREAD_STACK_SIZE,
                              PCB02_THREAD_PRIORITY,
                              PCB02_THREAD_TICK);
    if (thread == RT_NULL)
    {
        rt_kprintf("pcb02: failed to create thread\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}
INIT_APP_EXPORT(pcb02_app_init);
