// Monitor is the composition root it owns state and fixes the order
#ifndef MONITOR_H
#define MONITOR_H

#include "classifier.h"
#include "config.h"
#include "device_info.h"
#include "sample_queue.h"
#include "sensor.h"
#include "temp_types.h"

// boot order is identity then calibration then acquisition
// until the revision is known a digit cannot be read

// why the device is in COND_FAULT kept distinct for service
typedef enum {
  FAULT_NONE = 0,
  FAULT_CONFIG,
  FAULT_SENSOR_RAIL,
  FAULT_IMPLAUSIBLE,
  FAULT_NO_SAMPLES
} fault_reason_e;

typedef struct {
  device_info_t info;
  sensor_cal_t cal;
  classifier_cfg_t class_cfg;

  condition_e condition;
  fault_reason_e fault;
  bool config_faulted;  // latched never recovers in this run

  sample_queue_t q;
  uint32_t last_block_ms;

  temp_mdc_t last_temp_mdc;
  uint32_t blocks_processed;
  uint32_t blocks_skipped;  // stale blocks dropped when the loop fell behind

  // filter workspace kept here so the filter stays pure
  adc_raw_t scratch[SAMPLES_PER_BLOCK];
} monitor_t;

// returns false if the device cannot measure
bool MonitorInit(monitor_t* m, uint32_t now_ms);

// interrupt context just a bounds check and a store
void MonitorOnBlock(uint8_t block_index, void* ctx);

// main loop safe to call with no new block
void MonitorPoll(monitor_t* m, uint32_t now_ms);

condition_e MonitorCondition(const monitor_t* m);
fault_reason_e MonitorFault(const monitor_t* m);
temp_mdc_t MonitorLastTempMdc(const monitor_t* m);
const char* FaultReasonStr(fault_reason_e f);

#endif
