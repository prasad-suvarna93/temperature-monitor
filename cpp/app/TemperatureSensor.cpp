#include "TemperatureSensor.hpp"

#include "Config.hpp"

namespace tempmon {

// raw times num must fit in int32
static_assert(static_cast<std::int64_t>(config::kAdcRawMax) * 1000 < INT32_MAX,
              "raw * num overflows int32_t for the highest gain in use");

MilliCelsius LinearSensor::toTemperature(AdcRaw raw) const noexcept {
  return MilliCelsius {(static_cast<std::int32_t>(raw) * num_) / den_} + offset_;
}

MilliCelsius LinearSensor::resolution() const noexcept { return MilliCelsius {num_ / den_}; }

// namespace scope avoids a guard variable
namespace {
const RevASensor kRevA;
const RevBSensor kRevB;
}  // namespace

const ITemperatureSensor* sensorFor(HwRevision rev) noexcept {
  switch (rev) {
    case HwRevision::RevA: return &kRevA;
    case HwRevision::RevB: return &kRevB;
    default: return nullptr;
  }
}

}  // namespace tempmon
