// excluded from the host build
#ifndef HOST_BUILD

#include <array>
#include <cstring>

#include "Config.hpp"
#include "Hal.hpp"
#include "TargetRegs.hpp"

namespace tempmon::target {

class TargetAdc final : public IAdc {
 public:
  [[nodiscard]] bool init(std::uint32_t samplePeriodUs, IBlockSink& sink) noexcept override {
    // 64-bit intermediate to avoid 32-bit overflow
    const auto counts =
        static_cast<std::uint32_t>((static_cast<std::uint64_t>(kTimerClkHz) * samplePeriodUs) / 1000000u);

    if (samplePeriodUs == 0 || counts == 0 || counts > 0xFFFFu) return false;

    sink_ = &sink;

    // circular dma avoids a rearm gap
    kDmaAdc->CR   = 0;  // disable before touching
    kDmaAdc->PAR  = reinterpret_cast<std::uint32_t>(&kAdcTemp->DR);
    kDmaAdc->MAR  = reinterpret_cast<std::uint32_t>(ring_.data());
    kDmaAdc->NDTR = static_cast<std::uint32_t>(ring_.size());
    kDmaAdc->IFCR = kDmaIsrHtif | kDmaIsrTcif | kDmaIsrTeif;
    kDmaAdc->CR =
        kDmaCrCirc | kDmaCrMinc | kDmaCrPsize16 | kDmaCrMsize16 | kDmaCrHtie | kDmaCrTcie | kDmaCrTeie | kDmaCrEn;

    // adc is hardware triggered the cpu never starts a conversion
    kAdcTemp->SQR  = 0;     // single channel
    kAdcTemp->SMPR = 0x03;  // long enough for the source impedance
    kAdcTemp->CR2  = kAdcCr2Adon | kAdcCr2Dma | kAdcCr2Dds | kAdcCr2ExtenRising | kAdcCr2ExtselTrgo;

    // timer sets the sampling instant
    kTimSample->PSC = 0;  // full resolution
    kTimSample->ARR = counts - 1;
    kTimSample->CR2 = kTimCr2MmsUpdate;  // update event triggers the adc
    kTimSample->EGR = kTimEgrUg;         // latch PSC and ARR now

    return true;
  }

  [[nodiscard]] bool start() noexcept override {
    kTimSample->CR1 |= kTimCr1Cen;
    return true;
  }

  void stop() noexcept override { kTimSample->CR1 &= ~kTimCr1Cen; }

  [[nodiscard]] const AdcRaw* block(std::uint8_t blockIndex) const noexcept override {
    if (static_cast<std::size_t>(blockIndex) >= config::kSampleBlocks) return nullptr;

    return ring_.data() + static_cast<std::size_t>(blockIndex) * config::kSamplesPerBlock;
  }

  // called from the interrupt handler two notifications per trip
  void onIrq() noexcept {
    const std::uint32_t status = kDmaAdc->ISR;

    if (status & kDmaIsrHtif) {
      kDmaAdc->IFCR = kDmaIsrHtif;
      notify(0, config::kSampleBlocks / 2);
    }

    if (status & kDmaIsrTcif) {
      kDmaAdc->IFCR = kDmaIsrTcif;
      notify(config::kSampleBlocks / 2, config::kSampleBlocks);
    }

    if (status & kDmaIsrTeif) {
      // bus error stop and let the timeout notice the silence
      kDmaAdc->IFCR = kDmaIsrTeif;
      kDmaAdc->CR &= ~kDmaCrEn;
    }
  }

 private:
  void notify(std::size_t first, std::size_t last) noexcept {
    if (sink_ == nullptr) return;

    for (std::size_t b = first; b < last; ++b) sink_->onBlock(static_cast<std::uint8_t>(b));
  }

  // the dma ring aligned for the burst logic
  alignas(4) std::array<AdcRaw, config::kSampleBlocks * config::kSamplesPerBlock> ring_ {};
  IBlockSink* sink_ {nullptr};
};

class TargetGpio final : public IGpio {
 public:
  [[nodiscard]] bool init() noexcept override {
    for (const auto pin : kPins) kGpioLed->MODER |= (1u << (pin * 2));  // output

    return true;
  }

  void writeLed(LedId led, bool on) noexcept override {
    const auto i = static_cast<std::size_t>(led);
    if (i >= kLedCount) return;

    // bsrr so a single store cannot be interrupted half done
    const std::uint32_t pin = kPins[i];
    kGpioLed->BSRR          = on ? (1u << pin) : (1u << (pin + 16));
  }

 private:
  static constexpr std::array<std::uint32_t, kLedCount> kPins {kPinGreen, kPinYellow, kPinRed};
};

class TargetEeprom final : public IEeprom {
 public:
  [[nodiscard]] bool init() noexcept override {
    kI2cCfg->CR1 = 1;
    return true;
  }

  [[nodiscard]] bool read(std::uint16_t addr, std::uint8_t* dst, std::size_t len) noexcept override {
    if (dst == nullptr || len == 0) return false;

    kI2cCfg->CR2 = (static_cast<std::uint32_t>(kEepromI2cAddr) << 1) | (1u << 13);
    if (!wait(kI2cSrTxe)) return false;
    kI2cCfg->DR = static_cast<std::uint32_t>(addr >> 8);
    if (!wait(kI2cSrTxe)) return false;
    kI2cCfg->DR = static_cast<std::uint32_t>(addr & 0xFFu);
    if (!wait(kI2cSrTxe)) return false;

    kI2cCfg->CR2 = (static_cast<std::uint32_t>(kEepromI2cAddr) << 1) | 1u | (1u << 13);
    for (std::size_t i = 0; i < len; ++i) {
      if (!wait(kI2cSrRxne)) return false;
      dst[i] = static_cast<std::uint8_t>(kI2cCfg->DR);
    }

    return true;
  }

 private:
  // bounded spin so a wedged bus cannot hang the boot
  static constexpr std::uint32_t kSpinLimit = 100000;

  static bool wait(std::uint32_t flag) noexcept {
    for (std::uint32_t spins = 0; spins < kSpinLimit; ++spins) {
      const std::uint32_t sr = kI2cCfg->SR;
      if (sr & kI2cSrNack) return false;
      if (sr & flag) return true;
    }
    return false;
  }
};

volatile std::uint32_t g_millis = 0;

class TargetClock final : public IClock {
 public:
  [[nodiscard]] std::uint32_t nowMs() const noexcept override {
    return g_millis;  // aligned 32-bit load is indivisible
  }
};

// constructed before main no heap
TargetAdc g_adc;
TargetGpio g_gpio;
TargetEeprom g_eeprom;
TargetClock g_clock;

IAdc& adc() noexcept { return g_adc; }
IGpio& gpio() noexcept { return g_gpio; }
IEeprom& eeprom() noexcept { return g_eeprom; }
IClock& clock() noexcept { return g_clock; }

}  // namespace tempmon::target

// extern C so the vector table finds these by name
extern "C" void DMA_ADC_IRQHandler(void) { tempmon::target::g_adc.onIrq(); }

extern "C" void SysTick_Handler(void) {
  // 1 kHz lowest priority must never delay the sample interrupt
  ++tempmon::target::g_millis;
}

#endif  // !HOST_BUILD
