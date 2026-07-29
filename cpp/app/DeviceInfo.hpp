// device identity read from EEPROM the hardware revision and serial
#ifndef DEVICE_INFO_HPP
#define DEVICE_INFO_HPP

#include <array>
#include <cstddef>

#include "Hal.hpp"
#include "Types.hpp"

namespace tempmon {

constexpr std::size_t kSerialLen    = 8;

// record layout in the EEPROM
//   0x00  magic 0x5A 0xC5
//   0x02  revision
//   0x03  serial 8 bytes
//   0x0B  crc8 over the first 11 bytes
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
