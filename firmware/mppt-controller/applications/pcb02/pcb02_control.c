#include "pcb02_control.h"

static float clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

rt_uint16_t pcb02_pwm_max_counts(const pcb02_config_t *cfg)
{
    if (cfg->pwm_resolution_bits >= 16)
    {
        return 65535;
    }

    return (rt_uint16_t)((1U << cfg->pwm_resolution_bits) - 1U);
}

rt_uint16_t pcb02_pwm_limited_counts(const pcb02_config_t *cfg)
{
    float max_counts = (float)pcb02_pwm_max_counts(cfg);
    return (rt_uint16_t)((cfg->pwm_max_duty_percent * max_counts) / 100.0f);
}

rt_uint16_t pcb02_predictive_pwm_counts(const pcb02_config_t *cfg,
                                        const pcb02_measurements_t *meas)
{
    float basis_voltage;
    float pwm_counts;
    float pwm_limited = (float)pcb02_pwm_limited_counts(cfg);

    if (meas->voltage_input <= 0.0f)
    {
        return 0;
    }

    basis_voltage = meas->voltage_output;
    if (meas->voltage_output > cfg->voltage_battery_max)
    {
        basis_voltage = cfg->voltage_battery_max * 0.98f;
    }

    pwm_counts = (cfg->ppwm_margin_percent *
                  (float)pcb02_pwm_max_counts(cfg) *
                  basis_voltage) /
                 (100.0f * meas->voltage_input);

    return (rt_uint16_t)clamp_float(pwm_counts, 0.0f, pwm_limited);
}

rt_uint8_t pcb02_should_enable_backflow(const pcb02_config_t *cfg,
                                        const pcb02_measurements_t *meas)
{
    if (cfg->output_mode == 0)
    {
        return 1;
    }

    return (meas->voltage_input > (meas->voltage_output + cfg->voltage_dropout)) ? 1 : 0;
}

void pcb02_update_derived_measurements(const pcb02_config_t *cfg,
                                       pcb02_measurements_t *meas)
{
    if (meas->current_input < 0.0f)
    {
        meas->current_input = 0.0f;
    }

    if (meas->voltage_output <= 0.0f)
    {
        meas->current_output = 0.0f;
    }

    meas->power_input = meas->voltage_input * meas->current_input;
    meas->power_output = meas->voltage_output * meas->current_output;

    if (meas->power_input > 0.001f)
    {
        meas->buck_efficiency = (meas->power_output / meas->power_input) * 100.0f;
    }
    else
    {
        meas->buck_efficiency = 0.0f;
    }

    if (cfg->voltage_battery_max > cfg->voltage_battery_min)
    {
        meas->battery_percent =
            ((meas->voltage_output - cfg->voltage_battery_min) /
             (cfg->voltage_battery_max - cfg->voltage_battery_min)) * 101.0f;
        meas->battery_percent = clamp_float(meas->battery_percent, 0.0f, 100.0f);
    }
    else
    {
        meas->battery_percent = 0.0f;
    }
}

rt_uint8_t pcb02_eval_protection(const pcb02_config_t *cfg,
                                 const pcb02_measurements_t *meas,
                                 pcb02_faults_t *faults)
{
    rt_uint8_t err = 0;

    faults->over_temperature = (meas->temperature_c > cfg->temperature_max_c) ? 1 : 0;
    faults->input_over_current = (meas->current_input > cfg->current_in_absolute) ? 1 : 0;
    faults->output_over_current = (meas->current_output > cfg->current_out_absolute) ? 1 : 0;
    faults->output_over_voltage =
        (meas->voltage_output > (cfg->voltage_battery_max + cfg->voltage_battery_thresh)) ? 1 : 0;
    faults->fatal_low_voltage =
        ((meas->voltage_input < cfg->vin_system_min) &&
         (meas->voltage_output < cfg->vin_system_min)) ? 1 : 0;

    if (cfg->output_mode == 0)
    {
        faults->battery_not_connected = 0;
        faults->recovery_requested = 0;
        faults->input_under_voltage =
            (meas->voltage_input < (cfg->voltage_battery_max + cfg->voltage_dropout)) ? 1 : 0;
    }
    else
    {
        faults->battery_not_connected =
            (meas->voltage_output < cfg->vin_system_min) ? 1 : 0;
        faults->input_under_voltage =
            (meas->voltage_input < (cfg->voltage_battery_max + cfg->voltage_dropout)) ? 1 : 0;
        faults->recovery_requested = faults->input_under_voltage;
    }

    err += faults->over_temperature;
    err += faults->input_over_current;
    err += faults->output_over_current;
    err += faults->output_over_voltage;
    err += faults->fatal_low_voltage;
    err += faults->input_under_voltage;
    err += faults->battery_not_connected;
    faults->error_count = err;

    return err;
}

