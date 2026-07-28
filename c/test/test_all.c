// Host tests: everything above the HAL.

#include "config.h"
#include "sensor.h"
#include "filter.h"
#include "classifier.h"
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

static void TestClassifierThresholds(void) {
  SECTION("classifier: the thresholds, exactly as specified");

  const classifier_cfg_t* cfg = &CLASSIFIER_DEFAULT;

  // warning is >= 85 degC so one milli-degree below is still normal
  CHECK_EQ(ClassifierStep(cfg, COND_NORMAL, 84999), COND_NORMAL);
  CHECK_EQ(ClassifierStep(cfg, COND_NORMAL, 85000), COND_WARNING);

  CHECK_EQ(ClassifierStep(cfg, COND_WARNING, 104999), COND_WARNING);
  CHECK_EQ(ClassifierStep(cfg, COND_WARNING, 105000), COND_CRITICAL_HOT);

  // cold is < 5 degC so the threshold itself is still normal
  CHECK_EQ(ClassifierStep(cfg, COND_NORMAL, 5000), COND_NORMAL);
  CHECK_EQ(ClassifierStep(cfg, COND_NORMAL, 4999), COND_CRITICAL_COLD);

  SECTION("classifier: overlapping rules resolve by severity");

  // both yellow and red hold at 110 degC and red wins
  CHECK_EQ(ClassifierStep(cfg, COND_NORMAL, 110000), COND_CRITICAL_HOT);

  // both green and red hold at 3 degC and red wins
  CHECK_EQ(ClassifierStep(cfg, COND_NORMAL, 3000), COND_CRITICAL_COLD);

  // severity beats history
  CHECK_EQ(ClassifierStep(cfg, COND_WARNING, 200000), COND_CRITICAL_HOT);
  CHECK_EQ(ClassifierStep(cfg, COND_CRITICAL_HOT, 1000), COND_CRITICAL_COLD);
}

static void TestClassifierHysteresis(void) {
  SECTION("classifier: hysteresis holds on the way out only");

  const classifier_cfg_t* cfg = &CLASSIFIER_DEFAULT;

  // escalation is never delayed only recovery is
  CHECK_EQ(ClassifierStep(cfg, COND_NORMAL, 85000), COND_WARNING);

  CHECK_EQ(ClassifierStep(cfg, COND_WARNING, 84999), COND_WARNING);
  CHECK_EQ(ClassifierStep(cfg, COND_WARNING, 84500), COND_WARNING);
  CHECK_EQ(ClassifierStep(cfg, COND_WARNING, 84499), COND_NORMAL);

  CHECK_EQ(ClassifierStep(cfg, COND_CRITICAL_HOT, 104999), COND_CRITICAL_HOT);
  CHECK_EQ(ClassifierStep(cfg, COND_CRITICAL_HOT, 104500), COND_CRITICAL_HOT);
  CHECK_EQ(ClassifierStep(cfg, COND_CRITICAL_HOT, 104499), COND_WARNING);

  CHECK_EQ(ClassifierStep(cfg, COND_CRITICAL_COLD, 5000), COND_CRITICAL_COLD);
  CHECK_EQ(ClassifierStep(cfg, COND_CRITICAL_COLD, 5499), COND_CRITICAL_COLD);
  CHECK_EQ(ClassifierStep(cfg, COND_CRITICAL_COLD, 5500), COND_NORMAL);

  // out of a fault there is no history so state comes from the thresholds
  CHECK_EQ(ClassifierStep(cfg, COND_FAULT, 84999), COND_NORMAL);
  CHECK_EQ(ClassifierStep(cfg, COND_FAULT, 90000), COND_WARNING);

  SECTION("classifier: literal specification behaviour, hysteresis disabled");

  // with the band at zero the module matches the requirement to the letter
  const classifier_cfg_t literal = {
      .warning_mdc       = THRESH_WARNING_MDC,
      .critical_hot_mdc  = THRESH_CRIT_HOT_MDC,
      .critical_cold_mdc = THRESH_CRIT_COLD_MDC,
      .hysteresis_mdc    = 0,
  };

  CHECK_EQ(ClassifierStep(&literal, COND_WARNING, 84999), COND_NORMAL);
  CHECK_EQ(ClassifierStep(&literal, COND_CRITICAL_HOT, 104999), COND_WARNING);
  CHECK_EQ(ClassifierStep(&literal, COND_CRITICAL_COLD, 5000), COND_NORMAL);
}

