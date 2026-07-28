#ifndef CLASSIFIER_H
#define CLASSIFIER_H

#include "temp_types.h"

typedef struct {
  temp_mdc_t warning_mdc;
  temp_mdc_t critical_hot_mdc;
  temp_mdc_t critical_cold_mdc;
  temp_mdc_t hysteresis_mdc;
} classifier_cfg_t;

extern const classifier_cfg_t CLASSIFIER_DEFAULT;

// pass COND_FAULT as prev when there is no history
condition_e ClassifierStep(const classifier_cfg_t* cfg, condition_e prev, temp_mdc_t t_mdc);

const char* ConditionStr(condition_e cond);

#endif
