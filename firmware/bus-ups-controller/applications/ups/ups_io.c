#include "ups_io.h"

#include <rtdevice.h>
#include <stdlib.h>
#include <string.h>
#include "stm32g4xx_hal.h"

#define UPS_IO_DAC_REF_MV      3300U
#define UPS_IO_DAC_MAX_CODE    4095U

static DAC_HandleTypeDef s_dac1;
static TIM_HandleTypeDef s_tim4;
static rt_uint8_t s_dac_ready;
static rt_uint8_t s_encoder_ready;
static rt_uint16_t s_cc_mv;
static rt_uint16_t s_cv_mv;

static const rt_base_t s_output_pins[UPS_IO_OUTPUT_MAX] =
{
    UPS_IO_PIN_CSS,
    UPS_IO_PIN_UVCH1,
    UPS_IO_PIN_UVCH2,
    UPS_IO_PIN_UVCH3,
};

static rt_uint32_t mv_to_dac_code(rt_uint16_t mv)
{
    rt_uint32_t limited = mv;

    if (limited > UPS_IO_DAC_REF_MV)
    {
        limited = UPS_IO_DAC_REF_MV;
    }

    return (limited * UPS_IO_DAC_MAX_CODE) / UPS_IO_DAC_REF_MV;
}

void HAL_DAC_MspInit(DAC_HandleTypeDef *hdac)
{
    GPIO_InitTypeDef gpio = {0};

    if (hdac->Instance != DAC1)
    {
        return;
    }

    __HAL_RCC_DAC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void HAL_DAC_MspDeInit(DAC_HandleTypeDef *hdac)
{
    if (hdac->Instance != DAC1)
    {
        return;
    }

    __HAL_RCC_DAC1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_4 | GPIO_PIN_5);
}

void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef gpio = {0};

    if (htim->Instance != TIM4)
    {
        return;
    }

    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(GPIOB, &gpio);
}

void HAL_TIM_Encoder_MspDeInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM4)
    {
        return;
    }

    __HAL_RCC_TIM4_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
}

static rt_err_t ups_io_dac_init(void)
{
    DAC_ChannelConfTypeDef cfg = {0};

    s_dac1.Instance = DAC1;
    if (HAL_DAC_Init(&s_dac1) != HAL_OK)
    {
        return -RT_ERROR;
    }

    cfg.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
    cfg.DAC_DMADoubleDataMode = DISABLE;
    cfg.DAC_SignedFormat = DISABLE;
    cfg.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
    cfg.DAC_Trigger = DAC_TRIGGER_NONE;
    cfg.DAC_Trigger2 = DAC_TRIGGER_NONE;
    cfg.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    cfg.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
    cfg.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
    cfg.DAC_TrimmingValue = 0;

    if (HAL_DAC_ConfigChannel(&s_dac1, &cfg, DAC_CHANNEL_1) != HAL_OK ||
        HAL_DAC_ConfigChannel(&s_dac1, &cfg, DAC_CHANNEL_2) != HAL_OK)
    {
        return -RT_ERROR;
    }

    if (HAL_DAC_Start(&s_dac1, DAC_CHANNEL_1) != HAL_OK ||
        HAL_DAC_Start(&s_dac1, DAC_CHANNEL_2) != HAL_OK)
    {
        return -RT_ERROR;
    }

    (void)HAL_DAC_SetValue(&s_dac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0);
    (void)HAL_DAC_SetValue(&s_dac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, 0);
    s_dac_ready = 1;

    return RT_EOK;
}

static rt_err_t ups_io_encoder_init(void)
{
    TIM_Encoder_InitTypeDef cfg = {0};

    s_tim4.Instance = TIM4;
    s_tim4.Init.Prescaler = 0;
    s_tim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_tim4.Init.Period = 0xFFFF;
    s_tim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_tim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    cfg.EncoderMode = TIM_ENCODERMODE_TI12;
    cfg.IC1Polarity = TIM_ICPOLARITY_RISING;
    cfg.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    cfg.IC1Prescaler = TIM_ICPSC_DIV1;
    cfg.IC1Filter = 6;
    cfg.IC2Polarity = TIM_ICPOLARITY_RISING;
    cfg.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    cfg.IC2Prescaler = TIM_ICPSC_DIV1;
    cfg.IC2Filter = 6;

    if (HAL_TIM_Encoder_Init(&s_tim4, &cfg) != HAL_OK)
    {
        return -RT_ERROR;
    }

    if (HAL_TIM_Encoder_Start(&s_tim4, TIM_CHANNEL_ALL) != HAL_OK)
    {
        return -RT_ERROR;
    }

    __HAL_TIM_SET_COUNTER(&s_tim4, 0);
    s_encoder_ready = 1;

    return RT_EOK;
}

