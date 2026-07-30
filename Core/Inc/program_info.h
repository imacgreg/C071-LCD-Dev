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
	* Version 1.0.1: command1/command2/write1/write2 are now non-blocking -
	* they enqueue onto a ring buffer (lcd_queue, mirroring retarget.c's UART
	* TX/RX buffer style) and return immediately. LCD_Service(), called once
	* per ms from SysTick_Handler, drains the queue: one entry at a time, per
	* controller, waiting out that controller's HD44780 instruction-execution
	* time (1ms normally, 2ms after Clear Display/Return Home) before issuing
	* the next one. LCD_Strobe() now only does the E pulse itself. display()
	* returns in ~1ms instead of blocking for up to ~320ms; the actual writes
	* stream out over the following ~100-160ms in the background.
	* lcd_init()'s own HAL_Delay-based power-up sequencing is untouched - it
	* still works because interrupts (and so SysTick/LCD_Service) keep
	* running during HAL_Delay's busy-wait - and remains blocking for now,
	* to be revisited in a later pass.
	* Version 1.1.0: Start of a superloop demo feature - a counter that
	* increments once per second and is shown identically on LCD lines 1 and
	* 3, exercising the now-non-blocking write path from the main loop.
	* Version 1.1.1: Implemented the counter. main()'s while(1) loop compares
	* HAL_GetTick() against a running timestamp (no HAL_Delay - stays non-
	* blocking); every 1000ms it formats the count with snprintf and rewrites
	* line 1 (command1(0x80) + display_line(write1, ...)) and line 3
	* (command2(0x80) + display_line(write2, ...)). Lines 2 and 4 are
	* untouched, still showing display()'s original test text. Verified on
	* hardware: lines 1/3 count up together once per second.
	* Version 1.2.0: Start of modularizing the LCD driver out of main.c into
	* its own reusable module, so it can be dropped into other STM32
	* projects rather than hand-copied/adapted each time. Will need a public
	* init/setup function taking the project's GPIO pin/port wiring, called
	* after peripheral init, since pin assignments are project-specific.
  *
  * Future Enhancements:
  *
  ******************************************************************************
**/
// Program name
#define	PROG_NAME "DM Solutions LCD Test Code - "
// Version identification using X.Y.Z (Major.Minor.Patch) Semantic Versioning method
#define VER_MAJOR 1
#define VER_MINOR 2
#define VER_PATCH 0



