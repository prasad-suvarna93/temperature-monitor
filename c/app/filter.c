#include "filter.h"

#include "config.h"

adc_raw_t FilterAverage(const adc_raw_t* block, uint16_t n) {
  if (n == 0u) return 0u;

  // sum in uint32 so a long block does not overflow
  uint32_t sum = 0u;
  for (uint16_t i = 0u; i < n; ++i) {
    sum += block[i];
  }

  return (adc_raw_t)(sum / n);
}

bool FilterAtRail(adc_raw_t raw) { return (raw == 0u) || (raw >= (adc_raw_t)ADC_RAW_MAX); }
