#ifndef CONFIG_H
#define CONFIG_H

#include "temp_types.h"

#define SAMPLE_PERIOD_US 100u

#define SAMPLES_PER_BLOCK 64u

#define SAMPLE_BLOCKS 4u

#define BLOCK_PERIOD_US (SAMPLE_PERIOD_US * SAMPLES_PER_BLOCK)

// no block in this window means acquisition is dead
#define SAMPLE_TIMEOUT_MS 100u

#define ADC_RAW_MAX 4095u

#define THRESH_WARNING_MDC DEGC(85)
#define THRESH_CRIT_HOT_MDC DEGC(105)
#define THRESH_CRIT_COLD_MDC DEGC(5)

#define HYSTERESIS_MDC 500  // 0.5 degC

// outside this band the signal chain is broken not a temperature
#define TEMP_MIN_PLAUSIBLE_MDC DEGC(-40)
#define TEMP_MAX_PLAUSIBLE_MDC DEGC(200)

#define FAULT_BLINK_HALF_PERIOD_MS 250u  // 2 Hz

_Static_assert(SAMPLE_BLOCKS >= 2u, "need at least a double buffer");
_Static_assert((SAMPLE_BLOCKS & (SAMPLE_BLOCKS - 1u)) == 0u, "SAMPLE_BLOCKS must be a power of two");
_Static_assert(THRESH_CRIT_COLD_MDC + HYSTERESIS_MDC < THRESH_WARNING_MDC,
               "cold-release band overlaps the warning threshold");
_Static_assert(THRESH_WARNING_MDC + HYSTERESIS_MDC < THRESH_CRIT_HOT_MDC,
               "warning band is narrower than the hysteresis");

#endif
