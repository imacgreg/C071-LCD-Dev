/**
 ******************************************************************************
 * @file      retarget.c
 * @brief     Retargets stdio (printf/scanf) to a UART peripheral using
 *            interrupt-driven (non-blocking) transfers.
 *
 *            TX: bytes are queued into a ring buffer. __io_putchar() kicks off
 *            HAL_UART_Transmit_IT() the first time the peripheral goes idle;
 *            HAL_UART_TxCpltCallback() keeps draining the buffer from there.
 *            RX: HAL_UART_Receive_IT() is kept continuously armed for a
 *            single byte; HAL_UART_RxCpltCallback() copies it into a ring
 *            buffer and re-arms. __io_getchar() waits (WFI) for data to
 *            appear rather than blocking the peripheral itself.
 *
 *            syscalls.c's _write/_read call the weak __io_putchar/__io_getchar
 *            hooks below, so linking this file is sufficient to route
 *            printf/scanf through the UART configured via RetargetInit().
 ******************************************************************************
 */

#include "retarget.h"
#include <stdio.h>
#include <stdint.h>

#define RETARGET_TX_BUF_SIZE 256
#define RETARGET_RX_BUF_SIZE 128

static UART_HandleTypeDef *s_huart;

static volatile uint8_t s_tx_buf[RETARGET_TX_BUF_SIZE];
static volatile uint16_t s_tx_head;   /* next free slot; written by producer (main) */
static volatile uint16_t s_tx_tail;   /* next byte to send; written by consumer (ISR) */
static volatile uint8_t s_tx_busy;
static uint16_t s_tx_run;             /* length of the in-flight HAL_UART_Transmit_IT() run */

static volatile uint8_t s_rx_buf[RETARGET_RX_BUF_SIZE];
static volatile uint16_t s_rx_head;   /* next free slot; written by producer (ISR) */
static volatile uint16_t s_rx_tail;   /* next byte to read; written by consumer (main) */
static uint8_t s_rx_isr_byte;

/* Must be called with interrupts disabled. Starts transmitting the next
 * contiguous run of buffered bytes, stopping at the buffer wrap point. */
static void RetargetStartTx(void)
{
  if (s_tx_tail == s_tx_head)
  {
    s_tx_busy = 0;
    return;
  }

  s_tx_run = (s_tx_tail < s_tx_head)
               ? (uint16_t)(s_tx_head - s_tx_tail)
               : (uint16_t)(RETARGET_TX_BUF_SIZE - s_tx_tail);

  s_tx_busy = 1;
  HAL_UART_Transmit_IT(s_huart, (uint8_t *)&s_tx_buf[s_tx_tail], s_tx_run);
}

void RetargetInit(UART_HandleTypeDef *huart)
{
  s_huart = huart;

  /* Disable I/O buffering: stdout so output appears immediately, and stdin so
   * getchar() requests (and __io_getchar() blocks on) exactly one byte at a
   * time rather than newlib pre-filling a large internal buffer up front. */
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stdin, NULL, _IONBF, 0);

  /* Keep a single-byte receive continuously armed. */
  HAL_UART_Receive_IT(s_huart, &s_rx_isr_byte, 1);
}

int __io_putchar(int ch)
{
  uint16_t next_head = (uint16_t)((s_tx_head + 1) % RETARGET_TX_BUF_SIZE);

  /* Spin while the buffer is full; the TX-complete ISR drains it. */
  while (next_head == s_tx_tail)
  {
  }

  s_tx_buf[s_tx_head] = (uint8_t)ch;
  s_tx_head = next_head;

  __disable_irq();
  if (!s_tx_busy)
  {
    RetargetStartTx();
  }
  __enable_irq();

  return ch;
}

int __io_getchar(void)
{
  uint8_t ch;

  while (s_rx_tail == s_rx_head)
  {
    __WFI();
  }

  ch = s_rx_buf[s_rx_tail];
  s_rx_tail = (uint16_t)((s_rx_tail + 1) % RETARGET_RX_BUF_SIZE);
  return ch;
}

int Retarget_TryGetChar(void)
{
  if (s_rx_tail == s_rx_head)
  {
    return -1;
  }

  uint8_t ch = s_rx_buf[s_rx_tail];
  s_rx_tail = (uint16_t)((s_rx_tail + 1) % RETARGET_RX_BUF_SIZE);
  return (int)ch;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != s_huart->Instance)
  {
    return;
  }

  s_tx_tail = (uint16_t)((s_tx_tail + s_tx_run) % RETARGET_TX_BUF_SIZE);
  RetargetStartTx();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != s_huart->Instance)
  {
    return;
  }

  uint16_t next_head = (uint16_t)((s_rx_head + 1) % RETARGET_RX_BUF_SIZE);
  if (next_head != s_rx_tail)
  {
    s_rx_buf[s_rx_head] = s_rx_isr_byte;
    s_rx_head = next_head;
  }
  /* else: buffer full, byte dropped */

  HAL_UART_Receive_IT(s_huart, &s_rx_isr_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != s_huart->Instance)
  {
    return;
  }

  /* Re-arm reception after e.g. an overrun/framing/noise error aborts it. */
  HAL_UART_Receive_IT(s_huart, &s_rx_isr_byte, 1);
}
