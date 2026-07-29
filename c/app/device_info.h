// Device identity read from EEPROM the hardware revision and serial
#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include "temp_types.h"

#define SERIAL_LEN 8u

// record layout in the EEPROM
//   0x00  magic 0x5A 0xC5
//   0x02  revision
//   0x03  serial 8 bytes
//   0x0B  crc8 over the first 11 bytes
#define EE_ADDR_BASE 0x0000u
#define EE_RECORD_LEN 12u

typedef enum {
  DEVINFO_OK = 0,
  DEVINFO_ERR_BUS,
  DEVINFO_ERR_MAGIC,
  DEVINFO_ERR_CRC,
  DEVINFO_ERR_REVISION
} devinfo_status_e;

typedef struct {
  hw_rev_e revision;
  char serial[SERIAL_LEN];
} device_info_t;

// on anything but DEVINFO_OK out is left untouched
devinfo_status_e DeviceInfoLoad(device_info_t* out);

const char* DevinfoStatusStr(devinfo_status_e s);

uint8_t DeviceInfoCrc8(const uint8_t* data, uint32_t len);

#endif
