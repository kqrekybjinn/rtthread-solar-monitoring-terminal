#include <rtthread.h>
#include <rtdevice.h>
#ifdef RT_USING_PWM
#include <drivers/dev_pwm.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "pcb02_service.h"

static int s_active_test_id;

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

static void pcb02_print_csv_float(float value)
{
    int whole = (int)value;
    int milli = (int)((value - (float)whole) * 1000.0f);

    if (milli < 0)
    {
        milli = -milli;
    }

    if (value < 0.0f && whole == 0)
    {
        rt_kprintf("-");
    }
    rt_kprintf("%d.%03d", whole, milli);
}

static void pcb02_shell_print_help(void)
{
    rt_kprintf("pcb02 commands:\r\n");
    rt_kprintf("  pcb02 status\r\n");
    rt_kprintf("  pcb02 log <interval_ms> <count>\r\n");
    rt_kprintf("  pcb02 test <list|T1..T16>\r\n");
    rt_kprintf("  pcb02 testlog <T2..T12|T14|T16> <interval_ms> <count>\r\n");
    rt_kprintf("  pcb02 enable <0|1>\r\n");
    rt_kprintf("  pcb02 mode <mppt|cv>\r\n");
    rt_kprintf("  pcb02 output <charger|supply>\r\n");
    rt_kprintf("  pcb02 limits <vbat_max> <ichg_max>\r\n");
    rt_kprintf("  pcb02 fan <0|1|auto>\r\n");
    rt_kprintf("  pcb02 pwmtest <0..300 permille>\r\n");
    rt_kprintf("  pcb02 cvtest <target_v> <iout_limit> <0..300 max_permille>\r\n");
    rt_kprintf("  pcb02 cvpid <target_v> <iout_limit> <0..970 max_permille> <kp> <ki_per_s> [slew_v_s=0] [ff_scale=0.80]\r\n");
    rt_kprintf("  pcb02 cvctrl <fastpi|pi|inc|fuzzy|adapt> <target_v> <iout_limit> <max_permille> <kp> <ki_per_s> [kd] [slew_v_s=0]\r\n");
    rt_kprintf("  pcb02 chgtest <vbat_cv> <ichg_limit> <vin_min> <max_permille>\r\n");
    rt_kprintf("  pcb02 mppttest <po|inccond|vspo|adapt> <vout_max> <iout_limit> <max_permille> [step_counts]\r\n");
    rt_kprintf("  pcb02 clear_fault\r\n");
    rt_kprintf("  pcb02 safe\r\n");
}

static const char *pcb02_algo_name(rt_uint8_t algo)
{
    switch (algo)
    {
    case 1:
        return "inc";
    case 2:
        return "fuzzy";
    case 3:
        return "adapt";
    case 4:
        return "fastpi";
    default:
        return "pi";
    }
}

static rt_uint8_t pcb02_algo_id(const char *name)
{
    if (strcmp(name, "inc") == 0)
    {
        return 1;
    }
    if (strcmp(name, "fuzzy") == 0)
    {
        return 2;
    }
    if (strcmp(name, "adapt") == 0)
    {
        return 3;
    }
    if (strcmp(name, "fastpi") == 0)
    {
        return 4;
    }
    return 0;
}

static const char *pcb02_mppt_algo_name(rt_uint8_t algo)
{
    switch (algo)
    {
    case 1:
        return "inccond";
    case 2:
        return "vspo";
    case 3:
        return "adapt";
    default:
        return "po";
    }
}

static const char *pcb02_chg_state_name(rt_uint8_t state)
{
    switch (state)
    {
    case 1:
        return "pv_low";
    case 2:
        return "cc";
    case 3:
        return "cv";
    case 4:
        return "fault";
    default:
        return "idle";
    }
}

static rt_uint8_t pcb02_mppt_algo_id(const char *name)
{
    if (strcmp(name, "inccond") == 0)
    {
        return 1;
    }
    if (strcmp(name, "vspo") == 0)
    {
        return 2;
    }
    if (strcmp(name, "adapt") == 0)
    {
        return 3;
    }
    return 0;
}

