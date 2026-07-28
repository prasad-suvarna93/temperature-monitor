// Median filter turns a block of noisy digits into one value
#ifndef FILTER_H
#define FILTER_H

#include "temp_types.h"

// median ignores a single stuck sample
// scratch holds n elements and is clobbered
adc_raw_t FilterMedian(const adc_raw_t* block, uint16_t n, adc_raw_t* scratch);

// pinned at an end points at a broken input not a temperature
bool FilterAtRail(adc_raw_t v);

#endif
