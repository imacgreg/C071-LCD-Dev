/**
 ******************************************************************************
 * @file      lcd_NHD-0440AZ.h
 * @brief     Driver for Newhaven NHD-0440AZ-style 40x4 character LCDs: two
 *            HD44780 controllers (physical lines 1-2, lines 3-4) muxed onto
 *            one shared 8-bit data bus, bit-banged over plain GPIO.
 *
 * Usage:
 *   1. Build an LCD_Config_t describing this project's GPIO wiring.
 *   2. Call LCD_Init(&config) after enabling the GPIO port clocks it uses
 *      (e.g. via MX_GPIO_Init()).
 *   3. Call LCD_PowerOn() once to run the HD44780 power-on/reset sequence.
 *   4. Ensure LCD_Service() is called at least once per SysTick tick (from
 *      SysTick_Handler) - it drains the non-blocking write queue that
 *      LCD_Command1/2, LCD_Write1/2, and LCD_Display* enqueue onto.
 ******************************************************************************
 */

#ifndef __LCD_NHD_0440AZ_H
#define __LCD_NHD_0440AZ_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32c0xx_hal.h"

typedef struct {
  GPIO_TypeDef *port;
  uint16_t      pin;
} LCD_GPIO_t;

/* data[0..7] = D0..D7, in bit order (bit i of a byte written to the LCD
 * drives data[i]). */
typedef struct {
  LCD_GPIO_t data[8];
  LCD_GPIO_t rs;
  LCD_GPIO_t rw;
  LCD_GPIO_t enb1;   /* controller 1 enable (physical lines 1-2) */
  LCD_GPIO_t enb2;   /* controller 2 enable (physical lines 3-4) */
} LCD_Config_t;

/* Binds the driver to this project's GPIO wiring and configures pin modes
 * (open-drain output, matching the 5V-tolerant scheme this display needs)
 * itself - it does not rely on the host project's CubeMX-generated
 * MX_GPIO_Init() having already done so, though it's harmless if it has.
 * Caller must have already enabled the GPIO port clocks for whichever ports
 * `config` references (already true via any normal MX_GPIO_Init()). */
void LCD_Init(const LCD_Config_t *config);

/* Sends the HD44780 power-on/reset command sequence to both controllers.
 * Call once, after LCD_Init(). Blocking (uses HAL_Delay internally). */
void LCD_PowerOn(void);

/* Call once per SysTick tick (from SysTick_Handler) to drain the write
 * queue. */
void LCD_Service(void);

/* True once every enqueued command/write has actually reached the LCD. */
int LCD_Idle(void);

/* Raw primitives: RS low (command) vs high (data/write), addressed to
 * controller 1 (lines 1-2) or controller 2 (lines 3-4). Non-blocking - see
 * LCD_Service(). */
void LCD_Command1(uint8_t data);
void LCD_Command2(uint8_t data);
void LCD_Write1(uint8_t data);
void LCD_Write2(uint8_t data);

/* Moves the DDRAM cursor to the second physical line of controller 1/2. */
void LCD_NextLine1(void);
void LCD_NextLine2(void);

/* Each shows one plain C string on that physical line (space-padded or
 * truncated to 40 chars). LCD_Display() is a convenience wrapper calling
 * all four. */
void LCD_DisplayLine1(const char *text);
void LCD_DisplayLine2(const char *text);
void LCD_DisplayLine3(const char *text);
void LCD_DisplayLine4(const char *text);
void LCD_Display(const char *line1, const char *line2, const char *line3, const char *line4);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_NHD_0440AZ_H */