static void pcb02_shell_print_status(void)
{
    pcb02_status_t status;

    pcb02_service_get_status(&status);

    rt_kprintf("pcb02: sw=%u chg_en=%u sensors=%u fault=%u fan=%u dbg=%u cv=%u pid=%u chg=%u mppttest=%u mppt=%u output=%u pwm=%u buck=%u bypass=%u err=%u\r\n",
               status.software_enable,
               status.hardware_chg_en,
               status.sensor_ready,
               status.fault_active,
               status.fan_enable,
               status.debug_pwm_active,
               status.cvtest_active,
               status.cvpid_active,
               status.chgtest_active,
               status.mppttest_active,
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
    if (status.cvtest_active || status.cvpid_active)
    {
        pcb02_print_float("cv_target", status.cvtest_target_v, "V");
        pcb02_print_float("cv_iout_limit", status.cvtest_current_limit_a, "A");
        rt_kprintf("cv_pwm_max=%u", status.cvtest_pwm_max_counts);
        if (status.cvpid_active)
        {
            rt_kprintf(" ");
            rt_kprintf("algo=%s ", pcb02_algo_name(status.cvpid_algo));
            pcb02_print_float("kp", status.cvpid_kp, "");
            pcb02_print_float("ki", status.cvpid_ki, "");
            pcb02_print_float("kd", status.cvpid_kd, "");
            pcb02_print_float("vref", status.cvpid_vref, "V");
            pcb02_print_float("slew", status.cvpid_slew_v_s, "V/s");
            pcb02_print_float("ff_scale", status.cvpid_ff_scale, "");
            pcb02_print_float("integ", status.cvpid_integral, "");
            pcb02_print_float("u", status.cvpid_command_counts, "");
            pcb02_print_float("e_prev", status.cvpid_prev_error, "V");
            pcb02_print_float("e_prev2", status.cvpid_prev2_error, "V");
            rt_kprintf("ff=%u", status.cvpid_feedforward_counts);
        }
        rt_kprintf("\r\n");
    }
    if (status.mppttest_active)
    {
        rt_kprintf("mppt_algo=%s ", pcb02_mppt_algo_name(status.mppttest_algo));
        pcb02_print_float("mppt_vout_max", status.mppttest_vout_max, "V");
        pcb02_print_float("mppt_iout_limit", status.mppttest_iout_limit, "A");
        rt_kprintf("mppt_pwm_max=%u mppt_step=%u\r\n",
                   status.mppttest_pwm_max_counts,
                   status.mppttest_step_counts);
    }
    if (status.chgtest_active)
    {
        rt_kprintf("chg_state=%s ", pcb02_chg_state_name(status.chgtest_state));
        pcb02_print_float("chg_vbat_cv", status.chgtest_vbat_cv, "V");
        pcb02_print_float("chg_ichg_limit", status.chgtest_ichg_limit, "A");
        pcb02_print_float("chg_vin_min", status.chgtest_vin_min, "V");
        pcb02_print_float("chg_vref", status.chgtest_vref, "V");
        pcb02_print_float("chg_u", status.chgtest_command_counts, "");
        pcb02_print_float("chg_i", status.chgtest_integral, "");
        rt_kprintf("chg_pwm_max=%u chg_ff=%u\r\n",
                   status.chgtest_pwm_max_counts,
                   status.chgtest_feedforward_counts);
    }

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

static const char *pcb02_status_mode_name(const pcb02_status_t *status)
{
    if (status->debug_pwm_active)
    {
        return "pwmtest";
    }
    if (status->cvpid_active)
    {
        return pcb02_algo_name(status->cvpid_algo);
    }
    if (status->cvtest_active)
    {
        return "cvtest";
    }
    if (status->mppttest_active)
    {
        return pcb02_mppt_algo_name(status->mppttest_algo);
    }
    if (status->chgtest_active)
    {
        return "chgtest";
    }
    if (status->software_enable)
    {
        return status->cfg.mppt_mode ? "run_mppt" : "run_cv";
    }
    return "safe";
}

static void pcb02_shell_print_log_row(rt_uint32_t sample_index, rt_uint32_t elapsed_ms)
{
    pcb02_status_t status;
    float target_v;
    float limit_a;
    rt_uint16_t pwm_max;

    pcb02_service_get_status(&status);
    target_v = status.cvtest_target_v;
    limit_a = status.cvtest_current_limit_a;
    pwm_max = status.cvtest_pwm_max_counts;
    if (status.chgtest_active)
    {
        target_v = status.chgtest_vbat_cv;
        limit_a = status.chgtest_ichg_limit;
        pwm_max = status.chgtest_pwm_max_counts;
    }
    else if (status.mppttest_active)
    {
        target_v = status.mppttest_vout_max;
        limit_a = status.mppttest_iout_limit;
        pwm_max = status.mppttest_pwm_max_counts;
    }

    rt_kprintf("%u,%u,T%d,%s,%u,%u,%u,%u,%u,",
               sample_index,
               elapsed_ms,
               s_active_test_id,
               pcb02_status_mode_name(&status),
               status.ctrl.pwm_counts,
               status.ctrl.buck_enable,
               status.ctrl.bypass_enable,
               status.fault_active,
               status.faults.error_count);
    pcb02_print_csv_float(status.meas.voltage_input);
    rt_kprintf(",");
    pcb02_print_csv_float(status.meas.current_input);
    rt_kprintf(",");
    pcb02_print_csv_float(status.meas.power_input);
    rt_kprintf(",");
    pcb02_print_csv_float(status.meas.voltage_output);
    rt_kprintf(",");
    pcb02_print_csv_float(status.meas.current_output);
    rt_kprintf(",");
    pcb02_print_csv_float(status.meas.power_output);
    rt_kprintf(",");
    pcb02_print_csv_float(status.meas.temperature_c);
    rt_kprintf(",");
    pcb02_print_csv_float(target_v);
    rt_kprintf(",");
    pcb02_print_csv_float(limit_a);
    rt_kprintf(",");
    pcb02_print_csv_float(status.chgtest_vref);
    rt_kprintf(",");
    pcb02_print_csv_float(status.chgtest_command_counts);
    rt_kprintf(",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
                pwm_max,
                status.chgtest_feedforward_counts,
                status.faults.over_temperature,
                status.faults.input_over_current,
                status.faults.output_over_current,
                status.faults.output_over_voltage,
                status.faults.fatal_low_voltage,
                status.faults.input_under_voltage,
                status.faults.battery_not_connected,
                status.faults.recovery_requested);
}

static void pcb02_shell_print_test_list(void)
{
    rt_kprintf("pcb02 test presets:\r\n");
    rt_kprintf("  T1  open-loop sweep: run pwmtest points manually\r\n");
    rt_kprintf("  T2  fast PI CV:   pcb02 cvctrl fastpi 3.3 0.10 300 35 18 0 0\r\n");
    rt_kprintf("  T3  incremental:  pcb02 cvctrl inc 3.3 0.10 300 35 8 0 0\r\n");
    rt_kprintf("  T4  fuzzy PI:     pcb02 cvctrl fuzzy 3.3 0.10 300 35 14 0 0\r\n");
    rt_kprintf("  T5  adaptive PI:  pcb02 cvctrl adapt 3.3 0.10 300 35 18 0 0\r\n");
    rt_kprintf("  T6  current limit: pcb02 cvctrl pi 3.3 0.05 300 35 18 0 0\r\n");
    rt_kprintf("  T7  load step: start T2, then switch load manually\r\n");
    rt_kprintf("  T8  MPPT PO:      pcb02 mppttest po 3.3 0.10 300 2\r\n");
    rt_kprintf("  T9  MPPT IncCond: pcb02 mppttest inccond 3.3 0.10 300 2\r\n");
    rt_kprintf("  T10 MPPT VSPO:    pcb02 mppttest vspo 3.3 0.10 300 2\r\n");
    rt_kprintf("  T11 MPPT adapt:   pcb02 mppttest adapt 3.3 0.10 300 2\r\n");
    rt_kprintf("  T12 MPPT limit:   pcb02 mppttest vspo 3.3 0.05 300 2\r\n");
    rt_kprintf("  T13 sensor link:  run pcb02_i2c_scan i2c3 3, then pcb02_ina_probe\r\n");
    rt_kprintf("  T14 safe/en:      pcb02 pwmtest 100, then pcb02 safe\r\n");
    rt_kprintf("  T15 serial log:   pcb02 log 500 20\r\n");
    rt_kprintf("  T16 fault record: pcb02 cvctrl pi 3.3 0.03 300 35 18 0 0\r\n");
}

static int pcb02_shell_test_number(const char *arg)
{
    if (arg == RT_NULL)
    {
        return -1;
    }
    if (arg[0] == 'T' || arg[0] == 't')
    {
        arg++;
    }
    return atoi(arg);
}

static rt_uint8_t pcb02_shell_start_test_quiet(int test_no)
{
    switch (test_no)
    {
    case 2:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(4, 3.3f, 0.10f, 300, 35.0f, 18.0f, 0.0f, 0.0f, 0.80f);
        return 1;
    case 3:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(1, 3.3f, 0.10f, 300, 35.0f, 8.0f, 0.0f, 0.0f, 0.80f);
        return 1;
    case 4:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(2, 3.3f, 0.10f, 300, 35.0f, 14.0f, 0.0f, 0.0f, 0.80f);
        return 1;
    case 5:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(3, 3.3f, 0.10f, 300, 35.0f, 18.0f, 0.0f, 0.0f, 0.80f);
        return 1;
    case 6:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(0, 3.3f, 0.05f, 300, 35.0f, 18.0f, 0.0f, 0.0f, 0.80f);
        return 1;
    case 7:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(4, 3.3f, 0.10f, 300, 35.0f, 18.0f, 0.0f, 0.0f, 0.80f);
        return 1;
    case 8:
        s_active_test_id = test_no;
        pcb02_service_mppttest(0, 3.3f, 0.10f, 300, 2);
        return 1;
    case 9:
        s_active_test_id = test_no;
        pcb02_service_mppttest(1, 3.3f, 0.10f, 300, 2);
        return 1;
    case 10:
        s_active_test_id = test_no;
        pcb02_service_mppttest(2, 3.3f, 0.10f, 300, 2);
        return 1;
    case 11:
        s_active_test_id = test_no;
        pcb02_service_mppttest(3, 3.3f, 0.10f, 300, 2);
        return 1;
    case 12:
        s_active_test_id = test_no;
        pcb02_service_mppttest(2, 3.3f, 0.05f, 300, 2);
        return 1;
    case 14:
        s_active_test_id = test_no;
        pcb02_service_debug_pwm(100);
        return 1;
    case 16:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(0, 3.3f, 0.03f, 300, 35.0f, 18.0f, 0.0f, 0.0f, 0.80f);
        return 1;
    default:
        return 0;
    }
}

static void pcb02_shell_start_test(int test_no)
{
    switch (test_no)
    {
    case 1:
        s_active_test_id = test_no;
        rt_kprintf("T1 open-loop sweep: use pcb02 pwmtest 50/80/100/120/150/180/220/260/300, log each point.\r\n");
        break;
    case 2:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(4, 3.3f, 0.10f, 300, 35.0f, 18.0f, 0.0f, 0.0f, 0.80f);
        rt_kprintf("T2 fast PI CV started; suggested log: pcb02 log 200 40\r\n");
        pcb02_shell_print_status();
        break;
    case 3:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(1, 3.3f, 0.10f, 300, 35.0f, 8.0f, 0.0f, 0.0f, 0.80f);
        rt_kprintf("T3 incremental PID started; suggested log: pcb02 log 500 40\r\n");
        pcb02_shell_print_status();
        break;
    case 4:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(2, 3.3f, 0.10f, 300, 35.0f, 14.0f, 0.0f, 0.0f, 0.80f);
        rt_kprintf("T4 fuzzy PI started; suggested log: pcb02 log 500 40\r\n");
        pcb02_shell_print_status();
        break;
    case 5:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(3, 3.3f, 0.10f, 300, 35.0f, 18.0f, 0.0f, 0.0f, 0.80f);
        rt_kprintf("T5 adaptive feedforward PI started; suggested log: pcb02 log 500 40\r\n");
        pcb02_shell_print_status();
        break;
    case 6:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(0, 3.3f, 0.05f, 300, 35.0f, 18.0f, 0.0f, 0.0f, 0.80f);
        rt_kprintf("T6 current-limit CV started; suggested log: pcb02 log 500 40\r\n");
        pcb02_shell_print_status();
        break;
    case 7:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(4, 3.3f, 0.10f, 300, 35.0f, 18.0f, 0.0f, 0.0f, 0.80f);
        rt_kprintf("T7 load-step base started; switch load manually after output settles, then run pcb02 log 200 80.\r\n");
        pcb02_shell_print_status();
        break;
    case 8:
        s_active_test_id = test_no;
        pcb02_service_mppttest(0, 3.3f, 0.10f, 300, 2);
        rt_kprintf("T8 MPPT perturb-observe started; suggested log: pcb02 log 500 80\r\n");
        pcb02_shell_print_status();
        break;
    case 9:
        s_active_test_id = test_no;
        pcb02_service_mppttest(1, 3.3f, 0.10f, 300, 2);
        rt_kprintf("T9 MPPT incremental-conductance started; suggested log: pcb02 log 500 80\r\n");
        pcb02_shell_print_status();
        break;
    case 10:
        s_active_test_id = test_no;
        pcb02_service_mppttest(2, 3.3f, 0.10f, 300, 2);
        rt_kprintf("T10 MPPT variable-step P&O started; suggested log: pcb02 log 500 80\r\n");
        pcb02_shell_print_status();
        break;
    case 11:
        s_active_test_id = test_no;
        pcb02_service_mppttest(3, 3.3f, 0.10f, 300, 2);
        rt_kprintf("T11 MPPT adaptive perturbation started; suggested log: pcb02 log 500 80\r\n");
        pcb02_shell_print_status();
        break;
    case 12:
        s_active_test_id = test_no;
        pcb02_service_mppttest(2, 3.3f, 0.05f, 300, 2);
        rt_kprintf("T12 MPPT output limit test started; suggested log: pcb02 log 500 80\r\n");
        pcb02_shell_print_status();
        break;
    case 13:
        s_active_test_id = test_no;
        rt_kprintf("T13 sensor link: run pcb02_i2c_scan i2c3 3, then pcb02_ina_probe.\r\n");
        break;
    case 14:
        s_active_test_id = test_no;
        pcb02_service_debug_pwm(100);
        rt_kprintf("T14 safe/en stage 1 started at pwmtest 100; verify PA5/PB12, then run pcb02 safe.\r\n");
        pcb02_shell_print_status();
        break;
    case 15:
        s_active_test_id = test_no;
        rt_kprintf("T15 serial log: run pcb02 log 500 20 during any active test.\r\n");
        break;
    case 16:
        s_active_test_id = test_no;
        pcb02_service_cvctrl(0, 3.3f, 0.03f, 300, 35.0f, 18.0f, 0.0f, 0.0f, 0.80f);
        rt_kprintf("T16 low current-limit fault/protection record started; suggested log: pcb02 log 500 40\r\n");
        pcb02_shell_print_status();
        break;
    default:
        rt_kprintf("unknown test preset; use pcb02 test list\r\n");
        break;
    }
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

    if (strcmp(argv[1], "log") == 0)
    {
        int interval_ms;
        int count;
        rt_tick_t start_tick;

        if (argc < 4)
        {
            rt_kprintf("usage: pcb02 log <interval_ms> <count>\r\n");
            return;
        }

        interval_ms = atoi(argv[2]);
        count = atoi(argv[3]);
        if (interval_ms < 50)
        {
            interval_ms = 50;
        }
        if (interval_ms > 10000)
        {
            interval_ms = 10000;
        }
        if (count < 1)
        {
            count = 1;
        }
        if (count > 1000)
        {
            count = 1000;
        }

        rt_kprintf("sample,time_ms,test_id,mode,pwm,buck,bypass,fault,err,vin_v,iin_a,pin_w,vout_v,iout_a,pout_w,temp_c,target_v,limit_a,chg_vref,chg_u,pwm_max,chg_ff,OT,IIN_OC,IOUT_OC,OOV,FLV,IUV,BNC,REC\r\n");
        start_tick = rt_tick_get();
        for (int i = 0; i < count; i++)
        {
            rt_uint32_t elapsed_ms = (rt_uint32_t)(((rt_tick_get() - start_tick) * 1000U) / RT_TICK_PER_SECOND);
            pcb02_shell_print_log_row((rt_uint32_t)i, elapsed_ms);
            if (i + 1 < count)
            {
                rt_thread_mdelay(interval_ms);
            }
        }
        return;
    }

    if (strcmp(argv[1], "test") == 0)
    {
        if (argc < 3 || strcmp(argv[2], "list") == 0)
        {
            pcb02_shell_print_test_list();
            return;
        }

        pcb02_shell_start_test(pcb02_shell_test_number(argv[2]));
        return;
    }

    if (strcmp(argv[1], "testlog") == 0)
    {
        int test_no;
        int interval_ms;
        int count;
        rt_tick_t start_tick;

        if (argc < 5)
        {
            rt_kprintf("usage: pcb02 testlog <T2..T12|T14|T16> <interval_ms> <count>\r\n");
            return;
        }

        test_no = pcb02_shell_test_number(argv[2]);
        interval_ms = atoi(argv[3]);
        count = atoi(argv[4]);
        if (!pcb02_shell_start_test_quiet(test_no))
        {
            rt_kprintf("testlog does not support T%d; use pcb02 test list\r\n", test_no);
            return;
        }
        if (interval_ms < 50)
        {
            interval_ms = 50;
        }
        if (interval_ms > 10000)
        {
            interval_ms = 10000;
        }
        if (count < 1)
        {
            count = 1;
        }
        if (count > 1000)
        {
            count = 1000;
        }

        rt_thread_mdelay(50);
        rt_kprintf("sample,time_ms,test_id,mode,pwm,buck,bypass,fault,err,vin_v,iin_a,pin_w,vout_v,iout_a,pout_w,temp_c,target_v,limit_a,chg_vref,chg_u,pwm_max,chg_ff,OT,IIN_OC,IOUT_OC,OOV,FLV,IUV,BNC,REC\r\n");
        start_tick = rt_tick_get();
        for (int i = 0; i < count; i++)
        {
            rt_uint32_t elapsed_ms = (rt_uint32_t)(((rt_tick_get() - start_tick) * 1000U) / RT_TICK_PER_SECOND);
            pcb02_shell_print_log_row((rt_uint32_t)i, elapsed_ms);
            if (i + 1 < count)
            {
                rt_thread_mdelay(interval_ms);
            }
        }
        return;
    }

    if (strcmp(argv[1], "enable") == 0)
    {
        if (argc < 3)
        {
            rt_kprintf("usage: pcb02 enable <0|1>\r\n");
            return;
        }
        s_active_test_id = 0;
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
        s_active_test_id = 0;
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
        s_active_test_id = 0;
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
        s_active_test_id = 0;
        pcb02_service_set_charge_limits((float)atof(argv[2]), (float)atof(argv[3]));
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "fan") == 0)
    {
        rt_int8_t fan_enable;

        if (argc < 3)
        {
            rt_kprintf("usage: pcb02 fan <0|1|auto>\r\n");
            return;
        }

        if (strcmp(argv[2], "auto") == 0)
        {
            fan_enable = -1;
        }
        else if (atoi(argv[2]) != 0)
        {
            fan_enable = 1;
        }
        else
        {
            fan_enable = 0;
        }

        s_active_test_id = 0;
        pcb02_service_fan_test(fan_enable);
        if (fan_enable < 0)
        {
            rt_kprintf("pcb02: fan returned to automatic temperature control\r\n");
        }
        else
        {
            rt_kprintf("pcb02: fan forced %s; use 'pcb02 fan auto' or 'pcb02 safe' to stop override\r\n",
                       fan_enable ? "on" : "off");
        }
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "pwmtest") == 0)
    {
        int permille;

        if (argc < 3)
        {
            rt_kprintf("usage: pcb02 pwmtest <0..300 permille>\r\n");
            return;
        }

        permille = atoi(argv[2]);
        if (permille < 0)
        {
            permille = 0;
        }
        if (permille > 300)
        {
            permille = 300;
        }

        s_active_test_id = 0;
        pcb02_service_debug_pwm((rt_uint16_t)permille);
        rt_kprintf("pcb02: debug pwm %d permille applied; use 'pcb02 safe' to stop\r\n", permille);
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "cvtest") == 0)
    {
        float target_v;
        float current_limit_a;
        int max_permille;

        if (argc < 5)
        {
            rt_kprintf("usage: pcb02 cvtest <target_v> <iout_limit> <0..300 max_permille>\r\n");
            return;
        }

        target_v = (float)atof(argv[2]);
        current_limit_a = (float)atof(argv[3]);
        max_permille = atoi(argv[4]);
        if (max_permille < 0)
        {
            max_permille = 0;
        }
        if (max_permille > 300)
        {
            max_permille = 300;
        }

        s_active_test_id = 0;
        pcb02_service_cvtest(target_v, current_limit_a, (rt_uint16_t)max_permille);
        rt_kprintf("pcb02: cvtest target=");
        pcb02_print_float("", target_v, "V");
        rt_kprintf("limit=");
        pcb02_print_float("", current_limit_a, "A");
        rt_kprintf("max=%d permille; use 'pcb02 safe' to stop\r\n", max_permille);
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "cvpid") == 0)
    {
        float target_v;
        float current_limit_a;
        float kp;
        float ki;
        float slew_v_s = 0.0f;
        float ff_scale = 0.80f;
        int max_permille;

        if (argc < 7)
        {
            rt_kprintf("usage: pcb02 cvpid <target_v> <iout_limit> <0..970 max_permille> <kp> <ki_per_s> [slew_v_s=0] [ff_scale=0.80]\r\n");
            return;
        }

        target_v = (float)atof(argv[2]);
        current_limit_a = (float)atof(argv[3]);
        max_permille = atoi(argv[4]);
        kp = (float)atof(argv[5]);
        ki = (float)atof(argv[6]);
        if (argc >= 8)
        {
            slew_v_s = (float)atof(argv[7]);
        }
        if (argc >= 9)
        {
            ff_scale = (float)atof(argv[8]);
        }
        if (max_permille < 0)
        {
            max_permille = 0;
        }
        if (max_permille > 970)
        {
            max_permille = 970;
        }

        s_active_test_id = 0;
        pcb02_service_cvpid(target_v, current_limit_a, (rt_uint16_t)max_permille, kp, ki, slew_v_s, ff_scale);
        rt_kprintf("pcb02: cvpid target=");
        pcb02_print_float("", target_v, "V");
        rt_kprintf("limit=");
        pcb02_print_float("", current_limit_a, "A");
        rt_kprintf("max=%d permille kp=", max_permille);
        pcb02_print_float("", kp, "");
        rt_kprintf("ki=");
        pcb02_print_float("", ki, "");
        rt_kprintf("slew=");
        pcb02_print_float("", slew_v_s, "V/s");
        rt_kprintf("ff_scale=");
        pcb02_print_float("", ff_scale, "");
        rt_kprintf("; use 'pcb02 safe' to stop\r\n");
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "cvctrl") == 0)
    {
        rt_uint8_t algo;
        float target_v;
        float current_limit_a;
        float kp;
        float ki;
        float kd = 0.0f;
        float slew_v_s = 0.0f;
        float ff_scale = 0.80f;
        int max_permille;

        if (argc < 8)
        {
            rt_kprintf("usage: pcb02 cvctrl <fastpi|pi|inc|fuzzy|adapt> <target_v> <iout_limit> <max_permille> <kp> <ki_per_s> [kd] [slew_v_s=0]\r\n");
            return;
        }

        algo = pcb02_algo_id(argv[2]);
        target_v = (float)atof(argv[3]);
        current_limit_a = (float)atof(argv[4]);
        max_permille = atoi(argv[5]);
        kp = (float)atof(argv[6]);
        ki = (float)atof(argv[7]);
        if (argc >= 9)
        {
            kd = (float)atof(argv[8]);
        }
        if (argc >= 10)
        {
            slew_v_s = (float)atof(argv[9]);
        }
        if (argc >= 11)
        {
            ff_scale = (float)atof(argv[10]);
        }
        if (max_permille < 0)
        {
            max_permille = 0;
        }
        if (max_permille > 970)
        {
            max_permille = 970;
        }

        s_active_test_id = 0;
        pcb02_service_cvctrl(algo,
                             target_v,
                             current_limit_a,
                             (rt_uint16_t)max_permille,
                             kp,
                             ki,
                             kd,
                             slew_v_s,
                             ff_scale);
        rt_kprintf("pcb02: cvctrl algo=%s target=", pcb02_algo_name(algo));
        pcb02_print_float("", target_v, "V");
        rt_kprintf("limit=");
        pcb02_print_float("", current_limit_a, "A");
        rt_kprintf("max=%d permille kp=", max_permille);
        pcb02_print_float("", kp, "");
        rt_kprintf("ki=");
        pcb02_print_float("", ki, "");
        rt_kprintf("kd=");
        pcb02_print_float("", kd, "");
        rt_kprintf("slew=");
        pcb02_print_float("", slew_v_s, "V/s");
        rt_kprintf("ff_scale=");
        pcb02_print_float("", ff_scale, "");
        rt_kprintf("; use 'pcb02 safe' to stop\r\n");
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "chgtest") == 0)
    {
        float vbat_cv;
        float current_limit_a;
        float vin_min;
        int max_permille;

        if (argc < 6)
        {
            rt_kprintf("usage: pcb02 chgtest <vbat_cv> <ichg_limit> <vin_min> <max_permille>\r\n");
            return;
        }

        vbat_cv = (float)atof(argv[2]);
        current_limit_a = (float)atof(argv[3]);
        vin_min = (float)atof(argv[4]);
        max_permille = atoi(argv[5]);
        if (max_permille < 0)
        {
            max_permille = 0;
        }
        if (max_permille > 970)
        {
            max_permille = 970;
        }

        s_active_test_id = 0;
        pcb02_service_chgtest(vbat_cv,
                              current_limit_a,
                              vin_min,
                              (rt_uint16_t)max_permille);
        rt_kprintf("pcb02: chgtest vbat_cv=");
        pcb02_print_float("", vbat_cv, "V");
        rt_kprintf("ichg_limit=");
        pcb02_print_float("", current_limit_a, "A");
        rt_kprintf("vin_min=");
        pcb02_print_float("", vin_min, "V");
        rt_kprintf("max=%d permille; use 'pcb02 safe' to stop\r\n", max_permille);
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "mppttest") == 0)
    {
        rt_uint8_t algo;
        float vout_max;
        float current_limit_a;
        int max_permille;
        int step_counts = 2;

        if (argc < 6)
        {
            rt_kprintf("usage: pcb02 mppttest <po|inccond|vspo|adapt> <vout_max> <iout_limit> <max_permille> [step_counts]\r\n");
            return;
        }

        algo = pcb02_mppt_algo_id(argv[2]);
        vout_max = (float)atof(argv[3]);
        current_limit_a = (float)atof(argv[4]);
        max_permille = atoi(argv[5]);
        if (argc >= 7)
        {
            step_counts = atoi(argv[6]);
        }
        if (max_permille < 0)
        {
            max_permille = 0;
        }
        if (max_permille > 970)
        {
            max_permille = 970;
        }
        if (step_counts < 0)
        {
            step_counts = 0;
        }
        if (step_counts > 64)
        {
            step_counts = 64;
        }

        s_active_test_id = 0;
        pcb02_service_mppttest(algo,
                               vout_max,
                               current_limit_a,
                               (rt_uint16_t)max_permille,
                               (rt_uint16_t)step_counts);
        rt_kprintf("pcb02: mppttest algo=%s vout_max=", pcb02_mppt_algo_name(algo));
        pcb02_print_float("", vout_max, "V");
        rt_kprintf("limit=");
        pcb02_print_float("", current_limit_a, "A");
        rt_kprintf("max=%d permille step=%d; use 'pcb02 safe' to stop\r\n",
                   max_permille,
                   step_counts);
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "clear_fault") == 0)
    {
        s_active_test_id = 0;
        pcb02_service_clear_fault_latch();
        pcb02_shell_print_status();
        return;
    }

    if (strcmp(argv[1], "safe") == 0)
    {
        s_active_test_id = 0;
        pcb02_service_set_enable(0);
        pcb02_shell_print_status();
        return;
    }

    pcb02_shell_print_help();
}
MSH_CMD_EXPORT(pcb02, PCB02 MPPT service command);

