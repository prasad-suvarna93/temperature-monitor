#include "DeviceInfo.hpp"

#include "TemperatureSensor.hpp"

namespace tempmon {

std::uint8_t DeviceInfoReader::crc8(const std::uint8_t* data, std::size_t len) noexcept {
  // crc-8 poly 0x07
  std::uint8_t crc = 0x00;

  for (std::size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t shifted = static_cast<std::uint32_t>(crc) << 1;
      crc                         = static_cast<std::uint8_t>((crc & 0x80u) ? (shifted ^ 0x07u) : shifted);
    }
  }
  return crc;
}

DeviceInfoStatus DeviceInfoReader::load(DeviceInfo& out) noexcept {
  std::array<std::uint8_t, kEepromRecLen> rec {};

  if (!eeprom_.read(kEepromBase, rec.data(), rec.size())) return DeviceInfoStatus::BusError;

  // check magic first a blank eeprom can pass crc by chance
  if (rec[0] != 0x5Au || rec[1] != 0xC5u) return DeviceInfoStatus::BadMagic;

  if (crc8(rec.data(), kEepromRecLen - 1) != rec[kEepromRecLen - 1]) return DeviceInfoStatus::BadCrc;

  // unknown revision is a stop not a default
  const auto rev = static_cast<HwRevision>(rec[2]);
  if (sensorFor(rev) == nullptr) return DeviceInfoStatus::UnsupportedRevision;

  out.revision = rev;

  for (std::size_t i = 0; i < kSerialLen; ++i) out.serial[i] = static_cast<char>(rec[3 + i]);

  out.serial[kSerialLen - 1] = '\0';  // stored data may not terminate

  return DeviceInfoStatus::Ok;
}

const char* toString(DeviceInfoStatus status) noexcept {
  switch (status) {
    case DeviceInfoStatus::Ok: return "ok";
    case DeviceInfoStatus::BusError: return "eeprom bus error";
    case DeviceInfoStatus::BadMagic: return "bad magic (blank or foreign eeprom)";
    case DeviceInfoStatus::BadCrc: return "crc mismatch";
    case DeviceInfoStatus::UnsupportedRevision: return "unsupported hardware revision";
  }
  return "unknown";
}

}  // namespace tempmon
