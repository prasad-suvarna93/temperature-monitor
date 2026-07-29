// register map and pin wiring for the target board
#ifndef TARGET_REGS_HPP
#define TARGET_REGS_HPP

// representative not a real part

#include <cstdint>

namespace tempmon::target {

struct Tim {
  volatile std::uint32_t CR1;  // CEN in bit 0
  volatile std::uint32_t CR2;  // MMS selects the master mode output
  volatile std::uint32_t PSC;
  volatile std::uint32_t ARR;  // auto-reload value sets the period
  volatile std::uint32_t EGR;  // UG in bit 0
};

struct Adc {
  volatile std::uint32_t CR1;
  volatile std::uint32_t CR2;  // ADON DMA DDS EXTEN EXTSEL
  volatile std::uint32_t SMPR;
  volatile std::uint32_t SQR;
  volatile std::uint32_t DR;  // the dma source
};

struct DmaStream {
  volatile std::uint32_t CR;
  volatile std::uint32_t NDTR;
  volatile std::uint32_t PAR;
  volatile std::uint32_t MAR;
  volatile std::uint32_t ISR;
  volatile std::uint32_t IFCR;
};

struct Gpio {
  volatile std::uint32_t MODER;
  volatile std::uint32_t ODR;
  volatile std::uint32_t BSRR;  // atomic set and reset no read modify write
};

struct I2c {
  volatile std::uint32_t CR1;
  volatile std::uint32_t CR2;
  volatile std::uint32_t SR;
  volatile std::uint32_t DR;
};

constexpr std::uint32_t kTimCr1Cen       = 1u << 0;
constexpr std::uint32_t kTimCr2MmsUpdate = 2u << 4;
constexpr std::uint32_t kTimEgrUg        = 1u << 0;

constexpr std::uint32_t kAdcCr2Adon        = 1u << 0;
constexpr std::uint32_t kAdcCr2Dma         = 1u << 8;
constexpr std::uint32_t kAdcCr2Dds         = 1u << 9;
constexpr std::uint32_t kAdcCr2ExtenRising = 1u << 28;
constexpr std::uint32_t kAdcCr2ExtselTrgo  = 6u << 24;

constexpr std::uint32_t kDmaCrEn      = 1u << 0;
constexpr std::uint32_t kDmaCrTeie    = 1u << 2;
constexpr std::uint32_t kDmaCrHtie    = 1u << 3;
constexpr std::uint32_t kDmaCrTcie    = 1u << 4;
constexpr std::uint32_t kDmaCrCirc    = 1u << 8;
constexpr std::uint32_t kDmaCrMinc    = 1u << 10;
constexpr std::uint32_t kDmaCrPsize16 = 1u << 11;
constexpr std::uint32_t kDmaCrMsize16 = 1u << 13;

constexpr std::uint32_t kDmaIsrTeif = 1u << 3;
constexpr std::uint32_t kDmaIsrHtif = 1u << 4;
constexpr std::uint32_t kDmaIsrTcif = 1u << 5;

constexpr std::uint32_t kI2cSrBusy = 1u << 1;
constexpr std::uint32_t kI2cSrRxne = 1u << 2;
constexpr std::uint32_t kI2cSrTxe  = 1u << 3;
constexpr std::uint32_t kI2cSrNack = 1u << 4;

// treat this address as a peripheral
inline Tim* const kTimSample    = reinterpret_cast<Tim*>(0x40000000u);
inline Adc* const kAdcTemp      = reinterpret_cast<Adc*>(0x40012000u);
inline DmaStream* const kDmaAdc = reinterpret_cast<DmaStream*>(0x40026010u);
inline Gpio* const kGpioLed     = reinterpret_cast<Gpio*>(0x40020400u);
inline I2c* const kI2cCfg       = reinterpret_cast<I2c*>(0x40005400u);

// lamp wiring active high
constexpr std::uint32_t kPinGreen  = 5;
constexpr std::uint32_t kPinYellow = 6;
constexpr std::uint32_t kPinRed    = 7;

constexpr std::uint8_t kEepromI2cAddr = 0x50;
constexpr std::uint32_t kTimerClkHz   = 84000000;

}  // namespace tempmon::target

#endif  // TARGET_REGS_HPP