static void pcb02_i2c_scan(int argc, char **argv)
{
    const char *bus_name = "i2c3";
    int repeat = 1;
    struct rt_i2c_bus_device *bus;

    if (argc >= 2)
    {
        bus_name = argv[1];
    }
    if (argc >= 3)
    {
        repeat = atoi(argv[2]);
        if (repeat <= 0)
        {
            repeat = 1;
        }
    }

    bus = (struct rt_i2c_bus_device *)rt_device_find(bus_name);
    if (bus == RT_NULL)
    {
        rt_kprintf("i2c scan: bus %s not found\r\n", bus_name);
        return;
    }

    while (repeat-- > 0)
    {
        rt_uint8_t found = 0;

        rt_kprintf("i2c scan %s:", bus_name);
        for (rt_uint8_t addr = 0x03; addr <= 0x77; addr++)
        {
            struct rt_i2c_msg msg;
            rt_uint8_t data = 0x00;

            msg.addr = addr;
            msg.flags = RT_I2C_RD;
            msg.buf = &data;
            msg.len = 1;

            if (rt_i2c_transfer(bus, &msg, 1) == 1)
            {
                rt_kprintf(" 0x%02x", addr);
                found = 1;
            }
        }
        if (!found)
        {
            rt_kprintf(" none");
        }
        rt_kprintf("\r\n");
        rt_thread_mdelay(100);
    }
}
MSH_CMD_EXPORT(pcb02_i2c_scan, scan I2C bus: pcb02_i2c_scan [bus] [repeat]);

