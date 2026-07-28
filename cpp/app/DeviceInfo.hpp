#ifndef DEVICE_INFO_HPP
#define DEVICE_INFO_HPP

#include <array>
#include <cstddef>

#include "Hal.hpp"
#include "Types.hpp"

namespace tempmon {

constexpr std::size_t kSerialLen    = 8;
constexpr std::uint16_t kEepromBase = 0x0000;
constexpr std::size_t kEepromRecLen = 12;

enum class DeviceInfoStatus : std::uint8_t {
  Ok = 0,
  BusError,
  BadMagic,
  BadCrc,
  UnsupportedRevision
};

struct DeviceInfo {
  HwRevision revision {HwRevision::RevA};
  std::array<char, kSerialLen> serial {};
};

class DeviceInfoReader {
 public:
  explicit DeviceInfoReader(IEeprom& eeprom) noexcept : eeprom_ {eeprom} {}

  // out is untouched unless ok
  [[nodiscard]] DeviceInfoStatus load(DeviceInfo& out) noexcept;

  [[nodiscard]] static std::uint8_t crc8(const std::uint8_t* data, std::size_t len) noexcept;

 private:
  IEeprom& eeprom_;
};

const char* toString(DeviceInfoStatus s) noexcept;

}  // namespace tempmon

#endif  // DEVICE_INFO_HPP
