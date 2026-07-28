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

  [[nodiscard]] static Pattern patternFor(Condition cond, std::uint32_t nowMs) noexcept;

  // writes only the lamps that changed
  void apply(const Pattern& pattern) noexcept;

  void invalidate() noexcept { valid_ = false; }

 private:
  IGpio& gpio_;
  Pattern cached_ {};
  bool valid_ {false};
};

}  // namespace tempmon

#endif  // INDICATOR_HPP