static rt_err_t pcb02_i2c_read_u16(struct rt_i2c_bus_device *bus,
                                   rt_uint8_t addr,
                                   rt_uint8_t reg,
                                   rt_uint16_t *value)
{
    rt_uint8_t data[2];
    struct rt_i2c_msg msgs[2];

    msgs[0].addr = addr;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = &reg;
    msgs[0].len = 1;

    msgs[1].addr = addr;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = data;
    msgs[1].len = sizeof(data);

    if (rt_i2c_transfer(bus, msgs, 2) != 2)
    {
        return -RT_ERROR;
    }

    *value = ((rt_uint16_t)data[0] << 8) | data[1];
    return RT_EOK;
}

static void pcb02_ina_probe(int argc, char **argv)
{
    const char *bus_name = "i2c3";
    struct rt_i2c_bus_device *bus;

    if (argc >= 2)
    {
        bus_name = argv[1];
    }

    bus = (struct rt_i2c_bus_device *)rt_device_find(bus_name);
    if (bus == RT_NULL)
    {
        rt_kprintf("ina probe: bus %s not found\r\n", bus_name);
        return;
    }

    for (rt_uint8_t addr = 0x40; addr <= 0x41; addr++)
    {
        rt_uint16_t config = 0;
        rt_uint16_t shunt_raw = 0;
        rt_uint16_t bus_raw = 0;
        rt_uint16_t power_raw = 0;
        rt_uint16_t current_raw = 0;
        rt_uint16_t mfg = 0;
        rt_uint16_t die = 0;
        rt_err_t ret_cfg = pcb02_i2c_read_u16(bus, addr, 0x00, &config);
        rt_err_t ret_shunt = pcb02_i2c_read_u16(bus, addr, 0x01, &shunt_raw);
        rt_err_t ret_bus = pcb02_i2c_read_u16(bus, addr, 0x02, &bus_raw);
        rt_err_t ret_power = pcb02_i2c_read_u16(bus, addr, 0x03, &power_raw);
        rt_err_t ret_current = pcb02_i2c_read_u16(bus, addr, 0x04, &current_raw);
        rt_err_t ret_mfg = pcb02_i2c_read_u16(bus, addr, 0xfe, &mfg);
        rt_err_t ret_die = pcb02_i2c_read_u16(bus, addr, 0xff, &die);

        rt_kprintf("ina 0x%02x: cfg=%s", addr, ret_cfg == RT_EOK ? "ok" : "fail");
        if (ret_cfg == RT_EOK)
        {
            rt_kprintf("(0x%04x)", config);
        }
        rt_kprintf(" bus=%s", ret_bus == RT_EOK ? "ok" : "fail");
        if (ret_bus == RT_EOK)
        {
            rt_kprintf("(raw=0x%04x %d.%03dV)",
                       bus_raw,
                       (int)((bus_raw * 125U) / 100000U),
                       (int)(((bus_raw * 125U) / 100U) % 1000U));
        }
        rt_kprintf(" shunt=%s", ret_shunt == RT_EOK ? "ok" : "fail");
        if (ret_shunt == RT_EOK)
        {
            rt_int32_t uv = (rt_int32_t)((rt_int16_t)shunt_raw) * 2500;
            rt_kprintf("(raw=0x%04x %s%d.%03dmV)",
                       shunt_raw,
                       uv < 0 ? "-" : "",
                       (int)((uv < 0 ? -uv : uv) / 1000000),
                       (int)((uv < 0 ? -uv : uv) / 1000 % 1000));
        }
        rt_kprintf(" current=%s", ret_current == RT_EOK ? "ok" : "fail");
        if (ret_current == RT_EOK)
        {
            rt_kprintf("(raw=0x%04x %d)", current_raw, (int)((rt_int16_t)current_raw));
        }
        rt_kprintf(" power=%s", ret_power == RT_EOK ? "ok" : "fail");
        if (ret_power == RT_EOK)
        {
            rt_kprintf("(raw=0x%04x)", power_raw);
        }
        rt_kprintf(" mfg=%s", ret_mfg == RT_EOK ? "ok" : "fail");
        if (ret_mfg == RT_EOK)
        {
            rt_kprintf("(0x%04x)", mfg);
        }
        rt_kprintf(" die=%s", ret_die == RT_EOK ? "ok" : "fail");
        if (ret_die == RT_EOK)
        {
            rt_kprintf("(0x%04x)", die);
        }
        rt_kprintf("\r\n");
    }
}
MSH_CMD_EXPORT(pcb02_ina_probe, probe INA226 registers: pcb02_ina_probe [bus]);

