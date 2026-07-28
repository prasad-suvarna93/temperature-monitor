// Host tests: everything above the HAL.

#include "config.h"
#include "sensor.h"
#include "filter.h"
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

static void TestFilter(void) {
  SECTION("filter: median rejects what a mean would not");

  adc_raw_t scratch[SAMPLES_PER_BLOCK];

  {
    const adc_raw_t v[9] = {7u, 2u, 9u, 4u, 1u, 8u, 3u, 6u, 5u};
    CHECK_EQ(FilterMedian(v, 9u, scratch), 5u);
  }

  // even count returns the upper middle value not the average
  {
    const adc_raw_t v[4] = {40u, 10u, 30u, 20u};
    CHECK_EQ(FilterMedian(v, 4u, scratch), 30u);
  }

  // three samples slammed to full scale a mean would light red
  {
    adc_raw_t v[SAMPLES_PER_BLOCK];
    for (uint32_t i = 0u; i < SAMPLES_PER_BLOCK; ++i) v[i] = 850u;
    v[3] = v[17] = v[40] = (adc_raw_t)ADC_RAW_MAX;

    CHECK_EQ(FilterMedian(v, (uint16_t)SAMPLES_PER_BLOCK, scratch), 850u);
  }

  // corrupt more than half and the median follows the corruption
  {
    adc_raw_t v[SAMPLES_PER_BLOCK];
    for (uint32_t i = 0u; i < SAMPLES_PER_BLOCK; ++i) v[i] = (i < 33u) ? 4000u : 850u;

    CHECK_EQ(FilterMedian(v, (uint16_t)SAMPLES_PER_BLOCK, scratch), 4000u);
  }

  SECTION("filter: converter end stops");
  CHECK(FilterAtRail(0u));
  CHECK(FilterAtRail((adc_raw_t)ADC_RAW_MAX));
  CHECK(!FilterAtRail(1u));
  CHECK(!FilterAtRail(2000u));
}


int main(void) {
  printf("temperature monitor -- host tests (C)\n");

  TestSensor();
  TestFilter();

  printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return (g_failures == 0) ? 0 : 1;
}
