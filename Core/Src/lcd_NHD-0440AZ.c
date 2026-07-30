/**
 ******************************************************************************
 * @file      lcd_NHD-0440AZ.c
 * @brief     Driver for Newhaven NHD-0440AZ-style 40x4 character LCDs.
 *            Derived from NewHaven Display (NHD-0440AZ) example code:
 *            https://support.newhavendisplay.com/hc/en-us/articles/4413994414231-NHD-0440AZ
 ******************************************************************************
 */

#include "lcd_NHD-0440AZ.h"

typedef enum { LCD_CTRL_1 = 0, LCD_CTRL_2 = 1, LCD_NUM_CTRL } LCD_Controller_t;

typedef struct {
  uint8_t          data;
  uint8_t          rs;    // 0 = command, 1 = data/write (RW is always write, not stored)
  LCD_Controller_t ctrl;
} LCD_Op_t;

static LCD_Config_t lcd_config;

#define LCD_QUEUE_SIZE 256   // entries, not bytes; comfortably covers a full 160-char LCD_Display() burst

static volatile LCD_Op_t lcd_queue[LCD_QUEUE_SIZE];
static volatile uint16_t lcd_queue_head = 0;   // producer-owned (LCD_Command1/2, LCD_Write1/2)
static volatile uint16_t lcd_queue_tail = 0;   // consumer-owned (LCD_Service, runs in SysTick ISR)
static volatile uint32_t lcd_busy_until[LCD_NUM_CTRL]; // HAL_GetTick() timestamps, one per controller

// Busy-wait of a few hundred ns to ~1us. Cortex-M0+ has no DWT cycle counter, so this is
// a calibrated NOP-count loop rather than a precise timer, with margin above the
// HD44780's ~450-500ns minimum enable pulse width. Calibrated for a ~12MHz core clock;
// re-check this if porting to a project running at a very different SYSCLK.
static inline void LCD_ShortDelay(void)
{
	for (volatile uint32_t i = 0; i < 20; i++) { __NOP(); }
}

// Sets a data-bus pin's mode directly via MODER, without disturbing OTYPER (already
// configured open-drain by LCD_Init()). Avoids the overhead of a full HAL_GPIO_Init()
// call per bit on every byte written.
static inline void LCD_BusPinSetOutput(GPIO_TypeDef *port, uint16_t pin)
{
	uint32_t pos = __builtin_ctz(pin);        // convert GPIO_PIN_x into bit number
	port->MODER &= ~(3UL << (2 * pos));
	port->MODER |=  (1UL << (2 * pos));       // 01 = output
}

static inline void LCD_BusPinSetInput(GPIO_TypeDef *port, uint16_t pin)
{
	uint32_t pos = __builtin_ctz(pin);
	port->MODER &= ~(3UL << (2 * pos));       // 00 = input
}

// Safely places a byte on the 8-bit data bus for a 5V-tolerant open-drain interface:
// bits that are 1 are left as floating inputs (pulled high externally); bits that are 0
// are driven low. Never drives a pin high directly.
static void LCD_BusWrite(uint8_t data)
{
	for (int i = 0; i < 8; i++) {  // set every bus pin to input first
		LCD_BusPinSetInput(lcd_config.data[i].port, lcd_config.data[i].pin);
	}
	for (int i = 0; i < 8; i++) {  // clear every output latch
		lcd_config.data[i].port->BSRR = ((uint32_t)lcd_config.data[i].pin << 16);
	}
	for (int i = 0; i < 8; i++) {  // make only '0' bits outputs, '1' remain inputs (ext p/u)
		if (!(data & (1U << i))) {
			LCD_BusPinSetOutput(lcd_config.data[i].port, lcd_config.data[i].pin);
		}
	}
}

// Strobes an LCD controller's enable line: idle low, pulse high, back to idle low.
// Only covers the E pulse width itself - the required post-command settle time is
// handled by LCD_Service()'s lcd_busy_until scheduling below, not blocked on here.
static void LCD_Strobe(GPIO_TypeDef *port, uint16_t pin)
{
	HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
	LCD_ShortDelay();
	HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
}

// Producer side: enqueues a command/data byte for a controller and returns immediately.
// Called only from main-line code (never from LCD_Service/the SysTick ISR), so it's the
// sole writer of lcd_queue_head - single-writer-per-index scheme, matching this
// project's retarget.c UART TX/RX ring buffers. Silently drops on a full queue, same
// policy as retarget.c's RX overflow handling.
static int LCD_Enqueue(LCD_Controller_t ctrl, uint8_t rs, uint8_t data)
{
	uint16_t next_head = (lcd_queue_head + 1) % LCD_QUEUE_SIZE;
	if (next_head == lcd_queue_tail) { return 0; }	// queue full, drop
	lcd_queue[lcd_queue_head].data = data;
	lcd_queue[lcd_queue_head].rs   = rs;
	lcd_queue[lcd_queue_head].ctrl = ctrl;
	lcd_queue_head = next_head;
	return 1;
}

void LCD_Init(const LCD_Config_t *config)
{
	lcd_config = *config;

	GPIO_InitTypeDef gpio_init = {0};
	gpio_init.Mode  = GPIO_MODE_OUTPUT_OD;
	gpio_init.Pull  = GPIO_NOPULL;
	gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

	const LCD_GPIO_t *all_pins[12] = {
		&lcd_config.data[0], &lcd_config.data[1], &lcd_config.data[2], &lcd_config.data[3],
		&lcd_config.data[4], &lcd_config.data[5], &lcd_config.data[6], &lcd_config.data[7],
		&lcd_config.rs, &lcd_config.rw, &lcd_config.enb1, &lcd_config.enb2,
	};
	for (int i = 0; i < 12; i++) {
		HAL_GPIO_WritePin(all_pins[i]->port, all_pins[i]->pin, GPIO_PIN_RESET);
		gpio_init.Pin = all_pins[i]->pin;
		HAL_GPIO_Init(all_pins[i]->port, &gpio_init);
	}

	lcd_queue_head = lcd_queue_tail = 0;
	lcd_busy_until[LCD_CTRL_1] = lcd_busy_until[LCD_CTRL_2] = 0;
}

