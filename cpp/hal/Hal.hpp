// hardware interfaces the application depends on
#ifndef HAL_HPP
#define HAL_HPP

#include <cstddef>

#include "Types.hpp"

namespace tempmon {

// called from interrupt context once per completed block
class IBlockSink {
 public:
  virtual ~IBlockSink() = default;

  // interrupt context must not block
  virtual void onBlock(std::uint8_t blockIndex) noexcept = 0;
};

class IAdc {
 public:
  virtual ~IAdc() = default;

  // program the timer and dma but do not start
  [[nodiscard]] virtual bool init(std::uint32_t samplePeriodUs, IBlockSink& sink) noexcept = 0;

  [[nodiscard]] virtual bool start() noexcept = 0;
  virtual void stop() noexcept                = 0;

  // valid until the dma wraps onto it
  [[nodiscard]] virtual const AdcRaw* block(std::uint8_t blockIndex) const noexcept = 0;
};

class IGpio {
 public:
  virtual ~IGpio() = default;

  [[nodiscard]] virtual bool init() noexcept = 0;

  // idempotent
  virtual void writeLed(LedId led, bool on) noexcept = 0;
};

class IEeprom {
 public:
  virtual ~IEeprom() = default;

  [[nodiscard]] virtual bool init() noexcept = 0;

  // called once at boot before the sample timer starts
  [[nodiscard]] virtual bool read(std::uint16_t addr, std::uint8_t* dst, std::size_t len) noexcept = 0;
};

class IClock {
 public:
  virtual ~IClock() = default;

  // monotonic and callers compare by subtraction so wrap is fine
  [[nodiscard]] virtual std::uint32_t nowMs() const noexcept = 0;
};

}  // namespace tempmon

#endif  // HAL_HPP
