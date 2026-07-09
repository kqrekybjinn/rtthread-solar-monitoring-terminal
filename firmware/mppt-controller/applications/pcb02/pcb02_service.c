#include "pcb02_service.h"

#include <rtdevice.h>

#include "pcb02_hw.h"
#include "pcb02_sensors.h"

#define PCB02_SENSOR_FAIL_LIMIT 3
#define PCB02_CVPID_DEFAULT_SLEW_V_S 0.0f
#define PCB02_CVPID_DEFAULT_FF_SCALE 0.80f
#define PCB02_FASTPI_DEFAULT_FF_SCALE 0.35f
#define PCB02_FASTPI_PRELOAD_RATIO 0.70f
#define PCB02_CVPID_PROP_LIMIT_RATIO 0.20f
#define PCB02_FASTPI_PROP_LIMIT_RATIO 0.10f
#define PCB02_CVPID_ERR_NEAR_V 0.12f
#define PCB02_CVPID_ERR_MID_V 0.45f
#define PCB02_CVPID_ERR_FAR_V 1.20f
#define PCB02_CVPID_STEP_UP_NEAR 24.0f
#define PCB02_CVPID_STEP_UP_MID 60.0f
#define PCB02_CVPID_STEP_UP_FAR 140.0f
#define PCB02_CVPID_STEP_DOWN_NEAR 18.0f
#define PCB02_CVPID_STEP_DOWN_FAR 120.0f
#define PCB02_CV_TEST_OV_MARGIN_V 2.0f
#define PCB02_CV_TEST_OV_INSTANT_MARGIN_V 4.0f
#define PCB02_MPPT_TEST_OV_MARGIN_V 1.0f
#define PCB02_MPPT_TEST_OV_INSTANT_MARGIN_V 2.0f
#define PCB02_TEST_OV_CONFIRM_COUNT 3
#define PCB02_CV_ALGO_PI 0
#define PCB02_CV_ALGO_INC 1
#define PCB02_CV_ALGO_FUZZY 2
#define PCB02_CV_ALGO_ADAPT 3
#define PCB02_CV_ALGO_FASTPI 4
#define PCB02_MPPT_ALGO_PO 0
#define PCB02_MPPT_ALGO_INCCOND 1
#define PCB02_MPPT_ALGO_VSPO 2
#define PCB02_MPPT_ALGO_ADAPT 3
#define PCB02_CHG_STATE_IDLE 0
#define PCB02_CHG_STATE_PV_LOW 1
#define PCB02_CHG_STATE_CC 2
#define PCB02_CHG_STATE_CV 3
#define PCB02_CHG_STATE_FAULT 4
#define PCB02_CHG_NO_POWER_PWM_COUNTS 250U
#define PCB02_CHG_NO_POWER_TRIPS 40U
#define PCB02_CHG_VREF_SLEW_V_PER_S 20.0f

static pcb02_config_t s_cfg;
static pcb02_measurements_t s_meas;
static pcb02_faults_t s_faults;
static pcb02_control_state_t s_ctrl;
static struct rt_mutex s_lock;
static rt_uint8_t s_lock_ready;
static rt_uint8_t s_sensor_ready;
static rt_uint8_t s_sensor_fail_count;
static rt_uint8_t s_software_enable;
static rt_uint8_t s_hardware_chg_en;
static rt_uint8_t s_fault_active;
static rt_uint8_t s_fan_enable;
static rt_uint8_t s_fan_test_active;
static rt_uint8_t s_fan_test_enable;
static rt_uint8_t s_test_oov_count;
static rt_uint8_t s_debug_pwm_active;
static rt_uint8_t s_cvtest_active;
static rt_uint8_t s_cvpid_active;
static rt_uint8_t s_cvpid_algo;
static rt_uint8_t s_chgtest_active;
static rt_uint8_t s_chgtest_state;
static rt_uint8_t s_mppttest_active;
static rt_uint8_t s_mppttest_algo;
static float s_cvtest_target_v;
static float s_cvtest_current_limit_a;
static rt_uint16_t s_cvtest_pwm_max_counts;
static float s_chgtest_vbat_cv;
static float s_chgtest_ichg_limit;
static float s_chgtest_vin_min;
static rt_uint16_t s_chgtest_pwm_max_counts;
static float s_chgtest_command_counts;
static float s_chgtest_integral;
static float s_chgtest_vref;
static rt_uint16_t s_chgtest_feedforward_counts;
static rt_uint16_t s_chgtest_no_power_count;
static float s_mppttest_vout_max;
static float s_mppttest_iout_limit;
static rt_uint16_t s_mppttest_pwm_max_counts;
static rt_uint16_t s_mppttest_step_counts;
static float s_mppttest_prev_power;
static float s_mppttest_prev_vin;
static float s_mppttest_prev_iin;
static rt_int8_t s_mppttest_direction;
static float s_cvpid_kp;
static float s_cvpid_ki;
static float s_cvpid_kd;
static float s_cvpid_integral;
static float s_cvpid_vref;
static float s_cvpid_slew_v_s;
static float s_cvpid_ff_scale;
static float s_cvpid_prev_error;
static float s_cvpid_prev2_error;
static float s_cvpid_command_counts;
static rt_uint16_t s_cvpid_feedforward_counts;
static rt_uint32_t s_last_energy_tick;

static void pcb02_service_update_fan(void)
{
    if (s_fan_test_active)
    {
        s_fan_enable = s_fan_test_enable;
    }
    else
    {
        s_fan_enable = (s_cfg.enable_fan && s_meas.temperature_c >= s_cfg.temperature_fan_c) ? 1 : 0;
    }
}

static rt_uint8_t pcb02_service_test_output_over_voltage(float target_v,
                                                         float margin_v,
                                                         float instant_margin_v)
{
    if (s_meas.voltage_output > (target_v + instant_margin_v))
    {
        s_test_oov_count = PCB02_TEST_OV_CONFIRM_COUNT;
        return 1;
    }

    if (s_meas.voltage_output > (target_v + margin_v))
    {
        if (s_test_oov_count < PCB02_TEST_OV_CONFIRM_COUNT)
        {
            s_test_oov_count++;
        }
    }
    else
    {
        s_test_oov_count = 0;
    }

    return (s_test_oov_count >= PCB02_TEST_OV_CONFIRM_COUNT) ? 1 : 0;
}

static void pcb02_service_lock(void)
{
    if (s_lock_ready)
    {
        rt_mutex_take(&s_lock, RT_WAITING_FOREVER);
    }
}

static void pcb02_service_unlock(void)
{
    if (s_lock_ready)
    {
        rt_mutex_release(&s_lock);
    }
}

static void pcb02_service_apply_safe_state(void)
{
    s_debug_pwm_active = 0;
    s_cvtest_active = 0;
    s_cvpid_active = 0;
    s_chgtest_active = 0;
    s_chgtest_state = PCB02_CHG_STATE_IDLE;
    s_mppttest_active = 0;
    s_cvpid_algo = PCB02_CV_ALGO_PI;
    s_mppttest_algo = PCB02_MPPT_ALGO_PO;
    s_cvtest_target_v = 0.0f;
    s_cvtest_current_limit_a = 0.0f;
    s_cvtest_pwm_max_counts = 0;
    s_chgtest_vbat_cv = 0.0f;
    s_chgtest_ichg_limit = 0.0f;
    s_chgtest_vin_min = 0.0f;
    s_chgtest_pwm_max_counts = 0;
    s_chgtest_command_counts = 0.0f;
    s_chgtest_integral = 0.0f;
    s_chgtest_vref = 0.0f;
    s_chgtest_feedforward_counts = 0;
    s_chgtest_no_power_count = 0;
    s_chgtest_no_power_count = 0;
    s_mppttest_vout_max = 0.0f;
    s_mppttest_iout_limit = 0.0f;
    s_mppttest_pwm_max_counts = 0;
    s_mppttest_step_counts = 0;
    s_mppttest_prev_power = 0.0f;
    s_mppttest_prev_vin = 0.0f;
    s_mppttest_prev_iin = 0.0f;
    s_mppttest_direction = 1;
    s_cvpid_kp = 0.0f;
    s_cvpid_ki = 0.0f;
    s_cvpid_kd = 0.0f;
    s_cvpid_integral = 0.0f;
    s_cvpid_vref = 0.0f;
    s_cvpid_slew_v_s = 0.0f;
    s_cvpid_ff_scale = 0.0f;
    s_cvpid_prev_error = 0.0f;
    s_cvpid_prev2_error = 0.0f;
    s_cvpid_command_counts = 0.0f;
    s_cvpid_feedforward_counts = 0;
    s_ctrl.buck_enable = 0;
    s_ctrl.bypass_enable = 0;
    s_ctrl.pwm_counts = 0;
    s_fan_enable = 0;
    s_fan_test_active = 0;
    s_fan_test_enable = 0;
    s_test_oov_count = 0;
    pcb02_hw_safe_state();
}

