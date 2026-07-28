#include "classifier.h"

#include "config.h"

const classifier_cfg_t CLASSIFIER_DEFAULT = {
    .warning_mdc       = THRESH_WARNING_MDC,
    .critical_hot_mdc  = THRESH_CRIT_HOT_MDC,
    .critical_cold_mdc = THRESH_CRIT_COLD_MDC,
    .hysteresis_mdc    = HYSTERESIS_MDC,
};

condition_e ClassifierStep(const classifier_cfg_t* cfg, condition_e prev, temp_mdc_t t_mdc) {
  const temp_mdc_t hyst = cfg->hysteresis_mdc;

  // entering a critical band always wins now
  if (t_mdc >= cfg->critical_hot_mdc) return COND_CRITICAL_HOT;

  if (t_mdc < cfg->critical_cold_mdc) return COND_CRITICAL_COLD;

  // hysteresis holds the old band on the way out only
  if (prev == COND_CRITICAL_HOT && t_mdc >= cfg->critical_hot_mdc - hyst) return COND_CRITICAL_HOT;

  if (prev == COND_CRITICAL_COLD && t_mdc < cfg->critical_cold_mdc + hyst) return COND_CRITICAL_COLD;

  if (t_mdc >= cfg->warning_mdc) return COND_WARNING;

  if (prev == COND_WARNING && t_mdc >= cfg->warning_mdc - hyst) return COND_WARNING;

  return COND_NORMAL;
}

const char* ConditionStr(condition_e cond) {
  switch (cond) {
    case COND_NORMAL: return "NORMAL";
    case COND_WARNING: return "WARNING";
    case COND_CRITICAL_HOT: return "CRITICAL(hot)";
    case COND_CRITICAL_COLD: return "CRITICAL(cold)";
    case COND_FAULT: return "FAULT";
    default: return "?";
  }
}
