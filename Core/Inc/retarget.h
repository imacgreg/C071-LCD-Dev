/**
 ******************************************************************************
 * @file      retarget.h
 * @brief     Retargets stdio (printf/scanf) to a UART peripheral.
 ******************************************************************************
 */

#ifndef __RETARGET_H
#define __RETARGET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32c0xx_hal.h"

void RetargetInit(UART_HandleTypeDef *huart);

/* Non-blocking single-byte read from the RX ring buffer: returns 0-255 if a
 * byte was available, or -1 if not. Shares the same ring buffer/consumer
 * index as getchar()/scanf() (via __io_getchar) -- use one or the other in
 * a given part of the application, not both, since the buffer supports only
 * a single consumer. */
int Retarget_TryGetChar(void);

#ifdef __cplusplus
}
#endif

#endif /* __RETARGET_H */