rt_err_t pcb02_service_init(void)
{
    rt_err_t ret;

    s_cfg = *pcb02_get_config();
    rt_memset(&s_meas, 0, sizeof(s_meas));
    rt_memset(&s_faults, 0, sizeof(s_faults));
    pcb02_control_state_init(&s_ctrl);

    if (rt_mutex_init(&s_lock, "pcb02svc", RT_IPC_FLAG_PRIO) == RT_EOK)
    {
        s_lock_ready = 1;
    }

    s_software_enable = 0;
    s_hardware_chg_en = 0;
    s_fault_active = 0;
    s_fan_enable = 0;
    s_fan_test_active = 0;
    s_fan_test_enable = 0;
    s_test_oov_count = 0;
    s_debug_pwm_active = 0;
    s_cvtest_active = 0;
    s_cvpid_active = 0;
    s_chgtest_active = 0;
    s_chgtest_state = PCB02_CHG_STATE_IDLE;
    s_chgtest_no_power_count = 0;
    s_mppttest_active = 0;
    s_cvpid_algo = PCB02_CV_ALGO_PI;
    s_mppttest_algo = PCB02_MPPT_ALGO_PO;
    s_cvtest_target_v = 0.0f;
    s_cvtest_current_limit_a = 0.0f;
    s_cvtest_pwm_max_counts = 0;
    s_chgtest_vbat_cv = 0.0f;
    s_chgtest_ichg_limit = 0.0f;
    s_chgtest_vin_min = 0.0f;
    s_chgtest_pwm_max_counts = 0;
    s_chgtest_command_counts = 0.0f;
    s_chgtest_integral = 0.0f;
    s_chgtest_vref = 0.0f;
    s_chgtest_feedforward_counts = 0;
    s_chgtest_no_power_count = 0;
    s_mppttest_vout_max = 0.0f;
    s_mppttest_iout_limit = 0.0f;
    s_mppttest_pwm_max_counts = 0;
    s_mppttest_step_counts = 0;
    s_mppttest_prev_power = 0.0f;
    s_mppttest_prev_vin = 0.0f;
    s_mppttest_prev_iin = 0.0f;
    s_mppttest_direction = 1;
    s_cvpid_kp = 0.0f;
    s_cvpid_ki = 0.0f;
    s_cvpid_kd = 0.0f;
    s_cvpid_integral = 0.0f;
    s_cvpid_vref = 0.0f;
    s_cvpid_slew_v_s = 0.0f;
    s_cvpid_ff_scale = 0.0f;
    s_cvpid_prev_error = 0.0f;
    s_cvpid_prev2_error = 0.0f;
    s_cvpid_command_counts = 0.0f;
    s_cvpid_feedforward_counts = 0;

    ret = pcb02_hw_init(&s_cfg);
    if (ret != RT_EOK)
    {
        return ret;
    }

    s_sensor_ready = (pcb02_sensors_init(&s_cfg) == RT_EOK) ? 1 : 0;
    s_sensor_fail_count = 0;
    s_last_energy_tick = rt_tick_get();

    rt_kprintf("pcb02: service ready, sensors=%u, pwm=%uHz/%ubit, vbat=%d.%03dV\r\n",
               s_sensor_ready,
               s_cfg.pwm_frequency_hz,
               s_cfg.pwm_resolution_bits,
               (int)s_cfg.voltage_battery_max,
               (int)((s_cfg.voltage_battery_max - (int)s_cfg.voltage_battery_max) * 1000.0f));

    return RT_EOK;
}

