#include "Classifier.hpp"

namespace tempmon {

Condition Classifier::update(MilliCelsius temp) noexcept {
  const MilliCelsius hyst = cfg_.hysteresis;

  // severity wins so escalation is never delayed
  if (temp >= cfg_.criticalHot) {
    condition_ = Condition::CriticalHot;
    return condition_;
  }

  if (temp < cfg_.criticalCold) {
    condition_ = Condition::CriticalCold;
    return condition_;
  }

  // two leaving edges not one signed band
  if (condition_ == Condition::CriticalHot && temp >= cfg_.criticalHot - hyst) return condition_;

  if (condition_ == Condition::CriticalCold && temp < cfg_.criticalCold + hyst) return condition_;

  if (temp >= cfg_.warning) {
    condition_ = Condition::Warning;
    return condition_;
  }

  if (condition_ == Condition::Warning && temp >= cfg_.warning - hyst) return condition_;

  condition_ = Condition::Normal;
  return condition_;
}

const char* toString(Condition cond) noexcept {
  switch (cond) {
    case Condition::Normal: return "NORMAL";
    case Condition::Warning: return "WARNING";
    case Condition::CriticalHot: return "CRITICAL(hot)";
    case Condition::CriticalCold: return "CRITICAL(cold)";
    case Condition::Fault: return "FAULT";
  }
  return "?";
}

}  // namespace tempmon
