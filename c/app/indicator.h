// Indicator maps a condition to lamp states
#ifndef INDICATOR_H
#define INDICATOR_H

#include "temp_types.h"

typedef struct {
  bool on[LED_COUNT];
} led_pattern_t;

// pure so tests can check the blink phase without a clock
led_pattern_t IndicatorPattern(condition_e c, uint32_t now_ms);

// writes only the lamps that changed
void IndicatorApply(const led_pattern_t* p);

// drop the cache so the next apply writes every lamp
void IndicatorInvalidate(void);

#endif
