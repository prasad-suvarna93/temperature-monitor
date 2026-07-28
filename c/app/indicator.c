#include "indicator.h"

#include "config.h"
#include "hal_gpio.h"

static bool s_cached[LED_COUNT];
static bool s_valid;

led_pattern_t IndicatorPattern(condition_e cond, uint32_t now_ms) {
  led_pattern_t pattern = {.on = {false, false, false}};

  switch (cond) {
    case COND_NORMAL: pattern.on[LED_GREEN] = true; break;

    case COND_WARNING: pattern.on[LED_YELLOW] = true; break;

    case COND_CRITICAL_HOT:
    case COND_CRITICAL_COLD: pattern.on[LED_RED] = true; break;

    case COND_FAULT:
      // blink off the free running clock so a late loop skips a phase
      pattern.on[LED_RED] = ((now_ms / FAULT_BLINK_HALF_PERIOD_MS) & 1u) != 0u;
      break;

    default:
      // unreachable but a fault must never look normal
      pattern.on[LED_RED] = true;
      break;
  }

  return pattern;
}

void IndicatorApply(const led_pattern_t* pattern) {
  for (uint32_t i = 0u; i < (uint32_t)LED_COUNT; ++i) {
    if (!s_valid || s_cached[i] != pattern->on[i]) {
      HalGpioWriteLed((led_id_e)i, pattern->on[i]);
      s_cached[i] = pattern->on[i];
    }
  }
  s_valid = true;
}

void IndicatorInvalidate(void) { s_valid = false; }
