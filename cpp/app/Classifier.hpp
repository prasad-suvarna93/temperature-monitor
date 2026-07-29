// maps a temperature to a condition with hysteresis
#ifndef CLASSIFIER_HPP
#define CLASSIFIER_HPP

// set hysteresis to zero for the literal requirement

#include "Config.hpp"
#include "Types.hpp"

namespace tempmon {

class Classifier {
 public:
  struct Config {
    MilliCelsius warning;
    MilliCelsius criticalHot;
    MilliCelsius criticalCold;
    MilliCelsius hysteresis;
  };

  static constexpr Config kDefault {config::kWarning, config::kCriticalHot, config::kCriticalCold, config::kHysteresis};

  explicit Classifier(Config cfg = kDefault, Condition initial = Condition::Normal) noexcept
      : cfg_ {cfg}, condition_ {initial} {}

  // classify one reading against the previous condition
  // severity wins when the condition rules overlap
  // enter a band at the stated threshold and leave it only after real recovery
  Condition update(MilliCelsius temp) noexcept;

  [[nodiscard]] Condition condition() const noexcept { return condition_; }
  [[nodiscard]] MilliCelsius hysteresis() const noexcept { return cfg_.hysteresis; }

  void setHysteresis(MilliCelsius hyst) noexcept { cfg_.hysteresis = hyst; }

  void reset(Condition cond) noexcept { condition_ = cond; }

 private:
  Config cfg_;
  Condition condition_;
};

}  // namespace tempmon

#endif  // CLASSIFIER_HPP
