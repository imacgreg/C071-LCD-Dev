/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  * LCD 5V 4x40 Character Display Test
  *
  * Version -100: Original version with basic 4x40 LCD display. Using OD outputs
	* w/ external 5V pull-ups. Exposes output pins to 5V which is out of spec.
  * Version -110: Use OD output for low bits & input w/ 5V pull-up for high bits.
	* Eliminates output port exposure to 5V. Requires switching in/out mode.	
	* Works for Port E data but have not tested for discrete Port C controls yet.
	* Version -120: Use BSRR instead of ODR for output control. TIM8_CH1/CH2 used
	* for ENB1/2 active duration control.
	* Version -130: Removed TIM8 dependency (not ported from F-series). ENB1/2
	* active duration is now a plain GPIO pulse (LCD_Strobe) with a calibrated
	* busy-wait, since the STM32C071 target has no equivalent timer wired up.
	* Version -200: First working display on real hardware. Fixed several bugs
	* found during bring-up:
	*   - RS/RW were being toggled via raw GPIOC->MODER hacks hardcoded for the
	*     F-series pinout (RS=PC8, RW=PC9); on this board RS=PC7 and RW=PA4, so
	*     RS never actually reached logic high during data writes. Replaced with
	*     direct HAL_GPIO_WritePin calls (RS/RW are already true open-drain
	*     outputs per MX_GPIO_Init, no mode-toggling trick needed).
	*   - LCD_Strobe's busy-wait covered only the E pulse width, not the
	*     HD44780's required instruction execution time (~37-43us typical,
	*     ~1.52ms after Clear Display/Return Home) between commands sent to the
	*     same controller. lcd_init()/display() chain commands with no delay of
	*     their own, so a HAL_Delay(2) settle time was added after every strobe.
	*   - display() read a fixed 160 bytes (4 lines x 40 chars) from a single
	*     buffer with no null-terminator check, running past the end of shorter
	*     strings into whatever flash bytes followed (observed: text bleeding in
	*     from an unrelated printf format string). Reworked to take one
	*     independent, ordinary C string per line, each safely truncated/padded
	*     to 40 chars - see display()'s new signature.
	*   - Hardware: Nucleo PB3 (LCD_D2) is tied to the board's TRACESWO debug
	*     pin via a solder bridge/jumper, which prevented that line from
	*     driving correctly. Isolated by removing the jumper; ST-Link
	*     flash/debug and UART printf both continue to work without it.
	* Version -201: Cleanup pass, no functional change. Removed leftover
	* F-series detritus (dead SafeOutputPortE function, stale Port E/PC8/PC9
	* comments, unused lcd_data globals/locals) and the printf/heartbeat
	* diagnostics added during hardware bring-up.
	* Version 1.0.0: Start of a non-blocking rework of this same LCD driver/
	* functionality. The 0.x line (command1/command2/write1/write2/lcd_init/
	* display) is blocking throughout - LCD_Strobe's HAL_Delay(2) and the
	* HAL_Delay() calls in lcd_init() all stall the CPU in place. Major version
	* bump to mark that the driver's calling convention is expected to change.
  *
  * Future Enhancements:
  *
  ******************************************************************************
**/
// Program name
#define	PROG_NAME "DM Solutions LCD Test Code - "
// Version identification using X.Y.Z (Major.Minor.Patch) Semantic Versioning method
#define VER_MAJOR 1
#define VER_MINOR 0
#define VER_PATCH 0



