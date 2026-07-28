// Host tests: everything above the HAL.

#include <string.h>
#include "classifier.h"
#include "config.h"
#include "device_info.h"
#include "filter.h"
#include "hal_time.h"
#include "host_hal.h"
#include "indicator.h"
#include "monitor.h"
#include "sample_queue.h"
#include "sensor.h"
#include "test.h"

int g_checks;
int g_failures;

static void StepAt(monitor_t* mon, hw_rev_e rev, temp_mdc_t t_mdc, uint32_t dt_ms) {
  HostAdcSetRaw(HostRawForTemp(rev, t_mdc));
  HostAdcProduceBlock();
  HostTimeAdvanceMs(dt_ms);
  MonitorPoll(mon, HalTimeNowMs());
}

static void TestDeviceInfo(void) {
  SECTION("device_info: identity record");

  device_info_t info;

  HostReset();
  HostEepromProgramValid(HW_REV_A, "ABC1234");
  CHECK_EQ(DeviceInfoLoad(&info), DEVINFO_OK);
  CHECK_EQ(info.revision, HW_REV_A);
  CHECK_STR(info.serial, "ABC1234");

  HostReset();
  HostEepromProgramValid(HW_REV_B, "XYZ9876");
  CHECK_EQ(DeviceInfoLoad(&info), DEVINFO_OK);
  CHECK_EQ(info.revision, HW_REV_B);
  CHECK_STR(info.serial, "XYZ9876");

  // a failed read must not leave a plausible record behind
  HostReset();
  HostEepromProgramValid(HW_REV_A, "ABC1234");
  HostEepromSetBusFail(true);
  CHECK_EQ(DeviceInfoLoad(&info), DEVINFO_ERR_BUS);

  // blank device reads 0xFF everywhere
  HostReset();
  {
    uint8_t blank[EE_RECORD_LEN];
    memset(blank, 0xFF, sizeof blank);
    HostEepromProgramRaw(blank, sizeof blank);
  }
  CHECK_EQ(DeviceInfoLoad(&info), DEVINFO_ERR_MAGIC);

  HostReset();
  HostEepromProgramValid(HW_REV_B, "ABC1234");
  HostEepromCorruptCrc();
  CHECK_EQ(DeviceInfoLoad(&info), DEVINFO_ERR_CRC);

  // valid record but unknown revision must not fall back to Rev-A
  HostReset();
  {
    uint8_t rec[EE_RECORD_LEN] = {0x5Au, 0xC5u, 0x07u, 'Q', 'Q', 'Q', '0', '0', '0', '1', 0x00u};
    rec[EE_RECORD_LEN - 1u]    = DeviceInfoCrc8(rec, EE_RECORD_LEN - 1u);
    HostEepromProgramRaw(rec, sizeof rec);
  }
  CHECK_EQ(DeviceInfoLoad(&info), DEVINFO_ERR_REVISION);
}

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

static void TestIndicator(void) {
  SECTION("indicator: one lamp per condition");

  led_pattern_t pattern;

  pattern = IndicatorPattern(COND_NORMAL, 0u);
  CHECK(pattern.on[LED_GREEN] && !pattern.on[LED_YELLOW] && !pattern.on[LED_RED]);

  pattern = IndicatorPattern(COND_WARNING, 0u);
  CHECK(!pattern.on[LED_GREEN] && pattern.on[LED_YELLOW] && !pattern.on[LED_RED]);

  // both critical bands drive the same red lamp
  pattern = IndicatorPattern(COND_CRITICAL_HOT, 0u);
  CHECK(!pattern.on[LED_GREEN] && !pattern.on[LED_YELLOW] && pattern.on[LED_RED]);

  pattern = IndicatorPattern(COND_CRITICAL_COLD, 0u);
  CHECK(!pattern.on[LED_GREEN] && !pattern.on[LED_YELLOW] && pattern.on[LED_RED]);

  SECTION("indicator: fault blinks, and never shows green");

  CHECK(!IndicatorPattern(COND_FAULT, 0u).on[LED_RED]);
  CHECK(IndicatorPattern(COND_FAULT, 250u).on[LED_RED]);
  CHECK(!IndicatorPattern(COND_FAULT, 500u).on[LED_RED]);
  CHECK(IndicatorPattern(COND_FAULT, 750u).on[LED_RED]);

  // a device that cannot measure never shows the all-is-well lamp
  for (uint32_t t = 0u; t < 2000u; t += 37u) {
    const led_pattern_t fault_pat = IndicatorPattern(COND_FAULT, t);
    CHECK(!fault_pat.on[LED_GREEN] && !fault_pat.on[LED_YELLOW]);
  }

  SECTION("indicator: only changed lamps reach the pin");

  HostReset();
  IndicatorInvalidate();

  const led_pattern_t normal = IndicatorPattern(COND_NORMAL, 0u);
  IndicatorApply(&normal);
  const uint32_t after_first = HostLedWriteCount();
  CHECK_EQ(after_first, LED_COUNT);

  IndicatorApply(&normal);
  CHECK_EQ(HostLedWriteCount(), after_first);

  const led_pattern_t warn = IndicatorPattern(COND_WARNING, 0u);
  IndicatorApply(&warn);
  CHECK_EQ(HostLedWriteCount(), after_first + 2u);
}

