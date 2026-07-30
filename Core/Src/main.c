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
#include <stdio.h>
#include "retarget.h"
#include "program_info.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct{   //for generic port usae
  GPIO_TypeDef *port;
  uint16_t      pin;        // HAL GPIO_PIN_x mask
} SafeGPIO_t;

typedef struct{
  const SafeGPIO_t *pins;
  uint8_t           count;
} SafeGPIOBus_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
const SafeGPIO_t LCDPins[] =
{
  {GPIOB, GPIO_PIN_0},
  {GPIOB, GPIO_PIN_2},
  {GPIOB, GPIO_PIN_3},
  {GPIOB, GPIO_PIN_4},
  {GPIOB, GPIO_PIN_5},
  {GPIOB, GPIO_PIN_11},
  {GPIOC, GPIO_PIN_4},
  {GPIOC, GPIO_PIN_5},
};

SafeGPIOBus_t LCDBus;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void SafeGPIOBus_Init(SafeGPIOBus_t *bus,
                      const SafeGPIO_t *pinList,
                      uint8_t count);
static inline void GPIO_SetOutput(GPIO_TypeDef *port, uint16_t pin);
static inline void GPIO_SetInput(GPIO_TypeDef *port, uint16_t pin);
void SafeGPIOBus_Write(SafeGPIOBus_t *bus, uint32_t data);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// LCD driver derived from NewHaven Display (NHD-0440AZ) example code:
// https://support.newhavendisplay.com/hc/en-us/articles/4413994414231-NHD-0440AZ

// Busy-wait of a few hundred ns to ~1us at the ~12MHz core clock configured in
// SystemClock_Config (HSI/4). Cortex-M0+ has no DWT cycle counter, so this is a
// calibrated NOP-count loop rather than a precise timer, with margin above the
// HD44780's ~450-500ns minimum enable pulse width.
static inline void LCD_ShortDelay(void)
{
	for (volatile uint32_t i = 0; i < 20; i++) { __NOP(); }
}

// Strobes an LCD controller's enable line: idle low, pulse high, back to idle low.
// Replaces the original F-series TIM8 one-pulse-mode PWM trick with plain GPIO.
//
// The trailing HAL_Delay(2) is not part of the E pulse itself - it's the HD44780's
// required instruction execution time before the SAME controller can accept another
// command (~37-43us for most instructions, ~1.52ms for Clear Display/Return Home).
// lcd_init()/display() chain commands back-to-back with no delay of their own, so
// this has to cover the worst case (Clear Display) every time.
static void LCD_Strobe(GPIO_TypeDef *port, uint16_t pin)
{
	HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
	LCD_ShortDelay();
	HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
	HAL_Delay(2);
}

void command1(uint8_t InputData)	//command for LCD lines 1&2
{
	SafeGPIOBus_Write(&LCDBus, InputData);  //write data to LCD bus 
	HAL_GPIO_WritePin (LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET); //Set instr. register (RS) low for command
	HAL_GPIO_WritePin (LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET); //Set dir. register (RW) low for write
	LCD_Strobe(nLCD_ENB1_GPIO_Port, nLCD_ENB1_Pin); //Pulse ENB1 to strobe the data into LCD controller 1
}

void command2(uint8_t InputData)	//command for LCD lines 3&4
{
	SafeGPIOBus_Write(&LCDBus, InputData);  //write data to LCD bus
	HAL_GPIO_WritePin (LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET); //Set instr. register (RS) low for command
	HAL_GPIO_WritePin (LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET); //Set dir. register (RW) low for write
	LCD_Strobe(nLCD_ENB2_GPIO_Port, nLCD_ENB2_Pin); //Pulse ENB2 to strobe the data into LCD controller 2
}

void write1(uint8_t InputData)	//write data on lines 1&2
{
	SafeGPIOBus_Write(&LCDBus, InputData);  //write data to LCD bus
	HAL_GPIO_WritePin (LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_SET); //Set instr. register (RS) high for data
	HAL_GPIO_WritePin (LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET); //Set dir. register (RW) low for write
	LCD_Strobe(nLCD_ENB1_GPIO_Port, nLCD_ENB1_Pin); //Pulse ENB1 to strobe the data into LCD controller 1
}

void write2(uint8_t InputData)	//write data on lines 3&4
{
	SafeGPIOBus_Write(&LCDBus, InputData);  //write data to LCD bus
	HAL_GPIO_WritePin (LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_SET); //Set instr. register (RS) high for data
	HAL_GPIO_WritePin (LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET); //Set dir. register (RW) low for write
	LCD_Strobe(nLCD_ENB2_GPIO_Port, nLCD_ENB2_Pin); //Pulse ENB2 to strobe the data into LCD controller 2
}

void lcd_init()
{
	HAL_Delay(15);			//wait 15ms after power up
	command1(0x30);			//wake up controller 1
	HAL_Delay(5);				//wait 5ms
	command2(0x30);			//wake up controller 2
	HAL_Delay(5);				//wait 5ms
	command1(0x30);			//wake up again
	HAL_Delay(1);				//wait at least 160us
	command2(0x30);
	HAL_Delay(1);
	command1(0x30);			//wake up 3rd time
	HAL_Delay(1);				//wait 160us, or you can poll the busy flag from now on
	command2(0x30);
	HAL_Delay(1);
	command1(0x38);			//set interface length (8-bits, 2 lines)
	command2(0x38);
	command1(0x08);			//turn display off
	command2(0x08);
	command1(0x10);			//set cursor/display shift
	command2(0x10);
	command1(0x06);			//set cursor increment
	command2(0x06);
	command1(0x01);			//clear display
	command2(0x01);
//	command1(0x0F);			//turn display on (display ON, cursor ON, blinking ON)
//	command2(0x0F);
	command1(0x0E);			//turn display on (display ON, cursor ON, blinking OFF)
	command2(0x0E);
}