int ups_io_init(void)
{
    rt_size_t i;

    for (i = 0; i < UPS_IO_OUTPUT_MAX; i++)
    {
        rt_pin_mode(s_output_pins[i], PIN_MODE_OUTPUT);
        rt_pin_write(s_output_pins[i], PIN_LOW);
    }

    if (ups_io_dac_init() != RT_EOK)
    {
        rt_kprintf("ups_io: dac init failed\r\n");
    }

    if (ups_io_encoder_init() != RT_EOK)
    {
        rt_kprintf("ups_io: ec11 encoder init failed, pins can still be used as GPIO inputs\r\n");
        rt_pin_mode(UPS_IO_PIN_EC11_A, PIN_MODE_INPUT_PULLUP);
        rt_pin_mode(UPS_IO_PIN_EC11_B, PIN_MODE_INPUT_PULLUP);
    }

    rt_kprintf("ups_io: ready dac=%u enc=%u\r\n", s_dac_ready, s_encoder_ready);
    return RT_EOK;
}
INIT_DEVICE_EXPORT(ups_io_init);

rt_err_t ups_io_set_output(ups_io_output_t output, rt_uint8_t level)
{
    if (output >= UPS_IO_OUTPUT_MAX)
    {
        return -RT_EINVAL;
    }

    rt_pin_write(s_output_pins[output], level ? PIN_HIGH : PIN_LOW);
    return RT_EOK;
}

rt_err_t ups_io_set_cc_mv(rt_uint16_t mv)
{
    if (!s_dac_ready)
    {
        return -RT_ERROR;
    }

    s_cc_mv = mv;
    return (HAL_DAC_SetValue(&s_dac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, mv_to_dac_code(mv)) == HAL_OK) ? RT_EOK : -RT_ERROR;
}

rt_err_t ups_io_set_cv_mv(rt_uint16_t mv)
{
    if (!s_dac_ready)
    {
        return -RT_ERROR;
    }

    s_cv_mv = mv;
    return (HAL_DAC_SetValue(&s_dac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, mv_to_dac_code(mv)) == HAL_OK) ? RT_EOK : -RT_ERROR;
}

rt_int32_t ups_io_get_encoder(void)
{
    if (!s_encoder_ready)
    {
        rt_uint8_t a = rt_pin_read(UPS_IO_PIN_EC11_A);
        rt_uint8_t b = rt_pin_read(UPS_IO_PIN_EC11_B);
        return (rt_int32_t)((a << 1) | b);
    }

    return (rt_int32_t)(int16_t)__HAL_TIM_GET_COUNTER(&s_tim4);
}

static const char *output_name(ups_io_output_t output)
{
    static const char *names[UPS_IO_OUTPUT_MAX] = {"css", "uvch1", "uvch2", "uvch3"};
    return (output < UPS_IO_OUTPUT_MAX) ? names[output] : "unknown";
}

static rt_int32_t parse_output(const char *name)
{
    rt_int32_t i;

    for (i = 0; i < UPS_IO_OUTPUT_MAX; i++)
    {
        if (strcmp(name, output_name((ups_io_output_t)i)) == 0)
        {
            return i;
        }
    }

    return -1;
}

static void ups_io_help(void)
{
    rt_kprintf("upsio commands:\r\n");
    rt_kprintf("  upsio status\r\n");
    rt_kprintf("  upsio out <css|uvch1|uvch2|uvch3> <0|1>\r\n");
    rt_kprintf("  upsio dac <cc|cv> <mv>\r\n");
    rt_kprintf("  upsio enc\r\n");
}

static void upsio(int argc, char **argv)
{
    if (argc < 2)
    {
        ups_io_help();
        return;
    }

    if (strcmp(argv[1], "status") == 0)
    {
        rt_kprintf("ups_io: dac=%u enc=%u enc_count=%d cc=%umV cv=%umV\r\n", s_dac_ready, s_encoder_ready, ups_io_get_encoder(), s_cc_mv, s_cv_mv);
        return;
    }

    if (strcmp(argv[1], "out") == 0)
    {
        rt_int32_t output;
        rt_uint8_t level;

        if (argc < 4)
        {
            rt_kprintf("usage: upsio out <css|uvch1|uvch2|uvch3> <0|1>\r\n");
            return;
        }

        output = parse_output(argv[2]);
        if (output < 0)
        {
            rt_kprintf("unknown output: %s\r\n", argv[2]);
            return;
        }

        level = atoi(argv[3]) ? 1 : 0;
        (void)ups_io_set_output((ups_io_output_t)output, level);
        rt_kprintf("ups_io: %s=%u\r\n", output_name((ups_io_output_t)output), level);
        return;
    }

    if (strcmp(argv[1], "dac") == 0)
    {
        rt_uint16_t mv;
        rt_err_t ret;

        if (argc < 4)
        {
            rt_kprintf("usage: upsio dac <cc|cv> <mv>\r\n");
            return;
        }

        mv = (rt_uint16_t)atoi(argv[3]);
        if (strcmp(argv[2], "cc") == 0)
        {
            ret = ups_io_set_cc_mv(mv);
        }
        else if (strcmp(argv[2], "cv") == 0)
        {
            ret = ups_io_set_cv_mv(mv);
        }
        else
        {
            rt_kprintf("unknown dac channel: %s\r\n", argv[2]);
            return;
        }

        rt_kprintf("ups_io: dac %s %umV %s\r\n", argv[2], mv, ret == RT_EOK ? "ok" : "failed");
        return;
    }

    if (strcmp(argv[1], "enc") == 0)
    {
        rt_kprintf("ups_io: enc_count=%d\r\n", ups_io_get_encoder());
        return;
    }

    ups_io_help();
}
MSH_CMD_EXPORT(upsio, UPS board low-level IO test command);