static void TestSampleQueue(void) {
  SECTION("sample_queue: fills, drains, and drops rather than blocks");

  sample_queue_t queue;
  uint8_t v;

  SampleQueueInit(&queue);
  CHECK(!SampleQueuePop(&queue, &v));

  for (uint8_t i = 0u; i < (uint8_t)SAMPLE_QUEUE_DEPTH; ++i) CHECK(SampleQueuePush(&queue, i));

  // full so the interrupt drops and counts rather than blocking
  CHECK(!SampleQueuePush(&queue, 99u));
  CHECK_EQ(SampleQueueOverruns(&queue), 1u);

  CHECK(!SampleQueuePush(&queue, 99u));
  CHECK_EQ(SampleQueueOverruns(&queue), 2u);

  for (uint8_t i = 0u; i < (uint8_t)SAMPLE_QUEUE_DEPTH; ++i) {
    CHECK(SampleQueuePop(&queue, &v));
    CHECK_EQ(v, i);
  }
  CHECK(!SampleQueuePop(&queue, &v));

  // many trips round the ring must not drift or alias
  SampleQueueInit(&queue);
  for (uint32_t n = 0u; n < 1000u; ++n) {
    CHECK(SampleQueuePush(&queue, (uint8_t)(n & 0xFFu)));
    CHECK(SampleQueuePop(&queue, &v));
    CHECK_EQ(v, (uint8_t)(n & 0xFFu));
  }
  CHECK_EQ(SampleQueueOverruns(&queue), 0u);
}

static void TestIntegrationRevb(void) {
  SECTION("integration: Rev-B, sensor to lamp");

  monitor_t mon;

  HostReset();
  HostEepromProgramValid(HW_REV_B, "ABC1234");
  CHECK(MonitorInit(&mon, HalTimeNowMs()));
  CHECK(HostAdcRunning());

  // before any block the device says it does not know the temperature
  CHECK_EQ(MonitorCondition(&mon), COND_FAULT);
  CHECK_EQ(MonitorFault(&mon), FAULT_NO_SAMPLES);

  StepAt(&mon, HW_REV_B, 25000, 7u);
  CHECK_EQ(MonitorCondition(&mon), COND_NORMAL);
  CHECK_EQ(MonitorLastTempMdc(&mon), 25000);
  CHECK(HostLed(LED_GREEN) && !HostLed(LED_YELLOW) && !HostLed(LED_RED));

  StepAt(&mon, HW_REV_B, 85000, 7u);  // exactly the threshold
  CHECK_EQ(MonitorCondition(&mon), COND_WARNING);
  CHECK(!HostLed(LED_GREEN) && HostLed(LED_YELLOW) && !HostLed(LED_RED));

  StepAt(&mon, HW_REV_B, 110000, 7u);
  CHECK_EQ(MonitorCondition(&mon), COND_CRITICAL_HOT);
  CHECK(!HostLed(LED_GREEN) && !HostLed(LED_YELLOW) && HostLed(LED_RED));

  // inside the release band still critical and the lamp does not flicker
  StepAt(&mon, HW_REV_B, 104600, 7u);
  CHECK_EQ(MonitorCondition(&mon), COND_CRITICAL_HOT);

  StepAt(&mon, HW_REV_B, 104400, 7u);
  CHECK_EQ(MonitorCondition(&mon), COND_WARNING);

  StepAt(&mon, HW_REV_B, 20000, 7u);
  CHECK_EQ(MonitorCondition(&mon), COND_NORMAL);

  // cold excursion the other red band
  StepAt(&mon, HW_REV_B, 3000, 7u);
  CHECK_EQ(MonitorCondition(&mon), COND_CRITICAL_COLD);
  CHECK(HostLed(LED_RED));

  StepAt(&mon, HW_REV_B, 20000, 7u);
  CHECK_EQ(MonitorCondition(&mon), COND_NORMAL);
}

static void TestIntegrationNoiseAndSpikes(void) {
  SECTION("integration: noise and glitches do not move the lamp");

  monitor_t mon;

  HostReset();
  HostEepromProgramValid(HW_REV_B, "ABC1234");
  CHECK(MonitorInit(&mon, HalTimeNowMs()));

  // parked under the threshold so noise alone must not raise a warning
  HostAdcSetNoise(4u);
  HostAdcSetRaw(HostRawForTemp(HW_REV_B, 84500));

  for (uint32_t i = 0u; i < 200u; ++i) {
    HostAdcProduceBlock();
    HostTimeAdvanceMs(7u);
    MonitorPoll(&mon, HalTimeNowMs());
  }
  CHECK_EQ(MonitorCondition(&mon), COND_NORMAL);

  // a burst of glitches the median throws away where a mean would light red
  HostAdcSetNoise(0u);
  HostAdcSetRaw(HostRawForTemp(HW_REV_B, 50000));
  HostAdcInjectSpike((adc_raw_t)ADC_RAW_MAX, 20u);
  HostAdcProduceBlock();
  HostTimeAdvanceMs(7u);
  MonitorPoll(&mon, HalTimeNowMs());

  CHECK_EQ(MonitorCondition(&mon), COND_NORMAL);
  CHECK_EQ(MonitorLastTempMdc(&mon), 50000);
}

