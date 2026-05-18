/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
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
#include "stm32f1xx_hal.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define V_OUT__Pin GPIO_PIN_0
#define V_OUT__GPIO_Port GPIOA
#define V_OUT_A1_Pin GPIO_PIN_1
#define V_OUT_A1_GPIO_Port GPIOA
#define DAC_CS_Pin GPIO_PIN_4
#define DAC_CS_GPIO_Port GPIOA
#define DAC_SCK_Pin GPIO_PIN_5
#define DAC_SCK_GPIO_Port GPIOA
#define DAC_MOSI_Pin GPIO_PIN_7
#define DAC_MOSI_GPIO_Port GPIOA
#define SW_IN_Pin GPIO_PIN_10
#define SW_IN_GPIO_Port GPIOB
#define BUCK_EN_Pin GPIO_PIN_11
#define BUCK_EN_GPIO_Port GPIOB
#define KEY_1_Pin GPIO_PIN_12
#define KEY_1_GPIO_Port GPIOB
#define KEY_2_Pin GPIO_PIN_13
#define KEY_2_GPIO_Port GPIOB
#define KEY_3_Pin GPIO_PIN_14
#define KEY_3_GPIO_Port GPIOB
#define ENC_SW_Pin GPIO_PIN_8
#define ENC_SW_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
