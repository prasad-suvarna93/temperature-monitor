#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "Types.hpp"

namespace tempmon::config {

constexpr std::uint32_t kSamplePeriodUs = 100;

constexpr std::size_t kSamplesPerBlock = 64;

constexpr std::size_t kSampleBlocks = 4;

constexpr std::uint32_t kBlockPeriodUs = kSamplePeriodUs * static_cast<std::uint32_t>(kSamplesPerBlock);

constexpr std::uint32_t kSampleTimeoutMs = 100;

constexpr AdcRaw kAdcRawMax = 4095;  // 12-bit

constexpr MilliCelsius kWarning      = degC(85);
constexpr MilliCelsius kCriticalHot  = degC(105);
constexpr MilliCelsius kCriticalCold = degC(5);

// applied on the leaving edge only
constexpr MilliCelsius kHysteresis = MilliCelsius {500};  // 0.5 degC

constexpr MilliCelsius kMinPlausible = degC(-40);
constexpr MilliCelsius kMaxPlausible = degC(200);

constexpr std::uint32_t kFaultBlinkHalfPeriodMs = 250;

static_assert(kSampleBlocks >= 2, "need at least a double buffer");
static_assert((kSampleBlocks & (kSampleBlocks - 1)) == 0, "kSampleBlocks must be a power of two (index masking)");
static_assert(kCriticalCold + kHysteresis < kWarning, "cold release band overlaps the warning threshold");
static_assert(kWarning + kHysteresis < kCriticalHot, "warning band is narrower than the hysteresis");

}  // namespace tempmon::config

#endif  // CONFIG_HPP
