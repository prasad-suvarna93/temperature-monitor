#include "HostHal.hpp"

#include <cstring>

namespace tempmon::host {

bool HostEeprom::read(std::uint16_t addr, std::uint8_t* dst, std::size_t len) noexcept {
  if (busFail_) return false;

  if (static_cast<std::size_t>(addr) + len > img_.size()) return false;

  std::memcpy(dst, img_.data() + addr, len);
  return true;
}

void HostEeprom::programValid(HwRevision rev, const char* serial) noexcept {
  img_.fill(0);

  img_[0] = 0x5Au;
  img_[1] = 0xC5u;
  img_[2] = static_cast<std::uint8_t>(rev);

  // pad bytes are inside the crc
  for (std::size_t i = 0; i < kSerialLen; ++i) {
    const bool have = (serial != nullptr) && (serial[i] != '\0');
    img_[3 + i]     = have ? static_cast<std::uint8_t>(serial[i]) : 0u;
  }

  img_[kEepromRecLen - 1] = DeviceInfoReader::crc8(img_.data(), kEepromRecLen - 1);
}

void HostEeprom::programRaw(const std::uint8_t* record, std::size_t len) noexcept {
  img_.fill(0);
  if (record != nullptr) std::memcpy(img_.data(), record, (len < kEepromRecLen) ? len : kEepromRecLen);
}

void HostEeprom::corruptCrc() noexcept { img_[kEepromRecLen - 1] ^= 0x01u; }

bool HostGpio::init() noexcept {
  led_.fill(false);
  return true;
}

void HostGpio::writeLed(LedId led, bool on) noexcept {
  const auto i = static_cast<std::size_t>(led);
  if (i >= kLedCount) return;

  led_[i] = on;
  ++writes_;
}

bool HostGpio::led(LedId id) const noexcept {
  const auto i = static_cast<std::size_t>(id);
  return (i < kLedCount) && led_[i];
}

bool HostAdc::init(std::uint32_t samplePeriodUs, IBlockSink& sink) noexcept {
  if (samplePeriodUs == 0) return false;

  sink_    = &sink;
  next_    = 0;
  running_ = false;
  return true;
}

bool HostAdc::start() noexcept {
  running_ = true;
  return true;
}
void HostAdc::stop() noexcept { running_ = false; }

const AdcRaw* HostAdc::block(std::uint8_t blockIndex) const noexcept {
  if (static_cast<std::size_t>(blockIndex) >= config::kSampleBlocks) return nullptr;

  return ring_.data() + static_cast<std::size_t>(blockIndex) * config::kSamplesPerBlock;
}

void HostAdc::injectSpike(AdcRaw raw, std::size_t count) noexcept {
  spikeRaw_   = raw;
  spikeCount_ = count;
}

void HostAdc::produceBlock() noexcept {
  if (!running_ || sink_ == nullptr) return;

  AdcRaw* blk = ring_.data() + static_cast<std::size_t>(next_) * config::kSamplesPerBlock;

  for (std::size_t i = 0; i < config::kSamplesPerBlock; ++i) {
    std::int32_t sample = base_;

    if (noise_ > 0) {
      rng_              = (rng_ * 1103515245u) + 12345u;
      const auto spread = static_cast<std::int32_t>((rng_ >> 16) % (noise_ + 1u));
      sample += spread - static_cast<std::int32_t>(noise_ / 2);
    }

    if (sample < 0) sample = 0;
    if (sample > static_cast<std::int32_t>(config::kAdcRawMax)) sample = config::kAdcRawMax;

    blk[i] = static_cast<AdcRaw>(sample);
  }

  // position is irrelevant to a median
  for (std::size_t i = 0; i < spikeCount_ && i < config::kSamplesPerBlock; ++i) blk[i] = spikeRaw_;

  spikeCount_ = 0;  // one block only

  const std::uint8_t completed = next_;
  next_                        = static_cast<std::uint8_t>((next_ + 1) % config::kSampleBlocks);

  sink_->onBlock(completed);  // stand in for the dma interrupt
}

AdcRaw rawForTemperature(const ITemperatureSensor& sensor, MilliCelsius target) noexcept {
  // largest code whose temperature does not exceed the target
  std::int32_t lo = 0;
  std::int32_t hi = config::kAdcRawMax;

  while (lo < hi) {
    const std::int32_t mid = (lo + hi + 1) / 2;
    if (sensor.toTemperature(static_cast<AdcRaw>(mid)) > target)
      hi = mid - 1;
    else
      lo = mid;
  }

  return static_cast<AdcRaw>(lo);
}

}  // namespace tempmon::host
