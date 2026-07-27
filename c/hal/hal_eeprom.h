#ifndef HAL_EEPROM_H
#define HAL_EEPROM_H

// read only and it blocks so use it once at boot

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool HalEepromInit(void);

// on false the caller must not read dst
bool HalEepromRead(uint16_t addr, uint8_t* dst, size_t len);

#endif  // HAL_EEPROM_H
