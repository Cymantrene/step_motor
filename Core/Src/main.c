/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "axis.h"
#include "driver.h"
#include "safety.h"

#include "potentiometer.h"
#include "ui.h"
#include "estop.h"

#include "buttons.h"
#include "menu.h"
#include <stdint.h>
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_iwdg.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define USE_IWDG 1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc2;

I2C_HandleTypeDef hi2c1;

#if USE_IWDG
IWDG_HandleTypeDef hiwdg;
#endif


TIM_HandleTypeDef htim6;

/* USER CODE BEGIN PV */
Axis axis1;
Axis axis2;
Driver drv1;
Driver drv2;
Safety safety;

Potentiometer pot;
static uint16_t pot_value = 0;
static uint32_t speed_update_tick = 0;

Buttons btns;
Menu menu;





/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM6_Init(void);
static void MX_ADC2_Init(void);
static void MX_I2C1_Init(void);
#if USE_IWDG
static void MX_IWDG_Init(void);
#endif
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
	// Enable DWT cycle counter for microsecond delays (Cortex-M4)
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM6_Init();
  MX_ADC2_Init();
  MX_I2C1_Init();
#if USE_IWDG
  MX_IWDG_Init();
#endif
  /* USER CODE BEGIN 2 */
	/* Дать OLED чуть “проснуться” после старта питания */
	HAL_Delay(50);

	/* === I2C scan === */
	uint8_t found = 0;
	uint8_t a;

	for (a = 1; a < 127; a++) {
		if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t) (a << 1), 2, 20)
				== HAL_OK) {
			found = a;   // ждём 0x3C
			break;
		}
	}

	/* === индикация LED_TEST === */
	if (found == 0x3C) {
		/* OLED найден: моргнём 3 раза и продолжаем запуск */
		for (int i = 0; i < 3; i++) {
			HAL_GPIO_TogglePin(LED_TEST_GPIO_Port, LED_TEST_Pin);
			HAL_Delay(120);
			HAL_GPIO_TogglePin(LED_TEST_GPIO_Port, LED_TEST_Pin);
			HAL_Delay(120);
		}
	} else {
		/* OLED НЕ найден: медленно мигаем и стопоримся (чтобы сразу было видно проблему) */
		while (1) {
			HAL_GPIO_TogglePin(LED_TEST_GPIO_Port, LED_TEST_Pin);
			HAL_Delay(500);

#if USE_IWDG
			HAL_IWDG_Refresh(&hiwdg);
#endif
		}

	}

	/* дальше — твой обычный запуск */
	Axis_Init(&axis1,
	STEP_M1_GPIO_Port, STEP_M1_Pin,
	DIR_M1_GPIO_Port, DIR_M1_Pin, MICROSTEP_64);

	Axis_Init(&axis2,
	STEP_M2_GPIO_Port, STEP_M2_Pin,
	DIR_M2_GPIO_Port, DIR_M2_Pin, MICROSTEP_64);

	Driver_Init(&drv1, EN_M1_GPIO_Port, EN_M1_Pin, 0);
	Driver_Init(&drv2, EN_M2_GPIO_Port, EN_M2_Pin, 0);

	Driver_Enable(&drv1);
	Driver_Enable(&drv2);

	Safety_Init(&safety, ESTOP_GPIO_Port, ESTOP_Pin, &drv1, &drv2);

	Potentiometer_Init(&pot, &hadc2, 0, 4095);

	/* ВАЖНО: UI_Init теперь точно дойдёт и покажет "UI READY" */
	UI_Init(&hi2c1);

	Buttons_Init(&btns);
	Menu_Init(&menu, &axis1, &axis2, &drv1, &drv2);

	HAL_TIM_Base_Start_IT(&htim6);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		/* ===== аварийная остановка ===== */
		/* ===== аварийная остановка (latched fault, без "заморозки" MCU) ===== */
		static uint8_t estop_handled = 0;

		if (Estop_IsActive()) {

			if (!estop_handled) {
				/* 1) остановить генерацию STEP */
				HAL_TIM_Base_Stop_IT(&htim6);

				/* 2) программно остановить оси */
				Axis_EmergencyStop(&axis1);
				Axis_EmergencyStop(&axis2);

				/* 3) прижать STEP/DIR в 0 */
				HAL_GPIO_WritePin(STEP_M1_GPIO_Port, STEP_M1_Pin,
						GPIO_PIN_RESET);
				HAL_GPIO_WritePin(STEP_M2_GPIO_Port, STEP_M2_Pin,
						GPIO_PIN_RESET);
				HAL_GPIO_WritePin(DIR_M1_GPIO_Port, DIR_M1_Pin, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(DIR_M2_GPIO_Port, DIR_M2_Pin, GPIO_PIN_RESET);

				/* 4) отключить драйверы */
				Driver_DisableAll();

				/* помечаем что уже отработали один раз */
				estop_handled = 1;
			}

		} else {
			/* если E-STOP отпущен — разрешаем отработать снова при следующем срабатывании */
			estop_handled = 0;
		}




		/* ===== обычная работа ===== */
		Safety_Update(&safety);

		Buttons_Update(&btns);
		Menu_Update(&menu, &btns);   // меню само рисует UI

		/* ===== выполнение команды START от меню (one-shot) ===== */
		if (menu.start_request) {
			menu.start_request = 0;  // обязательно сбросить сразу

			if (!Estop_IsActive()) {
				int32_t steps = menu.steps_full;

				if (menu.link_motors) {
					Axis_MoveSteps(&axis1, steps);
					Axis_MoveSteps(&axis2, steps);
				} else {
					if (menu.selected_axis == 0)
						Axis_MoveSteps(&axis1, steps);
					else
						Axis_MoveSteps(&axis2, steps);
				}
			}
		}

		Potentiometer_Update(&pot);

		/* ===== скорость раз в 5 мс (max_speed для профиля) ===== */
		if (HAL_GetTick() - speed_update_tick >= 5U) {
			speed_update_tick = HAL_GetTick();

			pot_value = Potentiometer_GetValue(&pot);

			uint32_t max_usps =
			AXIS_MIN_SPEED_USTEP_S
					+ ((uint32_t) pot_value
							* (AXIS_MAX_SPEED_USTEP_S - AXIS_MIN_SPEED_USTEP_S))
							/ 4095U;

			axis1.max_speed = max_usps;
			axis2.max_speed = max_usps;
		}
#if USE_IWDG
		HAL_IWDG_Refresh(&hiwdg);
#endif
	}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	// сюда можно добавить что-то, но обычно пусто
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.GainCompensation = 0;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = ENABLE;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc2.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_17;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x30A0A7FB;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
#if USE_IWDG
static void MX_IWDG_Init(void)
{
  /* USER CODE BEGIN IWDG_Init 0 */
  /* USER CODE END IWDG_Init 0 */

  hiwdg.Instance = IWDG;
	hiwdg.Init.Prescaler = IWDG_PRESCALER_64;     // было _4
	hiwdg.Init.Window = IWDG_WINDOW_DISABLE;   // ок
	hiwdg.Init.Reload = 1000;                  // ~2 сек при LSI~32kHz

  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN IWDG_Init 2 */
  /* USER CODE END IWDG_Init 2 */
}
#endif