rt_err_t pcb02_service_step(void)
{
    pcb02_measurements_t meas;
    pcb02_config_t cfg_snapshot;
    rt_uint8_t software_enable;
    rt_uint8_t hardware_chg_en;
    rt_uint8_t sensor_ready;
    rt_uint8_t chgtest_active;
    rt_uint8_t mppttest_active;
    rt_uint8_t cvpid_active;
    rt_uint8_t cvtest_active;
    rt_err_t ret;

    pcb02_service_lock();
    cfg_snapshot = s_cfg;
    software_enable = s_software_enable;
    sensor_ready = s_sensor_ready;
    chgtest_active = s_chgtest_active;
    mppttest_active = s_mppttest_active;
    cvpid_active = s_cvpid_active;
    cvtest_active = s_cvtest_active;
    pcb02_service_unlock();

    hardware_chg_en = (rt_pin_read(PCB02_PIN_CHG_EN) == PIN_HIGH) ? 1 : 0;

    if (!sensor_ready)
    {
        pcb02_service_lock();
        s_hardware_chg_en = hardware_chg_en;
        pcb02_service_apply_safe_state();
        pcb02_service_unlock();
        return -RT_EBUSY;
    }

    ret = pcb02_sensors_read(&cfg_snapshot, &meas);
    if (ret != RT_EOK)
    {
        pcb02_service_lock();
        s_hardware_chg_en = hardware_chg_en;
        s_ctrl.buck_enable = 0;
        s_ctrl.bypass_enable = 0;
        s_ctrl.pwm_counts = 0;
        pcb02_hw_apply_outputs(0, 0, s_fan_enable, 1, 0, pcb02_pwm_max_counts(&s_cfg));
        if (++s_sensor_fail_count >= PCB02_SENSOR_FAIL_LIMIT)
        {
            rt_kprintf("pcb02: sensor read failed %d, stop active test chg=%u mppttest=%u cvpid=%u cvtest=%u\r\n",
                       ret,
                       chgtest_active,
                       mppttest_active,
                       cvpid_active,
                       cvtest_active);
            s_sensor_ready = 0;
            s_fault_active = 1;
            rt_memset(&s_faults, 0, sizeof(s_faults));
            s_faults.recovery_requested = 1;
            s_faults.error_count = 1;
            s_chgtest_active = 0;
            s_mppttest_active = 0;
            s_cvpid_active = 0;
            s_cvtest_active = 0;
            s_debug_pwm_active = 0;
        }
        pcb02_service_unlock();
        return ret;
    }
    s_sensor_fail_count = 0;

    if (s_debug_pwm_active)
    {
        pcb02_service_lock();
        s_hardware_chg_en = hardware_chg_en;
        s_meas = meas;
        pcb02_hw_apply_outputs(s_ctrl.buck_enable,
                               s_ctrl.bypass_enable,
                               s_fan_enable,
                               s_fault_active,
                               s_ctrl.pwm_counts,
                               pcb02_pwm_max_counts(&s_cfg));
        pcb02_service_unlock();
        return RT_EOK;
    }

    if (s_cvtest_active)
    {
        pcb02_service_lock();
        {
            rt_int32_t pwm = s_ctrl.pwm_counts;
            float error_v;

            s_hardware_chg_en = hardware_chg_en;
            s_meas = meas;
            pcb02_service_update_fan();

            s_faults.over_temperature = (s_meas.temperature_c > s_cfg.temperature_max_c) ? 1 : 0;
            s_faults.input_over_current = (s_meas.current_input > s_cfg.current_in_absolute) ? 1 : 0;
            s_faults.output_over_current = (s_meas.current_output > s_cfg.current_out_absolute) ? 1 : 0;
            s_faults.output_over_voltage = pcb02_service_test_output_over_voltage(s_cvtest_target_v,
                                                                                  PCB02_CV_TEST_OV_MARGIN_V,
                                                                                  PCB02_CV_TEST_OV_INSTANT_MARGIN_V);
            s_faults.fatal_low_voltage = (s_meas.voltage_input < s_cfg.vin_system_min) ? 1 : 0;
            s_faults.input_under_voltage = 0;
            s_faults.battery_not_connected = 0;
            s_faults.recovery_requested = 0;
            s_faults.error_count = s_faults.over_temperature +
                                   s_faults.input_over_current +
                                   s_faults.output_over_current +
                                   s_faults.output_over_voltage +
                                   s_faults.fatal_low_voltage;

            s_fault_active = (s_faults.error_count > 0) ? 1 : 0;
            if (s_fault_active)
            {
                s_ctrl.buck_enable = 0;
                s_ctrl.bypass_enable = 0;
                s_ctrl.pwm_counts = 0;
            }
            else
            {
                if (s_meas.current_output > s_cvtest_current_limit_a)
                {
                    pwm -= 16;
                }
                else
                {
                    error_v = s_cvtest_target_v - s_meas.voltage_output;
                    if (error_v > 1.0f)
                    {
                        pwm += 8;
                    }
                    else if (error_v > 0.2f)
                    {
                        pwm += 4;
                    }
                    else if (error_v > 0.03f)
                    {
                        pwm += 1;
                    }
                    else if (error_v < -0.5f)
                    {
                        pwm -= 8;
                    }
                    else if (error_v < -0.1f)
                    {
                        pwm -= 4;
                    }
                    else if (error_v < -0.03f)
                    {
                        pwm -= 1;
                    }
                }

                if (pwm < 0)
                {
                    pwm = 0;
                }
                if (pwm > s_cvtest_pwm_max_counts)
                {
                    pwm = s_cvtest_pwm_max_counts;
                }

                s_ctrl.buck_enable = (pwm > 0) ? 1 : 0;
                s_ctrl.bypass_enable = 0;
                s_ctrl.pwm_counts = (rt_uint16_t)pwm;
            }

            pcb02_hw_apply_outputs(s_ctrl.buck_enable,
                                   s_ctrl.bypass_enable,
                                   s_fan_enable,
                                   s_fault_active,
                                   s_ctrl.pwm_counts,
                                   pcb02_pwm_max_counts(&s_cfg));
        }
        pcb02_service_unlock();
        return RT_EOK;
    }

    if (s_cvpid_active)
    {
        pcb02_service_lock();
        {
            float error_v;
            float pwm_f;
            float pwm_clamped;
            float integral_limit;
            float dt_s;
            float vin_basis;
            float ff_counts;
            float p_counts;
            float integral_next;
            float kp_eff;
            float ki_eff;
            float kd_counts;
            float abs_error_v;
            float duty_basis;
            float max_step;
            float pwm_delta;
            float step_scale;
            float target_v;
            float vref_step;
            float approach_counts;
            rt_int32_t pwm;

            s_hardware_chg_en = hardware_chg_en;
            s_meas = meas;
            pcb02_service_update_fan();

            s_faults.over_temperature = (s_meas.temperature_c > s_cfg.temperature_max_c) ? 1 : 0;
            s_faults.input_over_current = (s_meas.current_input > s_cfg.current_in_absolute) ? 1 : 0;
            s_faults.output_over_current = (s_meas.current_output > s_cfg.current_out_absolute) ? 1 : 0;
            s_faults.output_over_voltage = pcb02_service_test_output_over_voltage(s_cvtest_target_v,
                                                                                  PCB02_CV_TEST_OV_MARGIN_V,
                                                                                  PCB02_CV_TEST_OV_INSTANT_MARGIN_V);
            s_faults.fatal_low_voltage = (s_meas.voltage_input < s_cfg.vin_system_min) ? 1 : 0;
            s_faults.input_under_voltage = 0;
            s_faults.battery_not_connected = 0;
            s_faults.recovery_requested = 0;
            s_faults.error_count = s_faults.over_temperature +
                                   s_faults.input_over_current +
                                   s_faults.output_over_current +
                                   s_faults.output_over_voltage +
                                   s_faults.fatal_low_voltage;

            s_fault_active = (s_faults.error_count > 0) ? 1 : 0;
            if (s_fault_active)
            {
                s_ctrl.buck_enable = 0;
                s_ctrl.bypass_enable = 0;
                s_ctrl.pwm_counts = 0;
                s_cvpid_integral = 0.0f;
                s_cvpid_command_counts = 0.0f;
                s_cvpid_active = 0;
            }
            else
            {
                dt_s = ((float)s_cfg.routine_interval_ms) / 1000.0f;
                step_scale = dt_s / 0.050f;
                if (step_scale < 0.10f)
                {
                    step_scale = 0.10f;
                }
                if (step_scale > 1.0f)
                {
                    step_scale = 1.0f;
                }
                target_v = s_cvtest_target_v;
                if (s_cvpid_vref <= 0.0f)
                {
                    s_cvpid_vref = (s_cvpid_slew_v_s > 0.0f) ? s_meas.voltage_output : target_v;
                }
                if (s_cvpid_slew_v_s > 0.0f && dt_s > 0.0f)
                {
                    vref_step = s_cvpid_slew_v_s * dt_s;
                    if (s_cvpid_vref < target_v)
                    {
                        s_cvpid_vref += vref_step;
                        if (s_cvpid_vref > target_v)
                        {
                            s_cvpid_vref = target_v;
                        }
                    }
                    else if (s_cvpid_vref > target_v)
                    {
                        s_cvpid_vref -= vref_step;
                        if (s_cvpid_vref < target_v)
                        {
                            s_cvpid_vref = target_v;
                        }
                    }
                }
                else
                {
                    s_cvpid_vref = target_v;
                }
                error_v = s_cvpid_vref - s_meas.voltage_output;
                abs_error_v = error_v < 0.0f ? -error_v : error_v;
                vin_basis = (s_meas.voltage_input > 1.0f) ? s_meas.voltage_input : 12.0f;
                ff_counts = (s_cvpid_vref / vin_basis) * (float)pcb02_pwm_max_counts(&s_cfg) * s_cvpid_ff_scale;
                if (ff_counts < 0.0f)
                {
                    ff_counts = 0.0f;
                }
                if (ff_counts > (float)s_cvtest_pwm_max_counts)
                {
                    ff_counts = (float)s_cvtest_pwm_max_counts;
                }
                s_cvpid_feedforward_counts = (rt_uint16_t)(ff_counts + 0.5f);
                if (s_cvpid_algo == PCB02_CV_ALGO_FASTPI &&
                    s_cvpid_command_counts <= 0.0f &&
                    s_ctrl.pwm_counts == 0 &&
                    s_meas.voltage_output < (s_cvpid_vref - 0.20f))
                {
                    s_cvpid_command_counts = ff_counts * PCB02_FASTPI_PRELOAD_RATIO;
                    s_cvpid_prev_error = error_v;
                    s_cvpid_prev2_error = error_v;
                }

                if (s_meas.current_output > s_cvtest_current_limit_a)
                {
                    pwm_f = s_cvpid_command_counts -
                            (8.0f + ((s_meas.current_output - s_cvtest_current_limit_a) * 100.0f));
                    if (s_cvpid_algo != PCB02_CV_ALGO_INC)
                    {
                        s_cvpid_integral -= 4.0f * dt_s;
                    }
                }
                else
                {
                    kp_eff = s_cvpid_kp;
                    ki_eff = s_cvpid_ki;
                    if (s_cvpid_algo == PCB02_CV_ALGO_FASTPI)
                    {
                        if (abs_error_v > 0.8f)
                        {
                            kp_eff *= 1.35f;
                            ki_eff *= 0.50f;
                        }
                        else if (abs_error_v < 0.08f)
                        {
                            kp_eff *= 0.80f;
                            ki_eff *= 1.15f;
                        }
                    }
                    else if (s_cvpid_algo == PCB02_CV_ALGO_FUZZY)
                    {
                        if (abs_error_v > 0.8f)
                        {
                            kp_eff *= 1.8f;
                            ki_eff *= 0.35f;
                        }
                        else if (abs_error_v > 0.25f)
                        {
                            kp_eff *= 1.25f;
                            ki_eff *= 0.65f;
                        }
                        else
                        {
                            kp_eff *= 0.75f;
                            ki_eff *= 1.10f;
                        }
                    }

                    p_counts = kp_eff * error_v;
                    {
                        float p_limit = (float)s_cvtest_pwm_max_counts *
                                        ((s_cvpid_algo == PCB02_CV_ALGO_FASTPI) ?
                                         PCB02_FASTPI_PROP_LIMIT_RATIO :
                                         PCB02_CVPID_PROP_LIMIT_RATIO);
                        if (p_counts > p_limit)
                        {
                            p_counts = p_limit;
                        }
                        if (p_counts < -p_limit)
                        {
                            p_counts = -p_limit;
                        }
                    }
                    integral_next = s_cvpid_integral + (ki_eff * error_v * dt_s);
                    integral_limit = (float)s_cvtest_pwm_max_counts;
                    if (integral_next > integral_limit)
                    {
                        integral_next = integral_limit;
                    }
                    if (integral_next < -integral_limit)
                    {
                        integral_next = -integral_limit;
                    }

                    if (s_cvpid_algo == PCB02_CV_ALGO_FASTPI)
                    {
                        approach_counts = 0.0f;
                        if (error_v > 1.00f)
                        {
                            approach_counts = 8.0f;
                        }
                        else if (error_v > 0.35f)
                        {
                            approach_counts = 4.0f;
                        }
                        else if (error_v > 0.08f)
                        {
                            approach_counts = 1.0f;
                        }
                        else if (error_v < -0.35f)
                        {
                            approach_counts = -10.0f;
                        }
                        else if (error_v < -0.08f)
                        {
                            approach_counts = -3.0f;
                        }

                        pwm_f = s_cvpid_command_counts +
                                approach_counts +
                                (0.25f * kp_eff * (error_v - s_cvpid_prev_error)) +
                                (ki_eff * error_v * dt_s);
                    }
                    else if (s_cvpid_algo == PCB02_CV_ALGO_INC)
                    {
                        kd_counts = (dt_s > 0.0f) ?
                                    (s_cvpid_kd *
                                     (error_v - (2.0f * s_cvpid_prev_error) + s_cvpid_prev2_error) / dt_s) :
                                    0.0f;
                        pwm_f = s_cvpid_command_counts +
                                (kp_eff * (error_v - s_cvpid_prev_error)) +
                                (ki_eff * error_v * dt_s) +
                                kd_counts;
                    }
                    else
                    {
                        pwm_f = (float)s_cvpid_feedforward_counts + p_counts + integral_next;
                    }
                    pwm_clamped = pwm_f;
                    if (pwm_clamped < 0.0f)
                    {
                        pwm_clamped = 0.0f;
                    }
                    if (pwm_clamped > (float)s_cvtest_pwm_max_counts)
                    {
                        pwm_clamped = (float)s_cvtest_pwm_max_counts;
                    }

                    if (s_cvpid_algo == PCB02_CV_ALGO_INC ||
                        s_cvpid_algo == PCB02_CV_ALGO_FASTPI)
                    {
                        s_cvpid_integral = 0.0f;
                    }
                    else if ((pwm_clamped == pwm_f) ||
                        (pwm_clamped >= (float)s_cvtest_pwm_max_counts && error_v < 0.0f) ||
                        (pwm_clamped <= 0.0f && error_v > 0.0f))
                    {
                        s_cvpid_integral = integral_next;
                    }

                    if (s_cvpid_algo != PCB02_CV_ALGO_INC &&
                        s_cvpid_algo != PCB02_CV_ALGO_FASTPI)
                    {
                        pwm_f = (float)s_cvpid_feedforward_counts + p_counts + s_cvpid_integral;
                    }
                }

                if (pwm_f < 0.0f)
                {
                    pwm_f = 0.0f;
                }
                if (pwm_f > (float)s_cvtest_pwm_max_counts)
                {
                    pwm_f = (float)s_cvtest_pwm_max_counts;
                }

                pwm_delta = pwm_f - s_cvpid_command_counts;
                if (pwm_delta < 0.0f && error_v <= -PCB02_CVPID_ERR_MID_V)
                {
                    max_step = PCB02_CVPID_STEP_DOWN_FAR * step_scale;
                }
                else if (pwm_delta < 0.0f)
                {
                    max_step = PCB02_CVPID_STEP_DOWN_NEAR * step_scale;
                }
                else if (s_cvpid_algo == PCB02_CV_ALGO_FASTPI)
                {
                    if (error_v > PCB02_CVPID_ERR_MID_V)
                    {
                        max_step = (float)s_cvtest_pwm_max_counts;
                    }
                    else if (error_v < -0.20f)
                    {
                        max_step = PCB02_CVPID_STEP_DOWN_FAR * 2.0f * step_scale;
                    }
                    else
                    {
                        max_step = PCB02_CVPID_STEP_UP_MID * step_scale;
                    }
                }
                else if (s_cvpid_algo != PCB02_CV_ALGO_INC)
                {
                    if (abs_error_v >= PCB02_CVPID_ERR_FAR_V)
                    {
                        max_step = PCB02_CVPID_STEP_UP_FAR * step_scale;
                    }
                    else if (abs_error_v >= PCB02_CVPID_ERR_MID_V)
                    {
                        max_step = PCB02_CVPID_STEP_UP_MID * step_scale;
                    }
                    else
                    {
                        max_step = PCB02_CVPID_STEP_UP_NEAR * step_scale;
                    }
                }
                else
                {
                    max_step = (float)s_cvtest_pwm_max_counts;
                }
                if (max_step < 1.0f)
                {
                    max_step = 1.0f;
                }

                if (pwm_delta < -max_step)
                {
                    pwm_f = s_cvpid_command_counts - max_step;
                }
                if (pwm_delta > max_step)
                {
                    pwm_f = s_cvpid_command_counts + max_step;
                }

                if (pwm_f < 0.0f)
                {
                    pwm_f = 0.0f;
                }
                if (pwm_f > (float)s_cvtest_pwm_max_counts)
                {
                    pwm_f = (float)s_cvtest_pwm_max_counts;
                }
                s_cvpid_command_counts = pwm_f;
                pwm = (rt_int32_t)(pwm_f + 0.5f);
                s_ctrl.buck_enable = (pwm > 0) ? 1 : 0;
                s_ctrl.bypass_enable = 0;
                s_ctrl.pwm_counts = (rt_uint16_t)pwm;

                if (s_cvpid_algo == PCB02_CV_ALGO_ADAPT &&
                    s_cvpid_vref >= s_cvtest_target_v &&
                    error_v < 0.08f && error_v > -0.08f &&
                    s_meas.voltage_input > 1.0f &&
                    s_cvtest_target_v > 0.1f)
                {
                    duty_basis = (s_cvtest_target_v / s_meas.voltage_input) * (float)pcb02_pwm_max_counts(&s_cfg);
                    if (duty_basis > 1.0f)
                    {
                        s_cvpid_ff_scale = (0.98f * s_cvpid_ff_scale) +
                                           (0.02f * ((float)s_ctrl.pwm_counts / duty_basis));
                        if (s_cvpid_ff_scale < 0.05f)
                        {
                            s_cvpid_ff_scale = 0.05f;
                        }
                        if (s_cvpid_ff_scale > 1.0f)
                        {
                            s_cvpid_ff_scale = 1.0f;
                        }
                    }
                }
                s_cvpid_prev2_error = s_cvpid_prev_error;
                s_cvpid_prev_error = error_v;
            }

            pcb02_hw_apply_outputs(s_ctrl.buck_enable,
                                   s_ctrl.bypass_enable,
                                   s_fan_enable,
                                   s_fault_active,
                                   s_ctrl.pwm_counts,
                                   pcb02_pwm_max_counts(&s_cfg));
        }
        pcb02_service_unlock();
        return RT_EOK;
    }

    if (s_mppttest_active)
    {
        pcb02_service_lock();
        {
            rt_int32_t pwm = s_ctrl.pwm_counts;
            rt_int32_t step = (s_mppttest_step_counts > 0) ? s_mppttest_step_counts : 2;
            float dp;
            float dv;
            float di;
            float dpidv;

            s_hardware_chg_en = hardware_chg_en;
            s_meas = meas;
            pcb02_service_update_fan();

            s_faults.over_temperature = (s_meas.temperature_c > s_cfg.temperature_max_c) ? 1 : 0;
            s_faults.input_over_current = (s_meas.current_input > s_cfg.current_in_absolute) ? 1 : 0;
            s_faults.output_over_current = (s_meas.current_output > s_cfg.current_out_absolute) ? 1 : 0;
            s_faults.output_over_voltage = pcb02_service_test_output_over_voltage(s_mppttest_vout_max,
                                                                                  PCB02_MPPT_TEST_OV_MARGIN_V,
                                                                                  PCB02_MPPT_TEST_OV_INSTANT_MARGIN_V);
            s_faults.fatal_low_voltage = (s_meas.voltage_input < s_cfg.vin_system_min) ? 1 : 0;
            s_faults.input_under_voltage = 0;
            s_faults.battery_not_connected = 0;
            s_faults.recovery_requested = 0;
            s_faults.error_count = s_faults.over_temperature +
                                   s_faults.input_over_current +
                                   s_faults.output_over_current +
                                   s_faults.output_over_voltage +
                                   s_faults.fatal_low_voltage;

            s_fault_active = (s_faults.error_count > 0) ? 1 : 0;
            if (s_fault_active)
            {
                s_ctrl.buck_enable = 0;
                s_ctrl.bypass_enable = 0;
                s_ctrl.pwm_counts = 0;
                s_mppttest_active = 0;
            }
            else if (s_meas.current_output > s_mppttest_iout_limit ||
                     s_meas.voltage_output > s_mppttest_vout_max)
            {
                pwm -= (step * 4);
            }
            else if (s_meas.voltage_input < (s_meas.voltage_output + s_cfg.voltage_dropout))
            {
                pwm -= (step * 2);
            }
            else if (s_meas.current_input < 0.005f || s_meas.power_input < 0.05f)
            {
                pwm += step;
            }
            else
            {
                dp = s_meas.power_input - s_mppttest_prev_power;
                dv = s_meas.voltage_input - s_mppttest_prev_vin;
                di = s_meas.current_input - s_mppttest_prev_iin;

                if (s_mppttest_algo == PCB02_MPPT_ALGO_INCCOND)
                {
                    dpidv = 0.0f;
                    if (dv > 0.02f || dv < -0.02f)
                    {
                        dpidv = s_meas.voltage_input * (di / dv) + s_meas.current_input;
                    }
                    else
                    {
                        dpidv = di;
                    }

                    if (dpidv > 0.01f)
                    {
                        pwm -= step;
                    }
                    else if (dpidv < -0.01f)
                    {
                        pwm += step;
                    }
                }
                else
                {
                    if (s_mppttest_algo == PCB02_MPPT_ALGO_VSPO)
                    {
                        if (dp > 0.20f || dp < -0.20f)
                        {
                            step *= 4;
                        }
                        else if (dp > 0.05f || dp < -0.05f)
                        {
                            step *= 2;
                        }
                    }
                    else if (s_mppttest_algo == PCB02_MPPT_ALGO_ADAPT)
                    {
                        if ((dp < 0.03f && dp > -0.03f) || (dv < 0.03f && dv > -0.03f))
                        {
                            step = 1;
                        }
                        else if (dp > 0.15f || dp < -0.15f)
                        {
                            step *= 3;
                        }
                    }

                    if (dp < 0.0f)
                    {
                        s_mppttest_direction = -s_mppttest_direction;
                    }
                    pwm += s_mppttest_direction * step;
                }
            }

            if (pwm < 0)
            {
                pwm = 0;
            }
            if (pwm > s_mppttest_pwm_max_counts)
            {
                pwm = s_mppttest_pwm_max_counts;
            }

            s_ctrl.buck_enable = (pwm > 0) ? 1 : 0;
            s_ctrl.bypass_enable = 0;
            s_ctrl.pwm_counts = (rt_uint16_t)pwm;
            s_mppttest_prev_power = s_meas.power_input;
            s_mppttest_prev_vin = s_meas.voltage_input;
            s_mppttest_prev_iin = s_meas.current_input;

            pcb02_hw_apply_outputs(s_ctrl.buck_enable,
                                   s_ctrl.bypass_enable,
                                   s_fan_enable,
                                   s_fault_active,
                                   s_ctrl.pwm_counts,
                                   pcb02_pwm_max_counts(&s_cfg));
        }
        pcb02_service_unlock();
        return RT_EOK;
    }

    if (s_chgtest_active)
    {
        pcb02_service_lock();
        {
            float dt_s;
            float vin_basis;
            float ff_counts;
            float error_v;
            float pwm_f;
            float pwm_delta;
            float kp;
            float ki;
            float integral_next;
            float max_step;
            float vref_step;
            float target_error_v;
            rt_int32_t pwm;

            s_hardware_chg_en = hardware_chg_en;
            s_meas = meas;
            pcb02_service_update_fan();

            s_faults.over_temperature = (s_meas.temperature_c > s_cfg.temperature_max_c) ? 1 : 0;
            s_faults.input_over_current = (s_meas.current_input > s_cfg.current_in_absolute) ? 1 : 0;
            s_faults.output_over_current = (s_meas.current_output > s_cfg.current_out_absolute) ? 1 : 0;
            s_faults.output_over_voltage = pcb02_service_test_output_over_voltage(s_chgtest_vbat_cv,
                                                                                  PCB02_CV_TEST_OV_MARGIN_V,
                                                                                  PCB02_CV_TEST_OV_INSTANT_MARGIN_V);
            s_faults.fatal_low_voltage = (s_meas.voltage_input < s_cfg.vin_system_min &&
                                          s_meas.voltage_output < s_cfg.vin_system_min) ? 1 : 0;
            s_faults.input_under_voltage = 0;
            s_faults.battery_not_connected = (s_meas.voltage_output < s_cfg.vin_system_min) ? 1 : 0;
            s_faults.recovery_requested = 0;
            if (s_ctrl.pwm_counts > PCB02_CHG_NO_POWER_PWM_COUNTS &&
                s_meas.voltage_output < 0.20f &&
                s_meas.current_input < 0.010f &&
                s_meas.current_output < 0.005f)
            {
                if (s_chgtest_no_power_count < PCB02_CHG_NO_POWER_TRIPS)
                {
                    s_chgtest_no_power_count++;
                }
                if (s_chgtest_no_power_count >= PCB02_CHG_NO_POWER_TRIPS)
                {
                    s_faults.recovery_requested = 1;
                }
            }
            else
            {
                s_chgtest_no_power_count = 0;
            }
            s_faults.error_count = s_faults.over_temperature +
                                   s_faults.input_over_current +
                                   s_faults.output_over_current +
                                   s_faults.output_over_voltage +
                                   s_faults.fatal_low_voltage +
                                   s_faults.recovery_requested;

            s_fault_active = (s_faults.error_count > 0) ? 1 : 0;
            if (s_fault_active)
            {
                rt_kprintf("pcb02: chgtest fault stop OT=%u IIN_OC=%u IOUT_OC=%u OOV=%u FLV=%u IUV=%u BNC=%u REC=%u vin_mv=%d vout_mv=%d iin_ma=%d iout_ma=%d pwm=%u\r\n",
                           s_faults.over_temperature,
                           s_faults.input_over_current,
                           s_faults.output_over_current,
                           s_faults.output_over_voltage,
                           s_faults.fatal_low_voltage,
                           s_faults.input_under_voltage,
                           s_faults.battery_not_connected,
                           s_faults.recovery_requested,
                           (int)(s_meas.voltage_input * 1000.0f),
                           (int)(s_meas.voltage_output * 1000.0f),
                           (int)(s_meas.current_input * 1000.0f),
                           (int)(s_meas.current_output * 1000.0f),
                           s_ctrl.pwm_counts);
                s_ctrl.buck_enable = 0;
                s_ctrl.bypass_enable = 0;
                s_ctrl.pwm_counts = 0;
                s_chgtest_command_counts = 0.0f;
                s_chgtest_integral = 0.0f;
                s_chgtest_vref = 0.0f;
                s_chgtest_no_power_count = 0;
                s_chgtest_state = PCB02_CHG_STATE_FAULT;
                s_chgtest_active = 0;
            }
            else
            {
                dt_s = ((float)s_cfg.routine_interval_ms) / 1000.0f;
                if (dt_s <= 0.0f)
                {
                    dt_s = 0.005f;
                }
                if (s_chgtest_vref <= 0.0f)
                {
                    s_chgtest_vref = s_meas.voltage_output;
                    if (s_chgtest_vref < 0.0f)
                    {
                        s_chgtest_vref = 0.0f;
                    }
                }
                vref_step = PCB02_CHG_VREF_SLEW_V_PER_S * dt_s;
                if (vref_step < 0.02f)
                {
                    vref_step = 0.02f;
                }
                if (s_chgtest_vref < s_chgtest_vbat_cv)
                {
                    s_chgtest_vref += vref_step;
                    if (s_chgtest_vref > s_chgtest_vbat_cv)
                    {
                        s_chgtest_vref = s_chgtest_vbat_cv;
                    }
                }
                else
                {
                    s_chgtest_vref = s_chgtest_vbat_cv;
                }

                vin_basis = (s_meas.voltage_input > 1.0f) ? s_meas.voltage_input : 1.0f;
                ff_counts = (s_chgtest_vref / vin_basis) *
                            (float)pcb02_pwm_max_counts(&s_cfg) *
                            0.92f;
                if (ff_counts < 0.0f)
                {
                    ff_counts = 0.0f;
                }
                if (ff_counts > (float)s_chgtest_pwm_max_counts)
                {
                    ff_counts = (float)s_chgtest_pwm_max_counts;
                }
                s_chgtest_feedforward_counts = (rt_uint16_t)(ff_counts + 0.5f);

                if (s_meas.voltage_input < s_chgtest_vin_min)
                {
                    pwm_f = s_chgtest_command_counts - 10.0f;
                    s_chgtest_integral = 0.0f;
                    s_chgtest_state = PCB02_CHG_STATE_PV_LOW;
                }
                else if (s_meas.current_output > s_chgtest_ichg_limit)
                {
                    pwm_f = s_chgtest_command_counts -
                            (6.0f + ((s_meas.current_output - s_chgtest_ichg_limit) * 80.0f));
                    if (s_chgtest_integral > 0.0f)
                    {
                        s_chgtest_integral -= 8.0f * dt_s;
                    }
                    s_chgtest_state = PCB02_CHG_STATE_CC;
                }
                else
                {
                    error_v = s_chgtest_vref - s_meas.voltage_output;
                    if (s_chgtest_vbat_cv >= 8.0f)
                    {
                        kp = 10.0f;
                        ki = 4.0f;
                    }
                    else if (s_chgtest_vbat_cv >= 6.0f)
                    {
                        kp = 18.0f;
                        ki = 7.0f;
                    }
                    else
                    {
                        kp = 28.0f;
                        ki = 12.0f;
                    }

                    integral_next = s_chgtest_integral + (ki * error_v * dt_s);
                    if (integral_next > 160.0f)
                    {
                        integral_next = 160.0f;
                    }
                    if (integral_next < -160.0f)
                    {
                        integral_next = -160.0f;
                    }

                    pwm_f = ff_counts + (kp * error_v) + integral_next;
                    if ((pwm_f >= 0.0f && pwm_f <= (float)s_chgtest_pwm_max_counts) ||
                        (pwm_f > (float)s_chgtest_pwm_max_counts && error_v < 0.0f) ||
                        (pwm_f < 0.0f && error_v > 0.0f))
                    {
                        s_chgtest_integral = integral_next;
                    }

                    target_error_v = s_chgtest_vbat_cv - s_meas.voltage_output;
                    s_chgtest_state = (target_error_v > 0.12f) ? PCB02_CHG_STATE_CC : PCB02_CHG_STATE_CV;
                }

                if (pwm_f < 0.0f)
                {
                    pwm_f = 0.0f;
                }
                if (pwm_f > (float)s_chgtest_pwm_max_counts)
                {
                    pwm_f = (float)s_chgtest_pwm_max_counts;
                }

                pwm_delta = pwm_f - s_chgtest_command_counts;
                max_step = 18.0f;
                if (s_chgtest_state == PCB02_CHG_STATE_PV_LOW || pwm_delta < 0.0f)
                {
                    max_step = 30.0f;
                }
                else if (s_chgtest_state == PCB02_CHG_STATE_CV)
                {
                    max_step = 8.0f;
                }

                if (pwm_delta > max_step)
                {
                    pwm_f = s_chgtest_command_counts + max_step;
                }
                if (pwm_delta < -max_step)
                {
                    pwm_f = s_chgtest_command_counts - max_step;
                }
                if (pwm_f < 0.0f)
                {
                    pwm_f = 0.0f;
                }

                s_chgtest_command_counts = pwm_f;
                pwm = (rt_int32_t)(pwm_f + 0.5f);
                s_ctrl.buck_enable = (pwm > 0) ? 1 : 0;
                s_ctrl.bypass_enable = 0;
                s_ctrl.pwm_counts = (rt_uint16_t)pwm;
            }

            pcb02_hw_apply_outputs(s_ctrl.buck_enable,
                                   s_ctrl.bypass_enable,
                                   s_fan_enable,
                                   s_fault_active,
                                   s_ctrl.pwm_counts,
                                   pcb02_pwm_max_counts(&s_cfg));
        }
        pcb02_service_unlock();
        return RT_EOK;
    }

    if (!software_enable || !hardware_chg_en)
    {
        pcb02_service_lock();
        s_hardware_chg_en = hardware_chg_en;
        s_meas = meas;
        pcb02_service_apply_safe_state();
        pcb02_service_unlock();
        return -RT_EBUSY;
    }

    pcb02_service_lock();
    {
        rt_tick_t now;
        float elapsed_h;
        rt_uint16_t pwm;

        s_hardware_chg_en = hardware_chg_en;
        s_meas = meas;
        pcb02_eval_protection(&s_cfg, &s_meas, &s_faults);
        pwm = pcb02_charging_step(&s_cfg, &s_meas, &s_faults, &s_ctrl);
        s_fault_active = (s_faults.error_count > 0) ? 1 : 0;
        pcb02_service_update_fan();

        now = rt_tick_get();
        elapsed_h = (float)(now - s_last_energy_tick) / (float)RT_TICK_PER_SECOND / 3600.0f;
        s_last_energy_tick = now;
        s_ctrl.watt_hours += s_meas.power_input * elapsed_h;

        pcb02_hw_apply_outputs(s_ctrl.buck_enable,
                               s_ctrl.bypass_enable,
                               s_fan_enable,
                               s_fault_active,
                               pwm,
                               pcb02_pwm_max_counts(&s_cfg));
    }
    pcb02_service_unlock();

    return RT_EOK;
}