static void pcb02_i2c_pulse(int argc, char **argv)
{
    int repeat = 100;

    if (argc >= 2)
    {
        repeat = atoi(argv[1]);
        if (repeat <= 0)
        {
            repeat = 100;
        }
    }

    rt_pin_mode(GET_PIN(C, 8), PIN_MODE_OUTPUT_OD);
    rt_pin_mode(GET_PIN(C, 9), PIN_MODE_OUTPUT_OD);

    rt_kprintf("pulse PC8/PC9 %d times\r\n", repeat);
    while (repeat-- > 0)
    {
        rt_pin_write(GET_PIN(C, 8), PIN_LOW);
        rt_pin_write(GET_PIN(C, 9), PIN_HIGH);
        rt_thread_mdelay(5);
        rt_pin_write(GET_PIN(C, 8), PIN_HIGH);
        rt_pin_write(GET_PIN(C, 9), PIN_LOW);
        rt_thread_mdelay(5);
    }

    rt_pin_write(GET_PIN(C, 8), PIN_HIGH);
    rt_pin_write(GET_PIN(C, 9), PIN_HIGH);
}
MSH_CMD_EXPORT(pcb02_i2c_pulse, pulse PC8 SCL and PC9 SDA: pcb02_i2c_pulse [repeat]);

