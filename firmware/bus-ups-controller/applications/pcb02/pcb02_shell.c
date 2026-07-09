#include <rtthread.h>

#include <stdlib.h>
#include <string.h>

#include "pcb02_service.h"

static void pcb02_print_float(const char *name, float value, const char *unit)
{
    int whole = (int)value;
    int milli = (int)((value - (float)whole) * 1000.0f);

    if (milli < 0)
    {
        milli = -milli;
    }

    rt_kprintf("%s=%d.%03d%s ", name, whole, milli, unit);
}

static void pcb02_shell_print_help(void)
{
    rt_kprintf("pcb02 commands:\r\n");
    rt_kprintf("  pcb02 status\r\n");
    rt_kprintf("  pcb02 enable <0|1>\r\n");
    rt_kprintf("  pcb02 mode <mppt|cv>\r\n");
    rt_kprintf("  pcb02 output <charger|supply>\r\n");
    rt_kprintf("  pcb02 limits <vbat_max> <ichg_max>\r\n");
    rt_kprintf("  pcb02 clear_fault\r\n");
    rt_kprintf("  pcb02 safe\r\n");
}

static void pcb02_shell_print_status(void)
{
    pcb02_status_t status;

    pcb02_service_get_status(&status);

    rt_kprintf("pcb02: sw=%u chg_en=%u sensors=%u fault=%u fan=%u mppt=%u output=%u pwm=%u buck=%u bypass=%u err=%u\r\n",
               status.software_enable,
               status.hardware_chg_en,
               status.sensor_ready,
               status.fault_active,
               status.fan_enable,
               status.cfg.mppt_mode,
               status.cfg.output_mode,
               status.ctrl.pwm_counts,
               status.ctrl.buck_enable,
               status.ctrl.bypass_enable,
               status.faults.error_count);

    pcb02_print_float("vin", status.meas.voltage_input, "V");
    pcb02_print_float("vout", status.meas.voltage_output, "V");
    pcb02_print_float("iin", status.meas.current_input, "A");
    pcb02_print_float("iout", status.meas.current_output, "A");
    pcb02_print_float("pin", status.meas.power_input, "W");
    pcb02_print_float("temp", status.meas.temperature_c, "C");
    pcb02_print_float("wh", status.ctrl.watt_hours, "Wh");
    rt_kprintf("\r\n");

    rt_kprintf("fault bits: OT=%u IIN_OC=%u IOUT_OC=%u OOV=%u FLV=%u IUV=%u BNC=%u REC=%u\r\n",
               status.faults.over_temperature,
               status.faults.input_over_current,
               status.faults.output_over_current,
               status.faults.output_over_voltage,
               status.faults.fatal_low_voltage,
               status.faults.input_under_voltage,
               status.faults.battery_not_connected,
               status.faults.recovery_requested);
}

static void pcb02(int argc, char **argv)
{
    if (argc < 2)
    {
        pcb02_shell_print_help();
        return;
    }

    if (strcmp(argv[1], "status") == 0)
    {
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "enable") == 0)
    {
        if (argc < 3)
        {
            rt_kprintf("usage: pcb02 enable <0|1>\r\n");
            return;
        }
        pcb02_service_set_enable((rt_uint8_t)atoi(argv[2]));
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "mode") == 0)
    {
        if (argc < 3)
        {
            rt_kprintf("usage: pcb02 mode <mppt|cv>\r\n");
            return;
        }
        pcb02_service_set_mppt_mode((strcmp(argv[2], "mppt") == 0) ? 1 : 0);
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "output") == 0)
    {
        if (argc < 3)
        {
            rt_kprintf("usage: pcb02 output <charger|supply>\r\n");
            return;
        }
        pcb02_service_set_output_mode((strcmp(argv[2], "charger") == 0) ? 1 : 0);
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "limits") == 0)
    {
        if (argc < 4)
        {
            rt_kprintf("usage: pcb02 limits <vbat_max> <ichg_max>\r\n");
            return;
        }
        pcb02_service_set_charge_limits((float)atof(argv[2]), (float)atof(argv[3]));
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "clear_fault") == 0)
    {
        pcb02_service_clear_fault_latch();
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "safe") == 0)
    {
        pcb02_service_set_enable(0);
        pcb02_shell_print_status();
        return;
    }

    pcb02_shell_print_help();
}
MSH_CMD_EXPORT(pcb02, PCB02 MPPT service command);
