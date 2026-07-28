#include "sensor.h"

#include "config.h"

// indexed by hw_rev_e
static const sensor_cal_t CAL[] = {
    [HW_REV_A] = {1000, 1, 0, "Rev-A (1.0 degC/digit)"},
    [HW_REV_B] = {100, 1, 0, "Rev-B (0.1 degC/digit)"},
};

#define CAL_COUNT (sizeof(CAL) / sizeof(CAL[0]))

// raw times num must fit in int32
_Static_assert((int64_t)ADC_RAW_MAX * 1000 < INT32_MAX, "raw*num overflows int32_t");

bool SensorCalForRevision(hw_rev_e rev, sensor_cal_t* out) {
  if ((uint32_t)rev >= CAL_COUNT) {
    return false;
  }

  // a table hole zero fills so den 0 guards a divide
  if (CAL[rev].den == 0) {
    return false;
  }

  *out = CAL[rev];
  return true;
}

temp_mdc_t SensorToMdc(const sensor_cal_t* cal, adc_raw_t raw) {
  return (temp_mdc_t)(((int32_t)raw * cal->num) / cal->den) + cal->offset_mdc;
}
