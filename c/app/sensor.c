// raw ADC to milli degC
#include "sensor.h"

#include "config.h"

// raw times num must fit in int32
_Static_assert((int64_t)ADC_RAW_MAX * 1000 < INT32_MAX, "raw*num overflows int32_t");

bool SensorCalForRevision(hw_rev_e rev, sensor_cal_t* out) {
  if (rev == HW_REV_A) {
    *out = (sensor_cal_t){1000, 1, 0, "Rev-A (1.0 degC/digit)"};
    return true;
  } else if (rev == HW_REV_B) {
    *out = (sensor_cal_t){100, 1, 0, "Rev-B (0.1 degC/digit)"};
    return true;
  }

  return false;
}

temp_mdc_t SensorToMdc(const sensor_cal_t* cal, adc_raw_t raw) {
  return (temp_mdc_t)(((int32_t)raw * cal->num) / cal->den) + cal->offset_mdc;
}