void pcb02_control_state_init(pcb02_control_state_t *state)
{
    rt_memset(state, 0, sizeof(*state));
}

rt_uint16_t pcb02_charging_step(const pcb02_config_t *cfg,
                                const pcb02_measurements_t *meas,
                                const pcb02_faults_t *faults,
                                pcb02_control_state_t *state)
{
    rt_int32_t pwm = state->pwm_counts;
    rt_uint16_t pwm_limited = pcb02_pwm_limited_counts(cfg);

    state->bypass_enable = pcb02_should_enable_backflow(cfg, meas);
    state->recovery_requested = faults->recovery_requested;

    if (faults->error_count > 0 || state->charging_pause)
    {
        state->buck_enable = 0;
        state->pwm_counts = 0;
        return 0;
    }

    if (state->recovery_requested)
    {
        state->recovery_requested = 0;
        state->buck_enable = 0;
        state->predictive_pwm_counts = pcb02_predictive_pwm_counts(cfg, meas);
        state->pwm_counts = state->predictive_pwm_counts;
        state->power_input_prev = meas->power_input;
        state->voltage_input_prev = meas->voltage_input;
        return state->pwm_counts;
    }

    if (cfg->mppt_mode == 0)
    {
        if (meas->current_output > cfg->current_charging_max)
        {
            pwm--;
        }
        else if (meas->voltage_output > cfg->voltage_battery_max)
        {
            pwm--;
        }
        else if (meas->voltage_output < cfg->voltage_battery_max)
        {
            pwm++;
        }
    }
    else
    {
        if (meas->current_output > cfg->current_charging_max)
        {
            pwm--;
        }
        else if (meas->voltage_output > cfg->voltage_battery_max)
        {
            pwm--;
        }
        else
        {
            if ((meas->current_output > 0.1f) &&
                (meas->voltage_input >= (meas->voltage_output + cfg->voltage_dropout + 1.0f)))
            {
                if ((meas->power_input > state->power_input_prev) &&
                    (meas->voltage_input > state->voltage_input_prev))
                {
                    pwm--;
                }
                else if ((meas->power_input > state->power_input_prev) &&
                         (meas->voltage_input < state->voltage_input_prev))
                {
                    pwm++;
                }
                else if ((meas->power_input < state->power_input_prev) &&
                         (meas->voltage_input > state->voltage_input_prev))
                {
                    pwm++;
                }
                else if ((meas->power_input < state->power_input_prev) &&
                         (meas->voltage_input < state->voltage_input_prev))
                {
                    pwm--;
                }
                else if (meas->voltage_output > cfg->voltage_battery_max)
                {
                    pwm--;
                }
                else if (meas->voltage_output < cfg->voltage_battery_max)
                {
                    pwm++;
                }
            }
            else
            {
                pwm--;
            }

            if (meas->current_output <= 0.0f)
            {
                pwm += 2;
            }
        }
    }

    if (cfg->output_mode != 0)
    {
        state->predictive_pwm_counts = pcb02_predictive_pwm_counts(cfg, meas);
        pwm = (rt_int32_t)clamp_float((float)pwm,
                                      (float)state->predictive_pwm_counts,
                                      (float)pwm_limited);
    }
    else
    {
        pwm = (rt_int32_t)clamp_float((float)pwm, 0.0f, (float)pwm_limited);
    }

    state->buck_enable = 1;
    state->pwm_counts = (rt_uint16_t)pwm;
    state->power_input_prev = meas->power_input;
    state->voltage_input_prev = meas->voltage_input;

    return state->pwm_counts;
}