static void pcb02_pa8_pulse(int argc, char **argv)
{
    int repeat = 100;

    if (argc >= 2)
    {
        repeat = atoi(argv[1]);
        if (repeat <= 0)
        {
            repeat = 100;
        }
    }

    rt_kprintf("pulse PA8 %d times; disconnect/disable PV power stage before using this test\r\n", repeat);
    rt_pin_mode(GET_PIN(A, 8), PIN_MODE_OUTPUT);
    while (repeat-- > 0)
    {
        rt_pin_write(GET_PIN(A, 8), PIN_HIGH);
        rt_thread_mdelay(5);
        rt_pin_write(GET_PIN(A, 8), PIN_LOW);
        rt_thread_mdelay(5);
    }
}
MSH_CMD_EXPORT(pcb02_pa8_pulse, pulse PA8 as GPIO: pcb02_pa8_pulse [repeat]);

static void pcb02_pa8_set(int argc, char **argv)
{
    int level;

    if (argc < 2)
    {
        rt_kprintf("usage: pcb02_pa8_set <0|1>\r\n");
        return;
    }

    level = atoi(argv[1]) ? 1 : 0;
    rt_pin_mode(GET_PIN(A, 8), PIN_MODE_OUTPUT);
    rt_pin_write(GET_PIN(A, 8), level ? PIN_HIGH : PIN_LOW);
    rt_thread_mdelay(1);
    rt_kprintf("PA8 set %d, readback=%d\r\n", level, rt_pin_read(GET_PIN(A, 8)));
}
MSH_CMD_EXPORT(pcb02_pa8_set, set PA8 as GPIO: pcb02_pa8_set <0|1>);