// Consumer side: call once per SysTick tick. Drains one ready queue entry per call,
// respecting each controller's independent instruction-execution settle time (~37-43us
// typical, ~1.52ms after Clear Display/Return Home - rounded up to the nearest 1ms tick
// since that's this design's timing granularity).
void LCD_Service(void)
{
	if (lcd_queue_head == lcd_queue_tail) { return; }	// empty

	LCD_Op_t op = lcd_queue[lcd_queue_tail];	// struct copy, single volatile read
	if (HAL_GetTick() < lcd_busy_until[op.ctrl]) { return; }	// target controller still settling

	LCD_BusWrite(op.data);
	HAL_GPIO_WritePin(lcd_config.rs.port, lcd_config.rs.pin, op.rs ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(lcd_config.rw.port, lcd_config.rw.pin, GPIO_PIN_RESET);
	const LCD_GPIO_t *enb = (op.ctrl == LCD_CTRL_1) ? &lcd_config.enb1 : &lcd_config.enb2;
	LCD_Strobe(enb->port, enb->pin);

	int long_settle = (op.rs == 0) && (op.data == 0x01 || (op.data & 0xFE) == 0x02); // Clear Display / Return Home
	lcd_busy_until[op.ctrl] = HAL_GetTick() + (long_settle ? 2 : 1);

	lcd_queue_tail = (lcd_queue_tail + 1) % LCD_QUEUE_SIZE;
}

int LCD_Idle(void)
{
	return (lcd_queue_head == lcd_queue_tail)
	    && HAL_GetTick() >= lcd_busy_until[LCD_CTRL_1]
	    && HAL_GetTick() >= lcd_busy_until[LCD_CTRL_2];
}

void LCD_Command1(uint8_t data)
{
	LCD_Enqueue(LCD_CTRL_1, 0, data);
}

void LCD_Command2(uint8_t data)
{
	LCD_Enqueue(LCD_CTRL_2, 0, data);
}

void LCD_Write1(uint8_t data)
{
	LCD_Enqueue(LCD_CTRL_1, 1, data);
}

void LCD_Write2(uint8_t data)
{
	LCD_Enqueue(LCD_CTRL_2, 1, data);
}

void LCD_PowerOn(void)
{
	HAL_Delay(15);			//wait 15ms after power up
	LCD_Command1(0x30);		//wake up controller 1
	HAL_Delay(5);				//wait 5ms
	LCD_Command2(0x30);		//wake up controller 2
	HAL_Delay(5);				//wait 5ms
	LCD_Command1(0x30);		//wake up again
	HAL_Delay(1);				//wait at least 160us
	LCD_Command2(0x30);
	HAL_Delay(1);
	LCD_Command1(0x30);		//wake up 3rd time
	HAL_Delay(1);				//wait 160us, or you can poll the busy flag from now on
	LCD_Command2(0x30);
	HAL_Delay(1);
	LCD_Command1(0x38);		//set interface length (8-bits, 2 lines)
	LCD_Command2(0x38);
	LCD_Command1(0x08);		//turn display off
	LCD_Command2(0x08);
	LCD_Command1(0x10);		//set cursor/display shift
	LCD_Command2(0x10);
	LCD_Command1(0x06);		//set cursor increment
	LCD_Command2(0x06);
	LCD_Command1(0x01);		//clear display
	LCD_Command2(0x01);
//	LCD_Command1(0x0F);		//turn display on (display ON, cursor ON, blinking ON)
//	LCD_Command2(0x0F);
	LCD_Command1(0x0E);		//turn display on (display ON, cursor ON, blinking OFF)
	LCD_Command2(0x0E);
}

void LCD_NextLine1(void)
{
	LCD_Command1(0xc0);		//set DDRAM address to 40 (line 2)
}

void LCD_NextLine2(void)
{
	LCD_Command2(0xc0);		//set DDRAM address to 40 (line 4)
}

// Writes up to 40 characters from text (a normal null-terminated C string) to one
// physical line, padding any remainder with spaces. Longer strings are truncated to 40
// chars. text[i] is only ever read up to and including its null terminator, never past it.
static void LCD_WriteLine(void (*write_fn)(uint8_t), const char *text)
{
	int ended = 0;
	for (int i = 0; i < 40; i++)
	{
		if (!ended && text[i] == '\0') { ended = 1; }
		write_fn((uint8_t)(ended ? ' ' : text[i]));
	}
}

void LCD_DisplayLine1(const char *text) { LCD_Command1(0x80); LCD_WriteLine(LCD_Write1, text); }
void LCD_DisplayLine2(const char *text) { LCD_NextLine1();    LCD_WriteLine(LCD_Write1, text); }
void LCD_DisplayLine3(const char *text) { LCD_Command2(0x80); LCD_WriteLine(LCD_Write2, text); }
void LCD_DisplayLine4(const char *text) { LCD_NextLine2();    LCD_WriteLine(LCD_Write2, text); }

void LCD_Display(const char *line1, const char *line2, const char *line3, const char *line4)
{
	LCD_DisplayLine1(line1);
	LCD_DisplayLine2(line2);
	LCD_DisplayLine3(line3);
	LCD_DisplayLine4(line4);
}
