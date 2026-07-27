// block average filter
#ifndef FILTER_H
#define FILTER_H

#include "temp_types.h"

adc_raw_t FilterAverage(const adc_raw_t* block, uint16_t n);

// true when the converter is pinned at either end
bool FilterAtRail(adc_raw_t v);

#endif