static void pcb02_pa8_regs(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);

    rt_kprintf("PA8 regs: MODER=0x%08x OTYPER=0x%08x OSPEEDR=0x%08x PUPDR=0x%08x IDR=0x%08x ODR=0x%08x AFRH=0x%08x\r\n",
               GPIOA->MODER,
               GPIOA->OTYPER,
               GPIOA->OSPEEDR,
               GPIOA->PUPDR,
               GPIOA->IDR,
               GPIOA->ODR,
               GPIOA->AFR[1]);
    rt_kprintf("PA8 decoded: mode=%u otype=%u pupd=%u odr=%u idr=%u af=%u\r\n",
               (unsigned)((GPIOA->MODER >> 16) & 0x3U),
               (unsigned)((GPIOA->OTYPER >> 8) & 0x1U),
               (unsigned)((GPIOA->PUPDR >> 16) & 0x3U),
               (unsigned)((GPIOA->ODR >> 8) & 0x1U),
               (unsigned)((GPIOA->IDR >> 8) & 0x1U),
               (unsigned)((GPIOA->AFR[1] >> 0) & 0xfU));
}
MSH_CMD_EXPORT(pcb02_pa8_regs, dump PA8 GPIO registers);

static void pcb02_pa5_swpwm(int argc, char **argv)
{
    int seconds = 5;
    rt_tick_t end_tick;

    if (argc >= 2)
    {
        seconds = atoi(argv[1]);
        if (seconds <= 0)
        {
            seconds = 5;
        }
        if (seconds > 30)
        {
            seconds = 30;
        }
    }

    rt_kprintf("PA5 software PWM 1kHz 50%% for %d seconds\r\n", seconds);
    rt_pin_mode(GET_PIN(A, 5), PIN_MODE_OUTPUT);
    end_tick = rt_tick_get() + (rt_tick_t)seconds * RT_TICK_PER_SECOND;

    while ((rt_int32_t)(end_tick - rt_tick_get()) > 0)
    {
        rt_pin_write(GET_PIN(A, 5), PIN_HIGH);
        rt_hw_us_delay(500);
        rt_pin_write(GET_PIN(A, 5), PIN_LOW);
        rt_hw_us_delay(500);
    }
}
MSH_CMD_EXPORT(pcb02_pa5_swpwm, output software PWM on PA5: pcb02_pa5_swpwm [seconds]);

