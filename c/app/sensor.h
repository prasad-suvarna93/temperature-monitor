// Sensor scaling from raw ADC digits to milli-degrees Celsius
#ifndef SENSOR_H
#define SENSOR_H

#include "temp_types.h"

// mdc = raw * num / den + offset
// gain is num over den in milli degC per digit
// Rev A is 1000 over 1 and Rev B is 100 over 1
// a fractional gain like 62.5 stays exact as 125 over 2
typedef struct {
  int32_t num;
  int32_t den;
  temp_mdc_t offset_mdc;  // zero today and kept for a biased or sub zero sensor
  const char* name;
} sensor_cal_t;

// false if the revision has no calibration
bool SensorCalForRevision(hw_rev_e rev, sensor_cal_t* out);

temp_mdc_t SensorToMdc(const sensor_cal_t* cal, adc_raw_t raw);

#endif
