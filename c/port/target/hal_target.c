#ifndef HOST_BUILD

// real hardware behind the HAL for the target build

#include <stddef.h>

#include "compiler.h"
#include "config.h"
#include "hal_adc.h"
#include "hal_eeprom.h"
#include "hal_gpio.h"
#include "hal_time.h"
#include "target_regs.h"

// adc

// DMA writes into this ring forever after init
static adc_raw_t s_ring[SAMPLE_BLOCKS * SAMPLES_PER_BLOCK] __attribute__((aligned(4)));

static hal_adc_block_cb_t s_cb;
static void* s_ctx;

static uint32_t TimerPeriodCounts(uint32_t period_us) {
  // 64-bit math so the multiply does not overflow
  return (uint32_t)(((uint64_t)TIMER_CLK_HZ * period_us) / 1000000u);
}

bool HalAdcInit(uint32_t sample_period_us, hal_adc_block_cb_t cb, void* ctx) {
  if (cb == NULL || sample_period_us == 0u) return false;

  s_cb  = cb;
  s_ctx = ctx;

  // circular so there is no re-arming gap
  DMA_ADC->CR   = 0u;  // disable before touching
  DMA_ADC->PAR  = (uint32_t)&ADC_TEMP->DR;
  DMA_ADC->MAR  = (uint32_t)s_ring;
  DMA_ADC->NDTR = (uint32_t)(SAMPLE_BLOCKS * SAMPLES_PER_BLOCK);
  DMA_ADC->IFCR = DMA_ISR_HTIF | DMA_ISR_TCIF | DMA_ISR_TEIF;
  DMA_ADC->CR   = DMA_CR_CIRC | DMA_CR_MINC | DMA_CR_PSIZE_16 | DMA_CR_MSIZE_16 | DMA_CR_HTIE | DMA_CR_TCIE |
                  DMA_CR_TEIE | DMA_CR_EN;

  // hardware triggered so the CPU never starts a conversion
  ADC_TEMP->SQR  = 0u;     // single channel
  ADC_TEMP->SMPR = 0x03u;  // sample time for the source impedance
  ADC_TEMP->CR2  = ADC_CR2_ADON | ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_EXTEN_RISING | ADC_CR2_EXTSEL_TIM_TRGO;

  // timer sets the sampling instant
  const uint32_t counts = TimerPeriodCounts(sample_period_us);
  if (counts == 0u || counts > 0xFFFFu) return false;  // unreachable at 100 us

  TIM_SAMPLE->PSC = 0u;  // full resolution
  TIM_SAMPLE->ARR = counts - 1u;
  TIM_SAMPLE->CR2 = TIM_CR2_MMS_UPDATE;  // update event drives TRGO then the ADC
  TIM_SAMPLE->EGR = TIM_EGR_UG;          // latch PSC/ARR now

  return true;
}

bool HalAdcStart(void) {
  TIM_SAMPLE->CR1 |= TIM_CR1_CEN;
  return true;
}

void HalAdcStop(void) { TIM_SAMPLE->CR1 &= ~TIM_CR1_CEN; }

const adc_raw_t* HalAdcBlock(uint8_t block_index) {
  if ((uint32_t)block_index >= SAMPLE_BLOCKS) return NULL;

  return &s_ring[(uint32_t)block_index * SAMPLES_PER_BLOCK];
}

// the isr

// least work possible so worst case equals best case
FAST_TEXT
void DMA_ADC_IRQHandler(void) {
  const uint32_t status = DMA_ADC->ISR;

  // half and full flags each hand over half of the ring
  if (status & DMA_ISR_HTIF) {
    DMA_ADC->IFCR = DMA_ISR_HTIF;
    for (uint8_t b = 0u; b < (uint8_t)(SAMPLE_BLOCKS / 2u); ++b) s_cb(b, s_ctx);
  }

  if (status & DMA_ISR_TCIF) {
    DMA_ADC->IFCR = DMA_ISR_TCIF;
    for (uint8_t b = (uint8_t)(SAMPLE_BLOCKS / 2u); b < (uint8_t)SAMPLE_BLOCKS; ++b) s_cb(b, s_ctx);
  }

  if (status & DMA_ISR_TEIF) {
    // bus error so stop and let the timeout notice the silence
    DMA_ADC->IFCR = DMA_ISR_TEIF;
    DMA_ADC->CR &= ~DMA_CR_EN;
  }
}

// gpio

static const uint32_t LED_PIN[LED_COUNT] = {
    [LED_GREEN]  = LED_PIN_GREEN,
    [LED_YELLOW] = LED_PIN_YELLOW,
    [LED_RED]    = LED_PIN_RED,
};

bool HalGpioInit(void) {
  for (uint32_t i = 0u; i < (uint32_t)LED_COUNT; ++i) GPIO_LED->MODER |= (1u << (LED_PIN[i] * 2u));  // output

  return true;
}

void HalGpioWriteLed(led_id_e led, bool on) {
  if ((uint32_t)led >= (uint32_t)LED_COUNT) return;

  // single store that cannot be interrupted half done
  const uint32_t pin = LED_PIN[led];
  GPIO_LED->BSRR     = on ? (1u << pin) : (1u << (pin + 16u));
}

// eeprom

// bounded spin runs once at boot so nothing waits on it
#define I2C_SPIN_LIMIT 100000u

static bool I2cWait(uint32_t flag) {
  for (uint32_t spins = 0u; spins < I2C_SPIN_LIMIT; ++spins) {
    const uint32_t sr = I2C_CFG->SR;
    if (sr & I2C_SR_NACK) return false;
    if (sr & flag) return true;
  }
  return false;
}

bool HalEepromInit(void) {
  I2C_CFG->CR1 = 1u;
  return true;
}

bool HalEepromRead(uint16_t addr, uint8_t* dst, size_t len) {
  if (dst == NULL || len == 0u) return false;

  I2C_CFG->CR2 = ((uint32_t)EEPROM_I2C_ADDR << 1) | (1u << 13);  // start write
  if (!I2cWait(I2C_SR_TXE)) return false;
  I2C_CFG->DR = (uint8_t)(addr >> 8);
  if (!I2cWait(I2C_SR_TXE)) return false;
  I2C_CFG->DR = (uint8_t)(addr & 0xFFu);
  if (!I2cWait(I2C_SR_TXE)) return false;

  I2C_CFG->CR2 = ((uint32_t)EEPROM_I2C_ADDR << 1) | 1u | (1u << 13);  // restart read
  for (size_t i = 0u; i < len; ++i) {
    if (!I2cWait(I2C_SR_RXNE)) return false;
    dst[i] = (uint8_t)I2C_CFG->DR;
  }

  return true;
}

// time

static volatile uint32_t s_millis;

bool HalTimeInit(void) {
  s_millis = 0u;
  return true;  // SysTick is armed in startup code
}

// lowest priority so it never delays sampling
void SysTick_Handler(void) { s_millis++; }

uint32_t HalTimeNowMs(void) { return s_millis; }  // aligned 32-bit load is indivisible

#endif  // !HOST_BUILD