void pcb02_service_get_status(pcb02_status_t *status)
{
    if (status == RT_NULL)
    {
        return;
    }

    pcb02_service_lock();
    status->cfg = s_cfg;
    status->meas = s_meas;
    status->faults = s_faults;
    status->ctrl = s_ctrl;
    status->software_enable = s_software_enable;
    status->hardware_chg_en = s_hardware_chg_en;
    status->sensor_ready = s_sensor_ready;
    status->fault_active = s_fault_active;
    status->fan_enable = s_fan_enable;
    status->debug_pwm_active = s_debug_pwm_active;
    status->cvtest_active = s_cvtest_active;
    status->cvpid_active = s_cvpid_active;
    status->cvpid_algo = s_cvpid_algo;
    status->chgtest_active = s_chgtest_active;
    status->chgtest_state = s_chgtest_state;
    status->mppttest_active = s_mppttest_active;
    status->mppttest_algo = s_mppttest_algo;
    status->cvtest_target_v = s_cvtest_target_v;
    status->cvtest_current_limit_a = s_cvtest_current_limit_a;
    status->cvtest_pwm_max_counts = s_cvtest_pwm_max_counts;
    status->chgtest_vbat_cv = s_chgtest_vbat_cv;
    status->chgtest_ichg_limit = s_chgtest_ichg_limit;
    status->chgtest_vin_min = s_chgtest_vin_min;
    status->chgtest_pwm_max_counts = s_chgtest_pwm_max_counts;
    status->chgtest_command_counts = s_chgtest_command_counts;
    status->chgtest_integral = s_chgtest_integral;
    status->chgtest_vref = s_chgtest_vref;
    status->chgtest_feedforward_counts = s_chgtest_feedforward_counts;
    status->mppttest_vout_max = s_mppttest_vout_max;
    status->mppttest_iout_limit = s_mppttest_iout_limit;
    status->mppttest_pwm_max_counts = s_mppttest_pwm_max_counts;
    status->mppttest_step_counts = s_mppttest_step_counts;
    status->cvpid_kp = s_cvpid_kp;
    status->cvpid_ki = s_cvpid_ki;
    status->cvpid_kd = s_cvpid_kd;
    status->cvpid_integral = s_cvpid_integral;
    status->cvpid_vref = s_cvpid_vref;
    status->cvpid_slew_v_s = s_cvpid_slew_v_s;
    status->cvpid_ff_scale = s_cvpid_ff_scale;
    status->cvpid_prev_error = s_cvpid_prev_error;
    status->cvpid_prev2_error = s_cvpid_prev2_error;
    status->cvpid_command_counts = s_cvpid_command_counts;
    status->cvpid_feedforward_counts = s_cvpid_feedforward_counts;
    pcb02_service_unlock();
}

