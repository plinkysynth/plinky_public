/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
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
#include "stm32l4xx_hal.h"

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
#define CVOUT_TRIG_Pin GPIO_PIN_6
#define CVOUT_TRIG_GPIO_Port GPIOE
#define IO1_Pin GPIO_PIN_13
#define IO1_GPIO_Port GPIOC
#define IO2_Pin GPIO_PIN_14
#define IO2_GPIO_Port GPIOC
#define IO3_Pin GPIO_PIN_15
#define IO3_GPIO_Port GPIOC
#define CVOUT_CLK_Pin GPIO_PIN_0
#define CVOUT_CLK_GPIO_Port GPIOB
#define CLKIN_Pin GPIO_PIN_2
#define CLKIN_GPIO_Port GPIOB
#define CLKIN_EXTI_IRQn EXTI2_IRQn
#define SENSE1_Pin GPIO_PIN_8
#define SENSE1_GPIO_Port GPIOE
#define SENSE2_Pin GPIO_PIN_15
#define SENSE2_GPIO_Port GPIOE
#define N5_Pin GPIO_PIN_15
#define N5_GPIO_Port GPIOD
#define DEBUG_Pin GPIO_PIN_8
#define DEBUG_GPIO_Port GPIOA
#define N4_Pin GPIO_PIN_0
#define N4_GPIO_Port GPIOD
#define N1_Pin GPIO_PIN_2
#define N1_GPIO_Port GPIOD
#define N2_Pin GPIO_PIN_5
#define N2_GPIO_Port GPIOD
#define N3_Pin GPIO_PIN_7
#define N3_GPIO_Port GPIOD
#define CS2_Pin GPIO_PIN_0
#define CS2_GPIO_Port GPIOE
#define CS1_Pin GPIO_PIN_1
#define CS1_GPIO_Port GPIOE
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
