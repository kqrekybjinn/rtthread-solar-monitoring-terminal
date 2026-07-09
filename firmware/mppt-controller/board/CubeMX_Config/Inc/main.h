/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define INA226_PV_ALERT_FROM_ESP32_GPIO35_Pin GPIO_PIN_0
#define INA226_PV_ALERT_FROM_ESP32_GPIO35_GPIO_Port GPIOC
#define ADC_NTC_MOS_FROM_ESP32_GPIO39_SENSOR_VN_Pin GPIO_PIN_0
#define ADC_NTC_MOS_FROM_ESP32_GPIO39_SENSOR_VN_GPIO_Port GPIOA
#define ADC_NTC_IND_NEW_NTC2_Pin GPIO_PIN_1
#define ADC_NTC_IND_NEW_NTC2_GPIO_Port GPIOA
#define NUCLEO_LD2_TEST_NEW_Pin GPIO_PIN_5
#define NUCLEO_LD2_TEST_NEW_GPIO_Port GPIOA
#define ADC_AUX_12V_NEW_MON_Pin GPIO_PIN_4
#define ADC_AUX_12V_NEW_MON_GPIO_Port GPIOC
#define ADC_3V3_MON_NEW_MON_Pin GPIO_PIN_5
#define ADC_3V3_MON_NEW_MON_GPIO_Port GPIOC
#define FAN_EN_FROM_ESP32_GPIO16_FAN_Pin GPIO_PIN_0
#define FAN_EN_FROM_ESP32_GPIO16_FAN_GPIO_Port GPIOB
#define LED_RUN_FROM_ESP32_GPIO2_LED_Pin GPIO_PIN_1
#define LED_RUN_FROM_ESP32_GPIO2_LED_GPIO_Port GPIOB
#define LED_FAULT_NEW_Pin GPIO_PIN_2
#define LED_FAULT_NEW_GPIO_Port GPIOB
#define DRV_EN_FROM_ESP32_GPIO32_BUCK_EN_Pin GPIO_PIN_12
#define DRV_EN_FROM_ESP32_GPIO32_BUCK_EN_GPIO_Port GPIOB
#define PWM_BUCK_N_RESERVE_NEW_Pin GPIO_PIN_13
#define PWM_BUCK_N_RESERVE_NEW_GPIO_Port GPIOB
#define BACKFLOW_EN_FROM_ESP32_GPIO27_BACKFLOW_MOSFET_Pin GPIO_PIN_6
#define BACKFLOW_EN_FROM_ESP32_GPIO27_BACKFLOW_MOSFET_GPIO_Port GPIOC
#define PWM_BUCK_FROM_ESP32_GPIO33_BUCK_IN_Pin GPIO_PIN_8
#define PWM_BUCK_FROM_ESP32_GPIO33_BUCK_IN_GPIO_Port GPIOA
#define FAULT_N_OD_NEW_FROM_PCB03_HARDWIRE_Pin GPIO_PIN_4
#define FAULT_N_OD_NEW_FROM_PCB03_HARDWIRE_GPIO_Port GPIOB
#define CAN_STB_Pin GPIO_PIN_10
#define CAN_STB_GPIO_Port GPIOA
#define FDCAN1_RX_NEW_CAN_TRANSCEIVER_RXD_Pin GPIO_PIN_11
#define FDCAN1_RX_NEW_CAN_TRANSCEIVER_RXD_GPIO_Port GPIOA
#define FDCAN1_TX_NEW_CAN_TRANSCEIVER_TXD_Pin GPIO_PIN_12
#define FDCAN1_TX_NEW_CAN_TRANSCEIVER_TXD_GPIO_Port GPIOA
#define CHG_EN_IN_NEW_FROM_PCB03_HARDWIRE_Pin GPIO_PIN_5
#define CHG_EN_IN_NEW_FROM_PCB03_HARDWIRE_GPIO_Port GPIOB
#define INA226_OUT_ALERT_FROM_ESP32_GPIO34_Pin GPIO_PIN_9
#define INA226_OUT_ALERT_FROM_ESP32_GPIO34_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
