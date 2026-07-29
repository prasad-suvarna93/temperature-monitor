#include "filter.h"

#include <string.h>

#include "config.h"

adc_raw_t FilterMedian(const adc_raw_t* block, uint16_t n, adc_raw_t* scratch) {
  if (n == 0u) return 0u;  // no defined answer so the caller checks n

  // copy the block into scratch memory so that the the input block is nevery mutated
  memcpy(scratch, block, (size_t)n * sizeof scratch[0]);

  // Insertion sort
  for (uint16_t i = 1u; i < n; ++i) {
    const adc_raw_t key = scratch[i];
    uint16_t j          = i;
    while (j > 0u && scratch[j - 1u] > key) {
      scratch[j] = scratch[j - 1u];
      --j;
    }
    scratch[j] = key;
  }

  // even n returns the upper middle value not the average
  return scratch[n / 2u];
}

bool FilterAtRail(adc_raw_t raw) { return (raw == 0u) || (raw >= (adc_raw_t)ADC_RAW_MAX); }
