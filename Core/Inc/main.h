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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ESTOP_Pin GPIO_PIN_13
#define ESTOP_GPIO_Port GPIOC
#define ESTOP_EXTI_IRQn EXTI15_10_IRQn
#define STEP1_IN_Pin GPIO_PIN_0
#define STEP1_IN_GPIO_Port GPIOA
#define DIR1_IN_Pin GPIO_PIN_1
#define DIR1_IN_GPIO_Port GPIOA
#define STEP2_IN_Pin GPIO_PIN_2
#define STEP2_IN_GPIO_Port GPIOA
#define DIR2_IN_Pin GPIO_PIN_3
#define DIR2_IN_GPIO_Port GPIOA
#define POT_ADC_Pin GPIO_PIN_4
#define POT_ADC_GPIO_Port GPIOA
#define BTN_OK_Pin GPIO_PIN_5
#define BTN_OK_GPIO_Port GPIOA
#define BTN_BACK_Pin GPIO_PIN_6
#define BTN_BACK_GPIO_Port GPIOA
#define EN_M1_Pin GPIO_PIN_0
#define EN_M1_GPIO_Port GPIOB
#define EN_M2_Pin GPIO_PIN_1
#define EN_M2_GPIO_Port GPIOB
#define BTN_DOWN_Pin GPIO_PIN_11
#define BTN_DOWN_GPIO_Port GPIOB
#define BTN_UP_Pin GPIO_PIN_12
#define BTN_UP_GPIO_Port GPIOB
#define STEP_M1_Pin GPIO_PIN_8
#define STEP_M1_GPIO_Port GPIOA
#define DIR_M1_Pin GPIO_PIN_9
#define DIR_M1_GPIO_Port GPIOA
#define STEP_M2_Pin GPIO_PIN_10
#define STEP_M2_GPIO_Port GPIOA
#define DIR_M2_Pin GPIO_PIN_11
#define DIR_M2_GPIO_Port GPIOA
#define I2C1_SCL_Pin GPIO_PIN_15
#define I2C1_SCL_GPIO_Port GPIOA
#define LED_TEST_Pin GPIO_PIN_4
#define LED_TEST_GPIO_Port GPIOB
#define I2C1_SDA_Pin GPIO_PIN_7
#define I2C1_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
