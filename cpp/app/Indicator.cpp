#include "Indicator.hpp"

#include "Config.hpp"

namespace tempmon {

Indicator::Pattern Indicator::patternFor(Condition cond, std::uint32_t nowMs) noexcept {
  Pattern pattern {};

  const auto green  = static_cast<std::size_t>(LedId::Green);
  const auto yellow = static_cast<std::size_t>(LedId::Yellow);
  const auto red    = static_cast<std::size_t>(LedId::Red);

  switch (cond) {
    case Condition::Normal: pattern[green] = true; break;

    case Condition::Warning: pattern[yellow] = true; break;

    case Condition::CriticalHot:
    case Condition::CriticalCold: pattern[red] = true; break;

    case Condition::Fault:
      // phase from the free clock so a late loop just skips a phase
      pattern[red] = ((nowMs / config::kFaultBlinkHalfPeriodMs) & 1u) != 0u;
      break;
  }

  return pattern;
}

void Indicator::apply(const Pattern& pattern) noexcept {
  for (std::size_t i = 0; i < kLedCount; ++i) {
    if (!valid_ || cached_[i] != pattern[i]) {
      gpio_.writeLed(static_cast<LedId>(i), pattern[i]);
      cached_[i] = pattern[i];
    }
  }
  valid_ = true;
}

}  // namespace tempmon