void nextline1()
{
	command1(0xc0);			//set DDRAM address to 40 (line 2)
}

void nextline2()
{
	command2(0xc0);			//set DDRAM address to 40 (line 4)
}

// Writes up to 40 characters from text (a normal null-terminated C string) to
// one physical line, padding any remainder with spaces. Longer strings are
// truncated to 40 chars. text[i] is only ever read up to and including its
// null terminator, never past it.
static void display_line(void (*write_fn)(uint8_t), const char *text)
{
	int ended = 0;
	for (int i = 0; i < 40; i++)
	{
		if (!ended && text[i] == '\0') { ended = 1; }
		write_fn((uint8_t)(ended ? ' ' : text[i]));
	}
}

// Shows one independent, ordinary C string per physical line (up to 40 chars
// each; shorter strings are space-padded, longer ones truncated).
void display(const char *line1, const char *line2, const char *line3, const char *line4)
{
	display_line(write1, line1);
	nextline1();				//move address to line 2
	display_line(write1, line2);
	display_line(write2, line3);
	nextline2();				//move address to line 4
	display_line(write2, line4);
}

void Version_Display()	// Display firmware version and other program ID info
{
	printf ("%s%u.%u.%u (%s, %s)\n\r",
					PROG_NAME, VER_MAJOR, VER_MINOR, VER_PATCH, __DATE__, __TIME__);
}

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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  RetargetInit(&huart2);
  printf("C071 LCD DEV\n\r");

	HAL_GPIO_WritePin (LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin (LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET);

  SafeGPIOBus_Init(&LCDBus, LCDPins, 8);

  // Test code: initialize the LCD and show some text on all 4 lines
	lcd_init();
	display("line1 test", "line 2 test.......", "line 3 tesssssssst", "and line 4");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
  }
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

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_0);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV4;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  HAL_GPIO_WritePin(GPIOA, LCD_RW_Pin|LD1_Pin|nLCD_ENB1_Pin|nLCD_ENB2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LCD_D6_Pin|LCD_D7_Pin|LCD_RS_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_D0_Pin|LCD_D1_Pin|LCD_D5_Pin|LCD_D2_Pin
                          |LCD_D3_Pin|LCD_D4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_RW_Pin nLCD_ENB1_Pin nLCD_ENB2_Pin */
  GPIO_InitStruct.Pin = LCD_RW_Pin|nLCD_ENB1_Pin|nLCD_ENB2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD1_Pin */
  GPIO_InitStruct.Pin = LD1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_D6_Pin LCD_D7_Pin LCD_RS_Pin */
  GPIO_InitStruct.Pin = LCD_D6_Pin|LCD_D7_Pin|LCD_RS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_D0_Pin LCD_D1_Pin LCD_D5_Pin LCD_D2_Pin
                           LCD_D3_Pin LCD_D4_Pin */
  GPIO_InitStruct.Pin = LCD_D0_Pin|LCD_D1_Pin|LCD_D5_Pin|LCD_D2_Pin
                          |LCD_D3_Pin|LCD_D4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void SafeGPIOBus_Init(SafeGPIOBus_t *bus,
                      const SafeGPIO_t *pinList,
                      uint8_t count)
{
    bus->pins  = pinList;
    bus->count = count;
}

static inline void GPIO_SetOutput(GPIO_TypeDef *port, uint16_t pin)
{
  uint32_t pos = __builtin_ctz(pin);        // convert GPIO_PIN_x into bit number

  port->MODER &= ~(3UL << (2 * pos));
  port->MODER |=  (1UL << (2 * pos));       // 01 = output
}

static inline void GPIO_SetInput(GPIO_TypeDef *port, uint16_t pin)
{
  uint32_t pos = __builtin_ctz(pin);
  port->MODER &= ~(3UL << (2 * pos));       // 00 = input
}

/**
  * @brief  Generic bus safe output
  * @retval None
  */
void SafeGPIOBus_Write(SafeGPIOBus_t *bus, uint32_t data) 
{
  GPIO_TypeDef *port;
  uint16_t pin;

  for (uint32_t i = 0; i < bus->count; i++){  //set every bus pin to input
    port = bus->pins[i].port;
    pin  = bus->pins[i].pin;

    GPIO_SetInput(port, pin);
  }

  for (uint32_t i = 0; i < bus->count; i++){  //clear every output latch
    port = bus->pins[i].port;
    pin  = bus->pins[i].pin;

    port->BSRR = ((uint32_t)pin << 16);
  }

  for (uint32_t i = 0; i < bus->count; i++){  //make only '0' bits outputs, '1' remain inputs (ext p/u)
    if (!(data & (1U << i))){
      port = bus->pins[i].port;
      pin  = bus->pins[i].pin;

      GPIO_SetOutput(port, pin);
    }
  }
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
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
