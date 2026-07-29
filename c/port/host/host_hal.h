// hooks for tests to script the fake HAL
#ifndef HOST_HAL_H
#define HOST_HAL_H

#include "temp_types.h"

// call at the top of every test
void HostReset(void);

// time

void HostTimeSetMs(uint32_t ms);
void HostTimeAdvanceMs(uint32_t ms);

// eeprom

void HostEepromProgramValid(hw_rev_e rev, const char* serial);

// raw image for cases a valid record cannot express
void HostEepromProgramRaw(const uint8_t* record, uint32_t len);

void HostEepromCorruptCrc(void);

void HostEepromSetBusFail(bool fail);

// adc

void HostAdcSetRaw(adc_raw_t raw);

// what raw digit maps to this temperature on this revision
adc_raw_t HostRawForTemp(hw_rev_e rev, temp_mdc_t t_mdc);

// uniform noise with the given peak to peak span
void HostAdcSetNoise(adc_raw_t lsb_pp);

// corrupts the first count samples of the next block only
void HostAdcInjectSpike(adc_raw_t raw, uint16_t count);

// fills a block and calls the ISR callback like the DMA would
void HostAdcProduceBlock(void);

bool HostAdcRunning(void);

// lamps

bool HostLed(led_id_e led);

uint32_t HostLedWriteCount(void);

#endif  // HOST_HAL_H
