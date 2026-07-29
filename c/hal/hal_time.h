#ifndef HAL_TIME_H
#define HAL_TIME_H

// millisecond tick for timeouts and blink phase
// compare by subtraction so the 32-bit wrap is a non-event

#include <stdbool.h>
#include <stdint.h>

bool HalTimeInit(void);

// milliseconds since boot and safe from any context
uint32_t HalTimeNowMs(void);

#endif  // HAL_TIME_H
