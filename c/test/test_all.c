// Host tests: everything above the HAL.

#include "config.h"
#include "sensor.h"
#include "test.h"

int g_checks;
int g_failures;

static void TestSensor(void) {
  SECTION("sensor: both revisions scale to one unit");

  sensor_cal_t cal;

  // the two values from the requirement
  CHECK(SensorCalForRevision(HW_REV_A, &cal));
  CHECK_EQ(SensorToMdc(&cal, 10u), 10000);

  CHECK(SensorCalForRevision(HW_REV_B, &cal));
  CHECK_EQ(SensorToMdc(&cal, 100u), 10000);

  CHECK(SensorCalForRevision(HW_REV_A, &cal));
  CHECK_EQ(SensorToMdc(&cal, 85u), 85000);
  CHECK_EQ(SensorToMdc(&cal, 105u), 105000);
  CHECK_EQ(SensorToMdc(&cal, 0u), 0);

  CHECK(SensorCalForRevision(HW_REV_B, &cal));
  CHECK_EQ(SensorToMdc(&cal, 850u), 85000);
  CHECK_EQ(SensorToMdc(&cal, 1050u), 105000);

  // top of the converter must not overflow or wrap
  CHECK_EQ(SensorToMdc(&cal, (adc_raw_t)ADC_RAW_MAX), 409500);

  CHECK(!SensorCalForRevision((hw_rev_e)7, &cal));
}


int main(void) {
  printf("temperature monitor -- host tests (C)\n");

  TestSensor();

  printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return (g_failures == 0) ? 0 : 1;
}
