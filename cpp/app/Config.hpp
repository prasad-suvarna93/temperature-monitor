// timing thresholds and plausibility limits in one place
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "Types.hpp"

namespace tempmon::config {

// 100 us period so 10 kHz
constexpr std::uint32_t kSamplePeriodUs = 100;

// 64 samples per block so the ISR runs at 156 Hz
constexpr std::size_t kSamplesPerBlock = 64;

// four blocks gives the main loop about 19 ms of slack
constexpr std::size_t kSampleBlocks = 4;

constexpr std::uint32_t kBlockPeriodUs = kSamplePeriodUs * static_cast<std::uint32_t>(kSamplesPerBlock);

// no block inside this window means the chain is dead
constexpr std::uint32_t kSampleTimeoutMs = 100;

constexpr AdcRaw kAdcRawMax = 4095;  // 12-bit

// trip points from the requirement
constexpr MilliCelsius kWarning      = degC(85);
constexpr MilliCelsius kCriticalHot  = degC(105);
constexpr MilliCelsius kCriticalCold = degC(5);

// applied on the leaving edge only
constexpr MilliCelsius kHysteresis = MilliCelsius {500};  // 0.5 degC

// outside this band the signal chain is broken not a temperature
constexpr MilliCelsius kMinPlausible = degC(-40);
constexpr MilliCelsius kMaxPlausible = degC(200);

constexpr std::uint32_t kFaultBlinkHalfPeriodMs = 250;  // 2 Hz

static_assert(kSampleBlocks >= 2, "need at least a double buffer");
static_assert((kSampleBlocks & (kSampleBlocks - 1)) == 0, "kSampleBlocks must be a power of two (index masking)");
static_assert(kCriticalCold + kHysteresis < kWarning, "cold release band overlaps the warning threshold");
static_assert(kWarning + kHysteresis < kCriticalHot, "warning band is narrower than the hysteresis");

}  // namespace tempmon::config

#endif  // CONFIG_HPP
