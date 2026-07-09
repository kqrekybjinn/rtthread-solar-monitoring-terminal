#include "pcb02_service.h"

#include <rtdevice.h>

#include "pcb02_hw.h"
#include "pcb02_sensors.h"

static pcb02_config_t s_cfg;
static pcb02_measurements_t s_meas;
static pcb02_faults_t s_faults;
static pcb02_control_state_t s_ctrl;
static struct rt_mutex s_lock;
static rt_uint8_t s_lock_ready;
static rt_uint8_t s_sensor_ready;
static rt_uint8_t s_software_enable;
static rt_uint8_t s_hardware_chg_en;
static rt_uint8_t s_fault_active;
static rt_uint8_t s_fan_enable;
static rt_uint32_t s_last_energy_tick;

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
    s_ctrl.buck_enable = 0;
    s_ctrl.bypass_enable = 0;
    s_ctrl.pwm_counts = 0;
    s_fan_enable = 0;
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

    ret = pcb02_hw_init(&s_cfg);
    if (ret != RT_EOK)
    {
        return ret;
    }

    s_sensor_ready = (pcb02_sensors_init(&s_cfg) == RT_EOK) ? 1 : 0;
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
    rt_err_t ret;

    pcb02_service_lock();
    cfg_snapshot = s_cfg;
    software_enable = s_software_enable;
    sensor_ready = s_sensor_ready;
    pcb02_service_unlock();

    hardware_chg_en = (rt_pin_read(PCB02_PIN_CHG_EN) == PIN_HIGH) ? 1 : 0;

    if (!software_enable || !hardware_chg_en || !sensor_ready)
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
        s_sensor_ready = 0;
        pcb02_service_apply_safe_state();
        pcb02_service_unlock();
        return ret;
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
        s_fan_enable = (s_cfg.enable_fan && s_meas.temperature_c >= s_cfg.temperature_fan_c) ? 1 : 0;

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
    pcb02_service_unlock();
}

void pcb02_service_set_enable(rt_uint8_t enable)
{
    pcb02_service_lock();
    s_software_enable = enable ? 1 : 0;
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
    s_sensor_ready = 1;
    pcb02_control_state_init(&s_ctrl);
    s_last_energy_tick = rt_tick_get();
    pcb02_service_unlock();
}

const pcb02_config_t *pcb02_service_get_config(void)
{
    return &s_cfg;
}
