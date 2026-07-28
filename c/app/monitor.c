// composition root
#include "monitor.h"

#include <stddef.h>

#include "compiler.h"
#include "filter.h"
#include "hal_adc.h"
#include "hal_gpio.h"
#include "indicator.h"

static void EnterFault(monitor_t* mon, fault_reason_e why) {
  mon->condition = COND_FAULT;
  mon->fault     = why;
}

bool MonitorInit(monitor_t* mon, uint32_t now_ms) {
  mon->condition        = COND_FAULT;
  mon->fault            = FAULT_NONE;
  mon->config_faulted   = false;
  mon->last_block_ms    = now_ms;
  mon->last_temp_mdc    = 0;
  mon->blocks_processed = 0u;
  mon->blocks_skipped   = 0u;
  mon->class_cfg        = CLASSIFIER_DEFAULT;

  SampleQueueInit(&mon->q);

  (void)HalGpioInit();
  IndicatorInvalidate();

  // read identity before sampling
  const devinfo_status_e st = DeviceInfoLoad(&mon->info);
  if (st != DEVINFO_OK) {
    mon->config_faulted = true;
    EnterFault(mon, FAULT_CONFIG);
    return false;
  }

  if (!SensorCalForRevision(mon->info.revision, &mon->cal)) {
    mon->config_faulted = true;
    EnterFault(mon, FAULT_CONFIG);
    return false;
  }

  if (!HalAdcInit(SAMPLE_PERIOD_US, MonitorOnBlock, mon) || !HalAdcStart()) {
    EnterFault(mon, FAULT_NO_SAMPLES);
    return false;
  }

  // stay in fault until the first block arrives
  mon->fault = FAULT_NO_SAMPLES;
  return true;
}

FAST_TEXT
void MonitorOnBlock(uint8_t block_index, void* ctx) {
  monitor_t* mon = (monitor_t*)ctx;

  // the ISR does one push only
  (void)SampleQueuePush(&mon->q, block_index);
}

void MonitorPoll(monitor_t* mon, uint32_t now_ms) {
  // config fault latches so check it first
  if (mon->config_faulted) {
    EnterFault(mon, FAULT_CONFIG);
    goto annunciate;
  }

  {
    // drain to the newest and drop the rest
    uint8_t idx    = 0u;
    uint8_t newest = 0u;
    bool got       = false;

    while (SampleQueuePop(&mon->q, &idx)) {
      if (got) mon->blocks_skipped++;
      newest = idx;
      got    = true;
    }

    if (!got) {
      // an empty queue is normal so fault only after the timeout
      if ((uint32_t)(now_ms - mon->last_block_ms) > SAMPLE_TIMEOUT_MS) EnterFault(mon, FAULT_NO_SAMPLES);

      goto annunciate;
    }

    mon->last_block_ms = now_ms;
    mon->blocks_processed++;

    const adc_raw_t* blk = HalAdcBlock(newest);
    if (blk == NULL) {
      EnterFault(mon, FAULT_NO_SAMPLES);
      goto annunciate;
    }

    const adc_raw_t median = FilterMedian(blk, (uint16_t)SAMPLES_PER_BLOCK, mon->scratch);

    // check the rail before scaling
    if (FilterAtRail(median)) {
      EnterFault(mon, FAULT_SENSOR_RAIL);
      goto annunciate;
    }

    const temp_mdc_t temp = SensorToMdc(&mon->cal, median);

    if (temp < TEMP_MIN_PLAUSIBLE_MDC || temp > TEMP_MAX_PLAUSIBLE_MDC) {
      EnterFault(mon, FAULT_IMPLAUSIBLE);
      goto annunciate;
    }

    mon->last_temp_mdc = temp;
    mon->condition     = ClassifierStep(&mon->class_cfg, mon->condition, temp);
    mon->fault         = FAULT_NONE;
  }

annunciate:
  // drive the lamps on every path including faults
  {
    const led_pattern_t pattern = IndicatorPattern(mon->condition, now_ms);
    IndicatorApply(&pattern);
  }
}

condition_e MonitorCondition(const monitor_t* mon) { return mon->condition; }
fault_reason_e MonitorFault(const monitor_t* mon) { return mon->fault; }
temp_mdc_t MonitorLastTempMdc(const monitor_t* mon) { return mon->last_temp_mdc; }

const char* FaultReasonStr(fault_reason_e reason) {
  switch (reason) {
    case FAULT_NONE: return "none";
    case FAULT_CONFIG: return "configuration unreadable";
    case FAULT_SENSOR_RAIL: return "sensor at rail (open or short)";
    case FAULT_IMPLAUSIBLE: return "reading outside plausible range";
    case FAULT_NO_SAMPLES: return "no samples from acquisition";
    default: return "unknown";
  }
}
