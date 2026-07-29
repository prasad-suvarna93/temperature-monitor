// maps a condition to lamp states
//   normal         green steady
//   warning        yellow steady
//   critical       red steady
//   fault          red blinking at 2 Hz
// only one lamp is lit at a time
#ifndef INDICATOR_HPP
#define INDICATOR_HPP

#include <array>

#include "Hal.hpp"
#include "Types.hpp"

namespace tempmon {

class Indicator {
 public:
  using Pattern = std::array<bool, kLedCount>;

  explicit Indicator(IGpio& gpio) noexcept : gpio_ {gpio} {}

  // pure so tests can check the blink phase without a clock
  [[nodiscard]] static Pattern patternFor(Condition cond, std::uint32_t nowMs) noexcept;

  // writes only the lamps that changed
  void apply(const Pattern& pattern) noexcept;

  // drop the cache so the next apply writes every lamp
  void invalidate() noexcept { valid_ = false; }

 private:
  IGpio& gpio_;
  Pattern cached_ {};
  bool valid_ {false};
};

}  // namespace tempmon

#endif  // INDICATOR_HPP