static void pcb02_pa5_hwpwm(int argc, char **argv)
{
#ifdef RT_USING_PWM
    struct rt_device_pwm *pwm_dev;
    int permille;
    rt_uint32_t period_ns = 1000000000UL / 39000UL;
    rt_uint32_t pulse_ns;
    rt_err_t ret;

    if (argc < 2)
    {
        rt_kprintf("usage: pcb02_pa5_hwpwm <0..970 permille>\r\n");
        return;
    }

    permille = atoi(argv[1]);
    if (permille < 0)
    {
        permille = 0;
    }
    if (permille > 970)
    {
        permille = 970;
    }

    pwm_dev = (struct rt_device_pwm *)rt_device_find("pwm2");
    if (pwm_dev == RT_NULL)
    {
        rt_kprintf("pwm2 not found\r\n");
        return;
    }

    pulse_ns = ((rt_uint64_t)period_ns * (rt_uint32_t)permille) / 1000U;
    ret = rt_pwm_set(pwm_dev, 1, period_ns, pulse_ns);
    if (ret != RT_EOK)
    {
        rt_kprintf("pwm2 ch1 set failed ret=%d\r\n", ret);
        return;
    }

    if (permille > 0)
    {
        ret = rt_pwm_enable(pwm_dev, 1);
    }
    else
    {
        ret = rt_pwm_disable(pwm_dev, 1);
    }

    rt_kprintf("PA5 pwm2 ch1 %d permille period=%u pulse=%u ret=%d\r\n",
               permille,
               period_ns,
               pulse_ns,
               ret);
#else
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    rt_kprintf("RT_USING_PWM disabled\r\n");
#endif
}
MSH_CMD_EXPORT(pcb02_pa5_hwpwm, output hardware PWM on PA5 TIM2_CH1: pcb02_pa5_hwpwm <permille>);
