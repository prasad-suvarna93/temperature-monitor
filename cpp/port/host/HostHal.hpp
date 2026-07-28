#ifndef HOST_HAL_HPP
#define HOST_HAL_HPP

#include <array>

#include "Config.hpp"
#include "DeviceInfo.hpp"
#include "Hal.hpp"
#include "TemperatureSensor.hpp"

namespace tempmon::host {

class HostClock final : public IClock {
 public:
  [[nodiscard]] std::uint32_t nowMs() const noexcept override { return now_; }

  void set(std::uint32_t ms) noexcept { now_ = ms; }
  void advance(std::uint32_t ms) noexcept { now_ += ms; }

 private:
  std::uint32_t now_ {0};
};

class HostEeprom final : public IEeprom {
 public:
  [[nodiscard]] bool init() noexcept override { return true; }
  [[nodiscard]] bool read(std::uint16_t addr, std::uint8_t* dst, std::size_t len) noexcept override;

  // valid by construction
  void programValid(HwRevision rev, const char* serial) noexcept;

  // raw image for what a valid record cannot express
  void programRaw(const std::uint8_t* record, std::size_t len) noexcept;

  void corruptCrc() noexcept;
  void setBusFail(bool fail) noexcept { busFail_ = fail; }

 private:
  std::array<std::uint8_t, kEepromRecLen> img_ {};
  bool busFail_ {false};
};

class HostGpio final : public IGpio {
 public:
  [[nodiscard]] bool init() noexcept override;
  void writeLed(LedId led, bool on) noexcept override;

  [[nodiscard]] bool led(LedId id) const noexcept;

  // counts writes that reached a pin
  [[nodiscard]] std::uint32_t writeCount() const noexcept { return writes_; }

 private:
  std::array<bool, kLedCount> led_ {};
  std::uint32_t writes_ {0};
};

class HostAdc final : public IAdc {
 public:
  [[nodiscard]] bool init(std::uint32_t samplePeriodUs, IBlockSink& sink) noexcept override;
  [[nodiscard]] bool start() noexcept override;
  void stop() noexcept override;
  [[nodiscard]] const AdcRaw* block(std::uint8_t blockIndex) const noexcept override;

  // the value the source sits at before noise
  void setRaw(AdcRaw raw) noexcept { base_ = raw; }

  // peak to peak noise in digits
  void setNoise(AdcRaw peakToPeak) noexcept { noise_ = peakToPeak; }

  // corrupt the first samples of the next block only
  void injectSpike(AdcRaw raw, std::size_t count) noexcept;

  // fill the next block and call the sink
  void produceBlock() noexcept;

  [[nodiscard]] bool running() const noexcept { return running_; }

 private:
  std::array<AdcRaw, config::kSampleBlocks * config::kSamplesPerBlock> ring_ {};
  IBlockSink* sink_ {nullptr};
  bool running_ {false};
  std::uint8_t next_ {0};

  AdcRaw base_ {0};
  AdcRaw noise_ {0};
  AdcRaw spikeRaw_ {0};
  std::size_t spikeCount_ {0};
  std::uint32_t rng_ {0x13579BDFu};
};

// raw digit for this temperature by binary search
[[nodiscard]] AdcRaw rawForTemperature(const ITemperatureSensor& sensor, MilliCelsius target) noexcept;

}  // namespace tempmon::host

#endif  // HOST_HAL_HPP