void pcb02_service_set_enable(rt_uint8_t enable)
{
    pcb02_service_lock();
    s_software_enable = enable ? 1 : 0;
    s_debug_pwm_active = 0;
    s_cvtest_active = 0;
    s_cvpid_active = 0;
    s_chgtest_active = 0;
    s_chgtest_state = PCB02_CHG_STATE_IDLE;
    s_chgtest_no_power_count = 0;
    s_mppttest_active = 0;
    s_cvpid_algo = PCB02_CV_ALGO_PI;
    s_mppttest_algo = PCB02_MPPT_ALGO_PO;
    s_cvtest_target_v = 0.0f;
    s_cvtest_current_limit_a = 0.0f;
    s_cvtest_pwm_max_counts = 0;
    s_chgtest_vbat_cv = 0.0f;
    s_chgtest_ichg_limit = 0.0f;
    s_chgtest_vin_min = 0.0f;
    s_chgtest_pwm_max_counts = 0;
    s_chgtest_command_counts = 0.0f;
    s_chgtest_integral = 0.0f;
    s_chgtest_vref = 0.0f;
    s_chgtest_feedforward_counts = 0;
    s_cvpid_kp = 0.0f;
    s_cvpid_ki = 0.0f;
    s_cvpid_kd = 0.0f;
    s_cvpid_integral = 0.0f;
    s_cvpid_vref = 0.0f;
    s_cvpid_slew_v_s = 0.0f;
    s_cvpid_ff_scale = 0.0f;
    s_cvpid_prev_error = 0.0f;
    s_cvpid_prev2_error = 0.0f;
    s_cvpid_command_counts = 0.0f;
    s_cvpid_feedforward_counts = 0;
    s_mppttest_vout_max = 0.0f;
    s_mppttest_iout_limit = 0.0f;
    s_mppttest_pwm_max_counts = 0;
    s_mppttest_step_counts = 0;
    s_mppttest_prev_power = 0.0f;
    s_mppttest_prev_vin = 0.0f;
    s_mppttest_prev_iin = 0.0f;
    s_mppttest_direction = 1;
    if (!s_software_enable)
    {
        pcb02_service_apply_safe_state();
    }
    pcb02_service_unlock();
}

