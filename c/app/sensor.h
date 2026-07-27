// Sensor scaling from raw ADC digits to milli-degrees Celsius
#ifndef SENSOR_H
#define SENSOR_H

#include "temp_types.h"

// mdc = raw * num / den + offset
typedef struct {
  int32_t num;
  int32_t den;
  temp_mdc_t offset_mdc;
  const char* name;
} sensor_cal_t;

// false if the revision has no calibration
bool SensorCalForRevision(hw_rev_e rev, sensor_cal_t* out);

temp_mdc_t SensorToMdc(const sensor_cal_t* cal, adc_raw_t raw);

#endif
