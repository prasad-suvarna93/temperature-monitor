#ifndef HAL_TIME_H
#define HAL_TIME_H

// compare by subtraction so the 32-bit wrap is a non-event

#include <stdbool.h>
#include <stdint.h>

bool HalTimeInit(void);
uint32_t HalTimeNowMs(void);

#endif  // HAL_TIME_H