void pcb02_service_set_mppt_mode(rt_uint8_t mppt_enable)
{
    pcb02_service_lock();
    s_cfg.mppt_mode = mppt_enable ? 1 : 0;
    pcb02_service_unlock();
}

void pcb02_service_set_output_mode(rt_uint8_t output_mode)
{
    pcb02_service_lock();
    s_cfg.output_mode = output_mode ? 1 : 0;
    pcb02_service_unlock();
}

void pcb02_service_set_charge_limits(float voltage_battery_max, float current_charging_max)
{
    pcb02_service_lock();
    if (voltage_battery_max > 0.0f)
    {
        s_cfg.voltage_battery_max = voltage_battery_max;
    }
    if (current_charging_max > 0.0f)
    {
        s_cfg.current_charging_max = current_charging_max;
    }
    pcb02_service_unlock();
}

void pcb02_service_clear_fault_latch(void)
{
    pcb02_service_lock();
    rt_memset(&s_faults, 0, sizeof(s_faults));
    s_fault_active = 0;
    s_test_oov_count = 0;
    s_sensor_ready = 1;
    s_sensor_fail_count = 0;
    pcb02_control_state_init(&s_ctrl);
    s_last_energy_tick = rt_tick_get();
    pcb02_service_unlock();
}

const pcb02_config_t *pcb02_service_get_config(void)
{
    return &s_cfg;
}

