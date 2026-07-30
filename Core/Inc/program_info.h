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
  *
  * Future Enhancements:
  *
  ******************************************************************************
**/
// Program name
#define	PROG_NAME "DM Solutions LCD Test Code - "
// Version identification using X.Y.Z (Major.Minor.Patch) Semantic Versioning method
#define VER_MAJOR 0
#define VER_MINOR 1
#define VER_PATCH 3



