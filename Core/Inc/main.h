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
#include "stm32c0xx_hal.h"

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
void LCD_Service(void);   // drains the LCD write queue; called once per tick from SysTick_Handler

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LCD_RW_Pin GPIO_PIN_4
#define LCD_RW_GPIO_Port GPIOA
#define LD1_Pin GPIO_PIN_5
#define LD1_GPIO_Port GPIOA
#define LCD_D6_Pin GPIO_PIN_4
#define LCD_D6_GPIO_Port GPIOC
#define LCD_D7_Pin GPIO_PIN_5
#define LCD_D7_GPIO_Port GPIOC
#define LCD_D0_Pin GPIO_PIN_0
#define LCD_D0_GPIO_Port GPIOB
#define LCD_D1_Pin GPIO_PIN_2
#define LCD_D1_GPIO_Port GPIOB
#define LCD_D5_Pin GPIO_PIN_11
#define LCD_D5_GPIO_Port GPIOB
#define LCD_RS_Pin GPIO_PIN_7
#define LCD_RS_GPIO_Port GPIOC
#define nLCD_ENB1_Pin GPIO_PIN_10
#define nLCD_ENB1_GPIO_Port GPIOA
#define nLCD_ENB2_Pin GPIO_PIN_15
#define nLCD_ENB2_GPIO_Port GPIOA
#define LD2_Pin GPIO_PIN_9
#define LD2_GPIO_Port GPIOC
#define LCD_D2_Pin GPIO_PIN_3
#define LCD_D2_GPIO_Port GPIOB
#define LCD_D3_Pin GPIO_PIN_4
#define LCD_D3_GPIO_Port GPIOB
#define LCD_D4_Pin GPIO_PIN_5
#define LCD_D4_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