void pcb02_service_fan_test(rt_int8_t enable)
{
    rt_uint16_t pwm_max;

    pcb02_service_lock();
    pwm_max = pcb02_pwm_max_counts(&s_cfg);

    if (enable < 0)
    {
        s_fan_test_active = 0;
        pcb02_service_update_fan();
    }
    else
    {
        s_fan_test_active = 1;
        s_fan_test_enable = enable ? 1 : 0;
        s_fan_enable = s_fan_test_enable;
    }

    pcb02_hw_apply_outputs(s_ctrl.buck_enable,
                           s_ctrl.bypass_enable,
                           s_fan_enable,
                           s_fault_active,
                           s_ctrl.pwm_counts,
                           pwm_max);
    pcb02_service_unlock();
}

void pcb02_service_debug_pwm(rt_uint16_t permille)
{
    rt_uint16_t pwm_max;
    rt_uint16_t pwm_counts;

    if (permille > 300)
    {
        permille = 300;
    }

    pcb02_service_lock();
    pwm_max = pcb02_pwm_max_counts(&s_cfg);
    pwm_counts = (rt_uint16_t)(((rt_uint32_t)pwm_max * permille) / 1000U);

    s_software_enable = 0;
    s_cvtest_active = 0;
    s_cvpid_active = 0;
    s_chgtest_active = 0;
    s_chgtest_state = PCB02_CHG_STATE_IDLE;
    s_chgtest_no_power_count = 0;
    s_mppttest_active = 0;
    s_cvpid_algo = PCB02_CV_ALGO_PI;
    s_mppttest_algo = PCB02_MPPT_ALGO_PO;
    s_cvpid_integral = 0.0f;
    s_cvpid_vref = 0.0f;
    s_cvpid_slew_v_s = 0.0f;
    s_cvpid_ff_scale = 0.0f;
    s_cvpid_prev_error = 0.0f;
    s_cvpid_prev2_error = 0.0f;
    s_cvpid_command_counts = 0.0f;
    s_cvpid_feedforward_counts = 0;
    s_ctrl.buck_enable = (permille > 0) ? 1 : 0;
    s_ctrl.bypass_enable = 0;
    s_ctrl.pwm_counts = pwm_counts;
    s_fault_active = 0;
    s_test_oov_count = 0;
    s_fan_enable = 0;
    s_fan_test_active = 0;
    s_fan_test_enable = 0;
    s_debug_pwm_active = (permille > 0) ? 1 : 0;

    pcb02_hw_apply_outputs(s_ctrl.buck_enable,
                           s_ctrl.bypass_enable,
                           s_fan_enable,
                           s_fault_active,
                           s_ctrl.pwm_counts,
                           pwm_max);
    pcb02_service_unlock();
}

void pcb02_service_cvtest(float target_v, float current_limit_a, rt_uint16_t max_permille)
{
    rt_uint16_t pwm_max;

    if (target_v <= 0.0f || current_limit_a <= 0.0f || max_permille == 0)
    {
        pcb02_service_set_enable(0);
        return;
    }

    if (target_v > s_cfg.voltage_battery_max)
    {
        target_v = s_cfg.voltage_battery_max;
    }
    if (current_limit_a > s_cfg.current_out_absolute)
    {
        current_limit_a = s_cfg.current_out_absolute;
    }
    if (max_permille > 300)
    {
        max_permille = 300;
    }

    pcb02_service_lock();
    pwm_max = pcb02_pwm_max_counts(&s_cfg);

    s_software_enable = 0;
    s_debug_pwm_active = 0;
    s_cvtest_active = 1;
    s_cvpid_active = 0;
    s_chgtest_active = 0;
    s_chgtest_state = PCB02_CHG_STATE_IDLE;
    s_mppttest_active = 0;
    s_cvtest_target_v = target_v;
    s_cvtest_current_limit_a = current_limit_a;
    s_cvtest_pwm_max_counts = (rt_uint16_t)(((rt_uint32_t)pwm_max * max_permille) / 1000U);

    s_fault_active = 0;
    s_test_oov_count = 0;
    s_fan_enable = 0;
    s_fan_test_active = 0;
    s_fan_test_enable = 0;
    rt_memset(&s_faults, 0, sizeof(s_faults));
    s_ctrl.buck_enable = 0;
    s_ctrl.bypass_enable = 0;
    s_ctrl.pwm_counts = 0;

    pcb02_hw_apply_outputs(0, 0, 0, 0, 0, pwm_max);
    pcb02_service_unlock();
}

void pcb02_service_cvpid(float target_v,
                         float current_limit_a,
                         rt_uint16_t max_permille,
                         float kp,
                         float ki,
                         float slew_v_s,
                         float ff_scale)
{
    pcb02_service_cvctrl(PCB02_CV_ALGO_PI,
                         target_v,
                         current_limit_a,
                         max_permille,
                         kp,
                         ki,
                         0.0f,
                         slew_v_s,
                         ff_scale);
}

