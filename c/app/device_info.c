#include "device_info.h"

#include <string.h>

#include "hal_eeprom.h"

// bitwise crc8 with no lookup table
uint8_t DeviceInfoCrc8(const uint8_t* data, uint32_t len) {
  uint8_t crc = 0x00u;

  for (uint32_t i = 0u; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0u; bit < 8u; ++bit)
      crc = (uint8_t)((crc & 0x80u) ? ((uint32_t)(crc << 1) ^ 0x07u) : (uint32_t)(crc << 1));
  }
  return crc;
}

devinfo_status_e DeviceInfoLoad(device_info_t* out) {
  uint8_t rec[EE_RECORD_LEN];

  if (!HalEepromRead(EE_ADDR_BASE, rec, sizeof rec)) return DEVINFO_ERR_BUS;

  // check magic first a blank eeprom can still pass crc
  if (rec[0] != 0x5Au || rec[1] != 0xC5u) return DEVINFO_ERR_MAGIC;

  if (DeviceInfoCrc8(rec, EE_RECORD_LEN - 1u) != rec[EE_RECORD_LEN - 1u]) return DEVINFO_ERR_CRC;

  // unknown revision must stop not default to Rev-A
  if (rec[2] != (uint8_t)HW_REV_A && rec[2] != (uint8_t)HW_REV_B) return DEVINFO_ERR_REVISION;

  out->revision = (hw_rev_e)rec[2];

  memcpy(out->serial, &rec[3], SERIAL_LEN);
  out->serial[SERIAL_LEN - 1u] = '\0';  // force a terminator never trust stored bytes

  return DEVINFO_OK;
}

const char* DevinfoStatusStr(devinfo_status_e status) {
  switch (status) {
    case DEVINFO_OK: return "ok";
    case DEVINFO_ERR_BUS: return "eeprom bus error";
    case DEVINFO_ERR_MAGIC: return "bad magic (blank or foreign eeprom)";
    case DEVINFO_ERR_CRC: return "crc mismatch";
    case DEVINFO_ERR_REVISION: return "unsupported hardware revision";
    default: return "unknown";
  }
}
