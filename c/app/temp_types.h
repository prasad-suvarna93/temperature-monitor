#ifndef TEMP_TYPES_H
#define TEMP_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef int32_t temp_mdc_t;  // milli-degrees Celsius

typedef uint16_t adc_raw_t;

#define MDC_PER_DEGC 1000

#define DEGC(x) ((temp_mdc_t)((x) * MDC_PER_DEGC))

// the indicator merges hot and cold onto one red lamp
typedef enum {
  COND_NORMAL = 0,
  COND_WARNING,
  COND_CRITICAL_HOT,
  COND_CRITICAL_COLD,
  COND_FAULT
} condition_e;

typedef enum { LED_GREEN = 0, LED_YELLOW, LED_RED, LED_COUNT } led_id_e;

// do not renumber these values are stored in eeprom
typedef enum {
  HW_REV_A = 0,
  HW_REV_B = 1
} hw_rev_e;

#endif
