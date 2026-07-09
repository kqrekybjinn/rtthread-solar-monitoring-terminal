#ifndef UPS_IO_H
#define UPS_IO_H

#include <rtthread.h>
#include <drv_gpio.h>

#define UPS_IO_PIN_CSS         GET_PIN(B, 11)
#define UPS_IO_PIN_UVCH1       GET_PIN(B, 12)
#define UPS_IO_PIN_UVCH2       GET_PIN(B, 13)
#define UPS_IO_PIN_UVCH3       GET_PIN(B, 14)

#define UPS_IO_PIN_VIN_1_ADC   GET_PIN(C, 4)
#define UPS_IO_PIN_NTC_ADC     GET_PIN(C, 5)
#define UPS_IO_PIN_DAC_CC      GET_PIN(A, 4)
#define UPS_IO_PIN_DAC_CV      GET_PIN(A, 5)
#define UPS_IO_PIN_EC11_A      GET_PIN(B, 6)
#define UPS_IO_PIN_EC11_B      GET_PIN(B, 7)
#define UPS_IO_PIN_I2C_SCL     GET_PIN(B, 8)
#define UPS_IO_PIN_I2C_SDA     GET_PIN(B, 9)

#define UPS_IO_ADC_DEV_NAME    "adc2"
#define UPS_IO_ADC_CH_VIN_1    5
#define UPS_IO_ADC_CH_NTC      11

typedef enum ups_io_output
{
    UPS_IO_OUTPUT_CSS = 0,
    UPS_IO_OUTPUT_UVCH1,
    UPS_IO_OUTPUT_UVCH2,
    UPS_IO_OUTPUT_UVCH3,
    UPS_IO_OUTPUT_MAX
} ups_io_output_t;

int ups_io_init(void);
rt_err_t ups_io_set_output(ups_io_output_t output, rt_uint8_t level);
rt_err_t ups_io_set_cc_mv(rt_uint16_t mv);
rt_err_t ups_io_set_cv_mv(rt_uint16_t mv);
rt_int32_t ups_io_get_encoder(void);

#endif