static void TestClassifierChatter(void) {
  SECTION("classifier: hysteresis is worth what it costs");

  const classifier_cfg_t literal = {
      .warning_mdc       = THRESH_WARNING_MDC,
      .critical_hot_mdc  = THRESH_CRIT_HOT_MDC,
      .critical_cold_mdc = THRESH_CRIT_COLD_MDC,
      .hysteresis_mdc    = 0,
  };

  uint32_t rng          = 0x2468ACE0u;
  condition_e s_hyst    = COND_NORMAL;
  condition_e s_literal = COND_NORMAL;
  uint32_t n_hyst       = 0u;
  uint32_t n_literal    = 0u;

  for (uint32_t i = 0u; i < 1000u; ++i) {
    // 85.000 degC plus or minus 0.3 deterministic
    rng                   = (rng * 1103515245u) + 12345u;
    const temp_mdc_t temp = 85000 + (temp_mdc_t)((rng >> 16) % 601u) - 300;

    const condition_e next_h = ClassifierStep(&CLASSIFIER_DEFAULT, s_hyst, temp);
    const condition_e next_l = ClassifierStep(&literal, s_literal, temp);

    if (next_h != s_hyst) n_hyst++;
    if (next_l != s_literal) n_literal++;

    s_hyst    = next_h;
    s_literal = next_l;
  }

  printf(
      "      lamp changes over 1000 samples on the threshold:"
      " %u with hysteresis, %u without\n",
      n_hyst,
      n_literal);

  CHECK_EQ(n_hyst, 1u);
  CHECK(n_literal > 100u);
}

static void TestClassifierSweep(void) {
  SECTION("classifier: full sweep up and down");

  const classifier_cfg_t* cfg = &CLASSIFIER_DEFAULT;
  condition_e state           = COND_NORMAL;

  const struct {
    temp_mdc_t t;
    condition_e want;
  } up[] = {
      {20000, COND_NORMAL},
      {84900, COND_NORMAL},
      {85000, COND_WARNING},
      {95000, COND_WARNING},
      {104900, COND_WARNING},
      {105000, COND_CRITICAL_HOT},
      {150000, COND_CRITICAL_HOT},
  };
  for (uint32_t i = 0u; i < sizeof up / sizeof up[0]; ++i) {
    state = ClassifierStep(cfg, state, up[i].t);
    CHECK_EQ(state, up[i].want);
  }

  const struct {
    temp_mdc_t t;
    condition_e want;
  } down[] = {
      {104900, COND_CRITICAL_HOT},  // inside the release band holds
      {104500, COND_CRITICAL_HOT},
      {104400, COND_WARNING},
      {84600, COND_WARNING},
      {84400, COND_NORMAL},
      {20000, COND_NORMAL},
      {5000, COND_NORMAL},
      {4999, COND_CRITICAL_COLD},
      {5400, COND_CRITICAL_COLD},  // release band on the cold side
      {5500, COND_NORMAL},
  };
  for (uint32_t i = 0u; i < sizeof down / sizeof down[0]; ++i) {
    state = ClassifierStep(cfg, state, down[i].t);
    CHECK_EQ(state, down[i].want);
  }
}


int main(void) {
  printf("temperature monitor -- host tests (C)\n");

  TestSensor();
  TestFilter();
  TestClassifierThresholds();
  TestClassifierHysteresis();
  TestClassifierChatter();
  TestClassifierSweep();

  printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return (g_failures == 0) ? 0 : 1;
}
