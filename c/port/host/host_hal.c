#ifdef HOST_BUILD

// software model of every HAL module for tests and the demo

#include "host_hal.h"

#include <string.h>

#include "config.h"
#include "device_info.h"
#include "hal_adc.h"
#include "hal_eeprom.h"
#include "hal_gpio.h"
#include "hal_time.h"
#include "sensor.h"

// time

static uint32_t s_now_ms;

bool HalTimeInit(void) { return true; }
uint32_t HalTimeNowMs(void) { return s_now_ms; }

void HostTimeSetMs(uint32_t ms) { s_now_ms = ms; }
void HostTimeAdvanceMs(uint32_t ms) { s_now_ms += ms; }

// eeprom

static uint8_t s_ee[EE_RECORD_LEN];
static bool s_ee_bus_fail;

bool HalEepromInit(void) { return true; }

bool HalEepromRead(uint16_t addr, uint8_t* dst, size_t len) {
  if (s_ee_bus_fail) return false;
  if ((size_t)addr + len > sizeof s_ee) return false;

  memcpy(dst, &s_ee[addr], len);
  return true;
}

void HostEepromProgramValid(hw_rev_e rev, const char* serial) {
  memset(s_ee, 0, sizeof s_ee);

  s_ee[0] = 0x5Au;
  s_ee[1] = 0xC5u;
  s_ee[2] = (uint8_t)rev;

  // pad bytes are inside the CRC
  for (uint32_t i = 0u; i < SERIAL_LEN; ++i)
    s_ee[3u + i] = (serial != NULL && serial[i] != '\0') ? (uint8_t)serial[i] : 0u;

  s_ee[EE_RECORD_LEN - 1u] = DeviceInfoCrc8(s_ee, EE_RECORD_LEN - 1u);
}

void HostEepromProgramRaw(const uint8_t* record, uint32_t len) {
  memset(s_ee, 0, sizeof s_ee);
  if (record != NULL) memcpy(s_ee, record, (len < EE_RECORD_LEN) ? len : EE_RECORD_LEN);
}

void HostEepromCorruptCrc(void) { s_ee[EE_RECORD_LEN - 1u] ^= 0x01u; }

void HostEepromSetBusFail(bool fail) { s_ee_bus_fail = fail; }

// adc

static adc_raw_t s_ring[SAMPLE_BLOCKS * SAMPLES_PER_BLOCK];
static hal_adc_block_cb_t s_cb;
static void* s_ctx;
static bool s_running;
static uint8_t s_next_block;

static adc_raw_t s_base_raw;
static adc_raw_t s_noise_pp;
static adc_raw_t s_spike_raw;
static uint16_t s_spike_count;

static uint32_t s_rng;

// seeded from a constant so runs repeat
static uint32_t RngNext(void) {
  s_rng = (s_rng * 1103515245u) + 12345u;
  return (s_rng >> 16) & 0x7FFFu;
}

static adc_raw_t ClampRaw(int32_t raw) {
  if (raw < 0) return 0u;
  if (raw > (int32_t)ADC_RAW_MAX) return (adc_raw_t)ADC_RAW_MAX;
  return (adc_raw_t)raw;
}

bool HalAdcInit(uint32_t sample_period_us, hal_adc_block_cb_t cb, void* ctx) {
  if (cb == NULL || sample_period_us == 0u) return false;

  s_cb         = cb;
  s_ctx        = ctx;
  s_next_block = 0u;
  s_running    = false;
  return true;
}

bool HalAdcStart(void) {
  s_running = true;
  return true;
}
void HalAdcStop(void) { s_running = false; }
bool HostAdcRunning(void) { return s_running; }

const adc_raw_t* HalAdcBlock(uint8_t block_index) {
  if ((uint32_t)block_index >= SAMPLE_BLOCKS) return NULL;

  return &s_ring[(uint32_t)block_index * SAMPLES_PER_BLOCK];
}

void HostAdcSetRaw(adc_raw_t raw) { s_base_raw = raw; }
void HostAdcSetNoise(adc_raw_t lsb_pp) { s_noise_pp = lsb_pp; }

void HostAdcInjectSpike(adc_raw_t raw, uint16_t count) {
  s_spike_raw   = raw;
  s_spike_count = count;
}

adc_raw_t HostRawForTemp(hw_rev_e rev, temp_mdc_t t_mdc) {
  sensor_cal_t cal;
  if (!SensorCalForRevision(rev, &cal)) return 0u;

  // inverse of SensorToMdc
  return ClampRaw(((t_mdc - cal.offset_mdc) * cal.den) / cal.num);
}

void HostAdcProduceBlock(void) {
  if (!s_running) return;

  adc_raw_t* blk = &s_ring[(uint32_t)s_next_block * SAMPLES_PER_BLOCK];

  for (uint32_t i = 0u; i < SAMPLES_PER_BLOCK; ++i) {
    int32_t raw = (int32_t)s_base_raw;

    if (s_noise_pp > 0u) raw += (int32_t)(RngNext() % ((uint32_t)s_noise_pp + 1u)) - (int32_t)(s_noise_pp / 2u);

    blk[i] = ClampRaw(raw);
  }

  // position is irrelevant to a median
  for (uint16_t i = 0u; i < s_spike_count && i < SAMPLES_PER_BLOCK; ++i) blk[i] = s_spike_raw;

  s_spike_count = 0u;

  const uint8_t completed = s_next_block;
  s_next_block            = (uint8_t)((s_next_block + 1u) % SAMPLE_BLOCKS);

  // stands in for the DMA completion interrupt
  s_cb(completed, s_ctx);
}

// gpio

static bool s_led[LED_COUNT];
static uint32_t s_led_writes;

bool HalGpioInit(void) {
  for (uint32_t i = 0u; i < (uint32_t)LED_COUNT; ++i) s_led[i] = false;

  return true;
}

void HalGpioWriteLed(led_id_e led, bool on) {
  if ((uint32_t)led >= (uint32_t)LED_COUNT) return;

  s_led[led] = on;
  s_led_writes++;
}

bool HostLed(led_id_e led) { return ((uint32_t)led < (uint32_t)LED_COUNT) && s_led[led]; }
uint32_t HostLedWriteCount(void) { return s_led_writes; }

void HostReset(void) {
  s_now_ms      = 0u;
  s_ee_bus_fail = false;
  memset(s_ee, 0, sizeof s_ee);

  s_cb          = NULL;
  s_ctx         = NULL;
  s_running     = false;
  s_next_block  = 0u;
  s_base_raw    = 0u;
  s_noise_pp    = 0u;
  s_spike_raw   = 0u;
  s_spike_count = 0u;
  s_rng         = 0x13579BDFu;
  memset(s_ring, 0, sizeof s_ring);

  s_led_writes = 0u;
  (void)HalGpioInit();
}

#endif  // HOST_BUILD
