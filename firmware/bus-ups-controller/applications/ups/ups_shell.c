#include <rtthread.h>

#include <stdlib.h>
#include <string.h>

#include "ups_service.h"

static void print_float(const char *name, float value, const char *unit)
{
    int whole = (int)value;
    int cent = (int)((value - (float)whole) * 100.0f);

    if (cent < 0)
    {
        cent = -cent;
    }

    rt_kprintf("%s=%d.%02d%s ", name, whole, cent, unit);
}

static void ups_print_status(void)
{
    ups_status_t st;

    ups_service_get_status(&st);
    rt_kprintf("ups: input=%u fail=%u fail_count=%u uptime=%us adc=%u ina=%u load=%u%u%u\r\n",
               st.input_present,
               st.power_fail_active,
               st.power_fail_count,
               st.uptime_s,
               st.adc_ready,
               st.ina226_ready,
               st.load_on[0],
               st.load_on[1],
               st.load_on[2]);
    print_float("vin", st.input_voltage_v, "V");
    print_float("vbat", st.battery_voltage_v, "V");
    print_float("vout", st.output_voltage_v, "V");
    print_float("iout", st.output_current_a, "A");
    print_float("pout", st.output_power_w, "W");
    print_float("wh", st.output_energy_wh, "Wh");
    rt_kprintf("\r\n");
}

static void ups_help(void)
{
    rt_kprintf("ups commands:\r\n");
    rt_kprintf("  ups status\r\n");
    rt_kprintf("  ups load <1|2|3> <0|1>\r\n");
    rt_kprintf("  ups page\r\n");
    rt_kprintf("  ups safe\r\n");
}

static void ups(int argc, char **argv)
{
    if (argc < 2)
    {
        ups_help();
        return;
    }

    if (strcmp(argv[1], "status") == 0)
    {
        ups_print_status();
        return;
    }

    if (strcmp(argv[1], "load") == 0)
    {
        int index;
        int on;

        if (argc < 4)
        {
            rt_kprintf("usage: ups load <1|2|3> <0|1>\r\n");
            return;
        }

        index = atoi(argv[2]);
        on = atoi(argv[3]);
        if (index < 1 || index > 3)
        {
            rt_kprintf("load index must be 1..3\r\n");
            return;
        }

        ups_service_set_load((rt_uint8_t)(index - 1), on ? 1 : 0);
        ups_print_status();
        return;
    }

    if (strcmp(argv[1], "page") == 0)
    {
        ups_service_toggle_page();
        return;
    }

    if (strcmp(argv[1], "safe") == 0)
    {
        ups_service_safe_outputs();
        ups_print_status();
        return;
    }

    ups_help();
}
MSH_CMD_EXPORT(ups, UPS local monitor and load control command);
