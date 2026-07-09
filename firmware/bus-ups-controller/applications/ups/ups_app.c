#include <rtthread.h>

#include "ups_config.h"
#include "ups_service.h"

#define UPS_THREAD_STACK_SIZE 2048
#define UPS_THREAD_PRIORITY   18
#define UPS_THREAD_TICK       10

static void ups_thread_entry(void *parameter)
{
    const ups_config_t *cfg = ups_get_config();

    RT_UNUSED(parameter);

    while (1)
    {
        ups_service_step();
        rt_thread_mdelay(cfg->sample_interval_ms);
    }
}

static int ups_app_init(void)
{
    rt_thread_t thread;

    if (ups_service_init() != RT_EOK)
    {
        rt_kprintf("ups: service init failed\r\n");
        return -RT_ERROR;
    }

    thread = rt_thread_create("ups",
                              ups_thread_entry,
                              RT_NULL,
                              UPS_THREAD_STACK_SIZE,
                              UPS_THREAD_PRIORITY,
                              UPS_THREAD_TICK);
    if (thread == RT_NULL)
    {
        rt_kprintf("ups: failed to create thread\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}
INIT_APP_EXPORT(ups_app_init);
