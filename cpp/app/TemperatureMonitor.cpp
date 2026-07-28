#include "TemperatureMonitor.hpp"

namespace tempmon {

TemperatureMonitor::TemperatureMonitor(IAdc& adc, IGpio& gpio, IEeprom& eeprom, IClock& clock) noexcept
    : adc_ {adc}, eeprom_ {eeprom}, clock_ {clock}, indicator_ {gpio} {}

void TemperatureMonitor::enterFault(FaultReason why) noexcept {
  condition_ = Condition::Fault;
  fault_     = why;
}

bool TemperatureMonitor::init() noexcept {
  lastBlockMs_ = clock_.nowMs();
  indicator_.invalidate();  // pin state after reset is unknown

  // read identity before sampling
  DeviceInfoReader reader {eeprom_};
  if (reader.load(info_) != DeviceInfoStatus::Ok) {
    configFaulted_ = true;
    enterFault(FaultReason::Config);
    return false;
  }

  sensor_ = sensorFor(info_.revision);
  if (sensor_ == nullptr) {
    // unreachable but guards the sample path
    configFaulted_ = true;
    enterFault(FaultReason::Config);
    return false;
  }

  // on Rev-A a 0.5 degC band is smaller than one count so floor it
  if (classifier_.hysteresis() < sensor_->resolution()) classifier_.setHysteresis(sensor_->resolution());

  if (!adc_.init(config::kSamplePeriodUs, *this) || !adc_.start()) {
    enterFault(FaultReason::NoSamples);
    return false;
  }

  // stay in fault until the first block arrives
  fault_ = FaultReason::NoSamples;
  return true;
}

void TemperatureMonitor::onBlock(std::uint8_t blockIndex) noexcept {
  static_cast<void>(queue_.push(blockIndex));
}

void TemperatureMonitor::poll() noexcept {
  const std::uint32_t now = clock_.nowMs();

  // a config fault is latched and checked first
  if (configFaulted_) {
    enterFault(FaultReason::Config);
    indicator_.apply(Indicator::patternFor(condition_, now));
    return;
  }

  // take the newest block and count the rest as skipped
  std::uint8_t idx    = 0;
  std::uint8_t newest = 0;
  bool got            = false;

  while (queue_.pop(idx)) {
    if (got) ++blocksSkipped_;
    newest = idx;
    got    = true;
  }

  if (!got) {
    // only silence past the timeout is a fault
    if ((now - lastBlockMs_) > config::kSampleTimeoutMs) enterFault(FaultReason::NoSamples);

    indicator_.apply(Indicator::patternFor(condition_, now));
    return;
  }

  lastBlockMs_ = now;

  const AdcRaw* block = adc_.block(newest);
  if (block == nullptr) {
    enterFault(FaultReason::NoSamples);
    indicator_.apply(Indicator::patternFor(condition_, now));
    return;
  }

  const AdcRaw median = filter_.apply(block);

  // rail check before scaling since a pinned converter is a wiring fault
  if (MedianFilter<config::kSamplesPerBlock>::atRail(median, config::kAdcRawMax)) {
    enterFault(FaultReason::SensorRail);
    indicator_.apply(Indicator::patternFor(condition_, now));
    return;
  }

  const MilliCelsius temp = sensor_->toTemperature(median);

  if (temp < config::kMinPlausible || temp > config::kMaxPlausible) {
    enterFault(FaultReason::Implausible);
    indicator_.apply(Indicator::patternFor(condition_, now));
    return;
  }

  // coming out of a fault judge the next reading on thresholds alone
  if (condition_ == Condition::Fault) classifier_.reset(Condition::Normal);

  lastTemp_  = temp;
  condition_ = classifier_.update(temp);
  fault_     = FaultReason::None;

  indicator_.apply(Indicator::patternFor(condition_, now));
}

const char* toString(FaultReason reason) noexcept {
  switch (reason) {
    case FaultReason::None: return "none";
    case FaultReason::Config: return "configuration unreadable";
    case FaultReason::SensorRail: return "sensor at rail (open or short)";
    case FaultReason::Implausible: return "reading outside plausible range";
    case FaultReason::NoSamples: return "no samples from acquisition";
  }
  return "unknown";
}

}  // namespace tempmon