void pcb02_service_cvctrl(rt_uint8_t algo,
                          float target_v,
                          float current_limit_a,
                          rt_uint16_t max_permille,
                          float kp,
                          float ki,
                          float kd,
                          float slew_v_s,
                          float ff_scale)
{
    rt_uint16_t pwm_max;

    if (target_v <= 0.0f || current_limit_a <= 0.0f || max_permille == 0 ||
        kp < 0.0f || ki < 0.0f || kd < 0.0f || slew_v_s < 0.0f || ff_scale < 0.0f)
    {
        pcb02_service_set_enable(0);
        return;
    }

    if (target_v > s_cfg.voltage_battery_max)
    {
        target_v = s_cfg.voltage_battery_max;
    }
    if (current_limit_a > s_cfg.current_out_absolute)
    {
        current_limit_a = s_cfg.current_out_absolute;
    }
    if (max_permille > 970)
    {
        max_permille = 970;
    }

    pcb02_service_lock();
    pwm_max = pcb02_pwm_max_counts(&s_cfg);

    s_software_enable = 0;
    s_debug_pwm_active = 0;
    s_cvtest_active = 0;
    s_cvpid_active = 1;
    s_chgtest_active = 0;
    s_chgtest_state = PCB02_CHG_STATE_IDLE;
    s_mppttest_active = 0;
    s_cvpid_algo = (algo <= PCB02_CV_ALGO_FASTPI) ? algo : PCB02_CV_ALGO_FASTPI;
    s_cvtest_target_v = target_v;
    s_cvtest_current_limit_a = current_limit_a;
    s_cvtest_pwm_max_counts = (rt_uint16_t)(((rt_uint32_t)pwm_max * max_permille) / 1000U);
    s_cvpid_kp = kp;
    s_cvpid_ki = ki;
    s_cvpid_kd = kd;
    s_cvpid_integral = 0.0f;
    s_cvpid_vref = 0.0f;
    s_cvpid_slew_v_s = (slew_v_s > 0.0f) ? slew_v_s : PCB02_CVPID_DEFAULT_SLEW_V_S;
    if (ff_scale > 1.0f)
    {
        ff_scale = 1.0f;
    }
    if (s_cvpid_algo == PCB02_CV_ALGO_FASTPI &&
        (ff_scale <= 0.0f || ff_scale == PCB02_CVPID_DEFAULT_FF_SCALE))
    {
        ff_scale = PCB02_FASTPI_DEFAULT_FF_SCALE;
    }
    s_cvpid_ff_scale = (ff_scale > 0.0f) ? ff_scale : PCB02_CVPID_DEFAULT_FF_SCALE;
    s_cvpid_prev_error = 0.0f;
    s_cvpid_prev2_error = 0.0f;
    s_cvpid_command_counts = 0.0f;
    s_cvpid_feedforward_counts = 0;

    s_fault_active = 0;
    s_test_oov_count = 0;
    s_fan_enable = 0;
    s_fan_test_active = 0;
    s_fan_test_enable = 0;
    rt_memset(&s_faults, 0, sizeof(s_faults));
    s_ctrl.buck_enable = 0;
    s_ctrl.bypass_enable = 0;
    s_ctrl.pwm_counts = 0;

    pcb02_hw_apply_outputs(s_ctrl.buck_enable,
                           s_ctrl.bypass_enable,
                           s_fan_enable,
                           s_fault_active,
                           s_ctrl.pwm_counts,
                           pwm_max);
    pcb02_service_unlock();
}

void pcb02_service_chgtest(float vbat_cv,
                           float current_limit_a,
                           float vin_min,
                           rt_uint16_t max_permille)
{
    rt_uint16_t pwm_max;

    if (vbat_cv <= 0.0f || current_limit_a <= 0.0f || vin_min <= 0.0f || max_permille == 0)
    {
        pcb02_service_set_enable(0);
        return;
    }

    if (vbat_cv > s_cfg.voltage_battery_max)
    {
        vbat_cv = s_cfg.voltage_battery_max;
    }
    if (current_limit_a > s_cfg.current_out_absolute)
    {
        current_limit_a = s_cfg.current_out_absolute;
    }
    if (max_permille > 970)
    {
        max_permille = 970;
    }

    pcb02_service_lock();
    pwm_max = pcb02_pwm_max_counts(&s_cfg);

    s_software_enable = 0;
    s_debug_pwm_active = 0;
    s_cvtest_active = 0;
    s_cvpid_active = 0;
    s_chgtest_active = 1;
    s_chgtest_state = PCB02_CHG_STATE_IDLE;
    s_mppttest_active = 0;
    s_chgtest_vbat_cv = vbat_cv;
    s_chgtest_ichg_limit = current_limit_a;
    s_chgtest_vin_min = vin_min;
    s_chgtest_pwm_max_counts = (rt_uint16_t)(((rt_uint32_t)pwm_max * max_permille) / 1000U);
    s_chgtest_command_counts = 0.0f;
    s_chgtest_integral = 0.0f;
    s_chgtest_vref = 0.0f;
    s_chgtest_feedforward_counts = 0;
    s_chgtest_no_power_count = 0;

    s_fault_active = 0;
    s_test_oov_count = 0;
    s_fan_enable = 0;
    s_fan_test_active = 0;
    s_fan_test_enable = 0;
    rt_memset(&s_faults, 0, sizeof(s_faults));
    s_ctrl.buck_enable = 0;
    s_ctrl.bypass_enable = 0;
    s_ctrl.pwm_counts = 0;

    pcb02_hw_apply_outputs(s_ctrl.buck_enable,
                           s_ctrl.bypass_enable,
                           s_fan_enable,
                           s_fault_active,
                           s_ctrl.pwm_counts,
                           pwm_max);
    pcb02_service_unlock();
}

void pcb02_service_mppttest(rt_uint8_t algo,
                            float vout_max,
                            float current_limit_a,
                            rt_uint16_t max_permille,
                            rt_uint16_t step_counts)
{
    rt_uint16_t pwm_max;

    if (vout_max <= 0.0f || current_limit_a <= 0.0f || max_permille == 0)
    {
        pcb02_service_set_enable(0);
        return;
    }

    if (vout_max > s_cfg.voltage_battery_max)
    {
        vout_max = s_cfg.voltage_battery_max;
    }
    if (current_limit_a > s_cfg.current_out_absolute)
    {
        current_limit_a = s_cfg.current_out_absolute;
    }
    if (max_permille > 970)
    {
        max_permille = 970;
    }
    if (step_counts == 0)
    {
        step_counts = 2;
    }
    if (step_counts > 64)
    {
        step_counts = 64;
    }

    pcb02_service_lock();
    pwm_max = pcb02_pwm_max_counts(&s_cfg);

    s_software_enable = 0;
    s_debug_pwm_active = 0;
    s_cvtest_active = 0;
    s_cvpid_active = 0;
    s_chgtest_active = 0;
    s_chgtest_state = PCB02_CHG_STATE_IDLE;
    s_chgtest_no_power_count = 0;
    s_mppttest_active = 1;
    s_mppttest_algo = (algo <= PCB02_MPPT_ALGO_ADAPT) ? algo : PCB02_MPPT_ALGO_PO;
    s_mppttest_vout_max = vout_max;
    s_mppttest_iout_limit = current_limit_a;
    s_mppttest_pwm_max_counts = (rt_uint16_t)(((rt_uint32_t)pwm_max * max_permille) / 1000U);
    s_mppttest_step_counts = step_counts;
    s_mppttest_prev_power = 0.0f;
    s_mppttest_prev_vin = 0.0f;
    s_mppttest_prev_iin = 0.0f;
    s_mppttest_direction = 1;

    s_fault_active = 0;
    s_test_oov_count = 0;
    s_fan_enable = 0;
    s_fan_test_active = 0;
    s_fan_test_enable = 0;
    rt_memset(&s_faults, 0, sizeof(s_faults));
    s_ctrl.buck_enable = 0;
    s_ctrl.bypass_enable = 0;
    s_ctrl.pwm_counts = 0;

    pcb02_hw_apply_outputs(s_ctrl.buck_enable,
                           s_ctrl.bypass_enable,
                           s_fan_enable,
                           s_fault_active,
                           s_ctrl.pwm_counts,
                           pwm_max);
    pcb02_service_unlock();
}
