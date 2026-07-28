#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>

namespace tempmon {

using AdcRaw = std::uint16_t;

// distinct type so raw digits cannot pass as a temperature
class MilliCelsius {
 public:
  constexpr MilliCelsius() noexcept = default;
  constexpr explicit MilliCelsius(std::int32_t v) noexcept : v_ {v} {}

  [[nodiscard]] constexpr std::int32_t count() const noexcept { return v_; }

  friend constexpr bool operator<(MilliCelsius a, MilliCelsius b) noexcept { return a.v_ < b.v_; }
  friend constexpr bool operator>(MilliCelsius a, MilliCelsius b) noexcept { return a.v_ > b.v_; }
  friend constexpr bool operator<=(MilliCelsius a, MilliCelsius b) noexcept { return a.v_ <= b.v_; }
  friend constexpr bool operator>=(MilliCelsius a, MilliCelsius b) noexcept { return a.v_ >= b.v_; }
  friend constexpr bool operator==(MilliCelsius a, MilliCelsius b) noexcept { return a.v_ == b.v_; }
  friend constexpr bool operator!=(MilliCelsius a, MilliCelsius b) noexcept { return a.v_ != b.v_; }

  friend constexpr MilliCelsius operator+(MilliCelsius a, MilliCelsius b) noexcept {
    return MilliCelsius {a.v_ + b.v_};
  }
  friend constexpr MilliCelsius operator-(MilliCelsius a, MilliCelsius b) noexcept {
    return MilliCelsius {a.v_ - b.v_};
  }

 private:
  std::int32_t v_ {0};
};

constexpr std::int32_t kMdcPerDegC = 1000;

// integer only so no rounding
constexpr MilliCelsius degC(std::int32_t whole) noexcept { return MilliCelsius {whole * kMdcPerDegC}; }

enum class Condition : std::uint8_t { Normal = 0, Warning, CriticalHot, CriticalCold, Fault };

enum class LedId : std::uint8_t { Green = 0, Yellow, Red };

constexpr std::size_t kLedCount = 3;

// do not renumber fixed by the spec
enum class HwRevision : std::uint8_t {
  RevA = 0,  // 1.0 degC per digit
  RevB = 1   // 0.1 degC per digit
};

const char* toString(Condition cond) noexcept;

}  // namespace tempmon

#endif  // TYPES_HPP
