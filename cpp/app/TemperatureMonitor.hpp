#ifndef TEMPERATURE_MONITOR_HPP
#define TEMPERATURE_MONITOR_HPP

#include "Classifier.hpp"
#include "Config.hpp"
#include "DeviceInfo.hpp"
#include "Hal.hpp"
#include "Indicator.hpp"
#include "MedianFilter.hpp"
#include "SampleQueue.hpp"
#include "TemperatureSensor.hpp"
#include "Types.hpp"

namespace tempmon {

// distinct reasons map to distinct service actions
enum class FaultReason : std::uint8_t {
  None = 0,
  Config,
  SensorRail,
  Implausible,
  NoSamples
};

const char* toString(FaultReason reason) noexcept;

class TemperatureMonitor final : public IBlockSink {
 public:
  TemperatureMonitor(IAdc& adc, IGpio& gpio, IEeprom& eeprom, IClock& clock) noexcept;

  // false if the device cannot measure but it still annunciates
  [[nodiscard]] bool init() noexcept;

  // interrupt context
  void onBlock(std::uint8_t blockIndex) noexcept override;

  // main loop idempotent
  void poll() noexcept;

  [[nodiscard]] Condition condition() const noexcept { return condition_; }
  [[nodiscard]] FaultReason fault() const noexcept { return fault_; }
  [[nodiscard]] MilliCelsius temperature() const noexcept { return lastTemp_; }
  [[nodiscard]] const DeviceInfo& info() const noexcept { return info_; }
  [[nodiscard]] const ITemperatureSensor* sensor() const noexcept { return sensor_; }
  [[nodiscard]] const Classifier& classifier() const noexcept { return classifier_; }
  [[nodiscard]] std::uint32_t blocksSkipped() const noexcept { return blocksSkipped_; }

 private:
  void enterFault(FaultReason why) noexcept;

  IAdc& adc_;
  IEeprom& eeprom_;
  IClock& clock_;

  DeviceInfo info_ {};
  const ITemperatureSensor* sensor_ {nullptr};
  Classifier classifier_ {};
  Indicator indicator_;

  MedianFilter<config::kSamplesPerBlock> filter_ {};
  SampleQueue<config::kSampleBlocks> queue_ {};

  Condition condition_ {Condition::Fault};
  FaultReason fault_ {FaultReason::None};
  bool configFaulted_ {false};  // latched never recovers in run

  std::uint32_t lastBlockMs_ {0};
  MilliCelsius lastTemp_ {};
  std::uint32_t blocksSkipped_ {0};
};

}  // namespace tempmon

#endif  // TEMPERATURE_MONITOR_HPP