/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 169;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 49;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, EN_M1_Pin|EN_M2_Pin|LED_TEST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, STEP_M1_Pin|DIR_M1_Pin|STEP_M2_Pin|DIR_M2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : ESTOP_Pin */
  GPIO_InitStruct.Pin = ESTOP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(ESTOP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : STEP1_IN_Pin STEP2_IN_Pin */
  GPIO_InitStruct.Pin = STEP1_IN_Pin|STEP2_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : DIR1_IN_Pin DIR2_IN_Pin */
  GPIO_InitStruct.Pin = DIR1_IN_Pin|DIR2_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_OK_Pin BTN_BACK_Pin */
  GPIO_InitStruct.Pin = BTN_OK_Pin|BTN_BACK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : EN_M1_Pin EN_M2_Pin */
  GPIO_InitStruct.Pin = EN_M1_Pin|EN_M2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_DOWN_Pin BTN_UP_Pin */
  GPIO_InitStruct.Pin = BTN_DOWN_Pin|BTN_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : STEP_M1_Pin DIR_M1_Pin STEP_M2_Pin DIR_M2_Pin */
  GPIO_InitStruct.Pin = STEP_M1_Pin|DIR_M1_Pin|STEP_M2_Pin|DIR_M2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_TEST_Pin */
  GPIO_InitStruct.Pin = LED_TEST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_TEST_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM6) {
		Axis_Tick(&axis1);
		Axis_Tick(&axis2);
	}
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == ESTOP_Pin) {
		Estop_Trigger();
		return;
	}

	// Внешние STEP/DIR пока НЕ используем (режим Axis)
	/*
	 if (GPIO_Pin == STEP1_IN_Pin) {
	 StepDir_OnStepIRQ(&sd1);
	 } else if (GPIO_Pin == STEP2_IN_Pin) {
	 StepDir_OnStepIRQ(&sd2);
	 }
	 */
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