static void TestIntegrationFaults(void) {
  SECTION("integration: a device that cannot measure never shows green");

  monitor_t mon;

  // eeprom unreadable so acquisition never starts
  HostReset();
  HostEepromSetBusFail(true);
  CHECK(!MonitorInit(&mon, HalTimeNowMs()));
  CHECK_EQ(MonitorCondition(&mon), COND_FAULT);
  CHECK_EQ(MonitorFault(&mon), FAULT_CONFIG);
  CHECK(!HostAdcRunning());

  // latched so repairing the bus at runtime does not un-boot the device
  HostEepromSetBusFail(false);
  HostEepromProgramValid(HW_REV_B, "ABC1234");
  HostTimeAdvanceMs(1000u);
  MonitorPoll(&mon, HalTimeNowMs());
  CHECK_EQ(MonitorFault(&mon), FAULT_CONFIG);

  SECTION("integration: converter at an end stop is a wiring fault");

  HostReset();
  HostEepromProgramValid(HW_REV_B, "ABC1234");
  CHECK(MonitorInit(&mon, HalTimeNowMs()));

  StepAt(&mon, HW_REV_B, 25000, 7u);
  CHECK_EQ(MonitorCondition(&mon), COND_NORMAL);

  HostAdcSetRaw((adc_raw_t)ADC_RAW_MAX);  // input open or shorted
  HostAdcProduceBlock();
  HostTimeAdvanceMs(7u);
  MonitorPoll(&mon, HalTimeNowMs());
  CHECK_EQ(MonitorCondition(&mon), COND_FAULT);
  CHECK_EQ(MonitorFault(&mon), FAULT_SENSOR_RAIL);

  // recovers on its own once the signal returns
  StepAt(&mon, HW_REV_B, 25000, 7u);
  CHECK_EQ(MonitorCondition(&mon), COND_NORMAL);
  CHECK_EQ(MonitorFault(&mon), FAULT_NONE);

  SECTION("integration: silence from the acquisition chain is a fault");

  HostReset();
  HostEepromProgramValid(HW_REV_B, "ABC1234");
  CHECK(MonitorInit(&mon, HalTimeNowMs()));

  StepAt(&mon, HW_REV_B, 25000, 7u);
  CHECK_EQ(MonitorCondition(&mon), COND_NORMAL);

  // inside the timeout the device holds its last reading
  HostTimeAdvanceMs(50u);
  MonitorPoll(&mon, HalTimeNowMs());
  CHECK_EQ(MonitorCondition(&mon), COND_NORMAL);

  // past the timeout the silence itself is the diagnosis
  HostTimeAdvanceMs(100u);
  MonitorPoll(&mon, HalTimeNowMs());
  CHECK_EQ(MonitorCondition(&mon), COND_FAULT);
  CHECK_EQ(MonitorFault(&mon), FAULT_NO_SAMPLES);
}

static void TestIntegrationBacklog(void) {
  SECTION("integration: a late main loop takes the newest block, not the oldest");

  monitor_t mon;

  HostReset();
  HostEepromProgramValid(HW_REV_B, "ABC1234");
  CHECK(MonitorInit(&mon, HalTimeNowMs()));

  HostAdcSetRaw(HostRawForTemp(HW_REV_B, 20000));
  HostAdcProduceBlock();
  HostAdcSetRaw(HostRawForTemp(HW_REV_B, 40000));
  HostAdcProduceBlock();
  HostAdcSetRaw(HostRawForTemp(HW_REV_B, 95000));
  HostAdcProduceBlock();

  HostTimeAdvanceMs(20u);
  MonitorPoll(&mon, HalTimeNowMs());

  CHECK_EQ(MonitorLastTempMdc(&mon), 95000);  // newest not first
  CHECK_EQ(MonitorCondition(&mon), COND_WARNING);
  CHECK_EQ(mon.blocks_skipped, 2u);
}


int main(void) {
  printf("temperature monitor -- host tests (C)\n");

  TestDeviceInfo();
  TestSensor();
  TestFilter();
  TestClassifierThresholds();
  TestClassifierHysteresis();
  TestClassifierChatter();
  TestClassifierSweep();
  TestIndicator();
  TestSampleQueue();
  TestIntegrationRevb();
  TestIntegrationNoiseAndSpikes();
  TestIntegrationFaults();
  TestIntegrationBacklog();

  printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return (g_failures == 0) ? 0 : 1;
}
