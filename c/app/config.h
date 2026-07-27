#ifndef CONFIG_H
#define CONFIG_H

#include "temp_types.h"

#define SAMPLE_PERIOD_US 100u

#define SAMPLES_PER_BLOCK 64u

#define SAMPLE_BLOCKS 4u

#define BLOCK_PERIOD_US (SAMPLE_PERIOD_US * SAMPLES_PER_BLOCK)

#define SAMPLE_TIMEOUT_MS 100u

#define ADC_RAW_MAX 4095u

#define THRESH_WARNING_MDC DEGC(85)
#define THRESH_CRIT_HOT_MDC DEGC(105)
#define THRESH_CRIT_COLD_MDC DEGC(5)

// outside this band is a broken signal chain not a real temperature
#define TEMP_MIN_PLAUSIBLE_MDC DEGC(-40)
#define TEMP_MAX_PLAUSIBLE_MDC DEGC(200)

#define FAULT_BLINK_HALF_PERIOD_MS 250u

_Static_assert(SAMPLE_BLOCKS >= 2u, "need at least a double buffer");
_Static_assert((SAMPLE_BLOCKS & (SAMPLE_BLOCKS - 1u)) == 0u, "SAMPLE_BLOCKS must be a power of two");

#endif
