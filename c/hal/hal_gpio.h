#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdbool.h>

#include "temp_types.h"

bool HalGpioInit(void);

// safe to call with the value the pin already holds
void HalGpioWriteLed(led_id_e led, bool on);

#endif  // HAL_GPIO_H
