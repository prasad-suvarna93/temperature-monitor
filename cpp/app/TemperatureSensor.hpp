#ifndef TEMPERATURE_SENSOR_HPP
#define TEMPERATURE_SENSOR_HPP

#include "Types.hpp"

namespace tempmon {

class ITemperatureSensor {
 public:
  virtual ~ITemperatureSensor() = default;

  [[nodiscard]] virtual MilliCelsius toTemperature(AdcRaw raw) const noexcept = 0;

  // one count in temperature
  [[nodiscard]] virtual MilliCelsius resolution() const noexcept = 0;

  [[nodiscard]] virtual const char* name() const noexcept = 0;
};

// affine case raw times num over den plus offset
class LinearSensor : public ITemperatureSensor {
 public:
  LinearSensor(std::int32_t num, std::int32_t den, MilliCelsius offset, const char* name) noexcept
      : num_ {num}, den_ {den}, offset_ {offset}, name_ {name} {}

  [[nodiscard]] MilliCelsius toTemperature(AdcRaw raw) const noexcept override;
  [[nodiscard]] MilliCelsius resolution() const noexcept override;
  [[nodiscard]] const char* name() const noexcept override { return name_; }

 private:
  std::int32_t num_;
  std::int32_t den_;
  MilliCelsius offset_;
  const char* name_;
};

// final so the compiler can devirtualise
class RevASensor final : public LinearSensor {
 public:
  RevASensor() noexcept : LinearSensor {1000, 1, MilliCelsius {0}, "Rev-A (1.0 degC/digit)"} {}
};

class RevBSensor final : public LinearSensor {
 public:
  RevBSensor() noexcept : LinearSensor {100, 1, MilliCelsius {0}, "Rev-B (0.1 degC/digit)"} {}
};

// nullptr for an unsupported revision
[[nodiscard]] const ITemperatureSensor* sensorFor(HwRevision rev) noexcept;

}  // namespace tempmon

#endif  // TEMPERATURE_SENSOR_HPP
