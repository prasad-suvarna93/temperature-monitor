// nothing time critical here just service and sleep
#ifndef HOST_BUILD

#include "Hal.hpp"
#include "TemperatureMonitor.hpp"

namespace tempmon::target {
IAdc& adc() noexcept;
IGpio& gpio() noexcept;
IEeprom& eeprom() noexcept;
IClock& clock() noexcept;
}  // namespace tempmon::target

extern "C" {
void board_init(void);  // clocks pins nvic
void wdt_kick(void);
}

// static storage no heap
static tempmon::TemperatureMonitor g_monitor {
    tempmon::target::adc(), tempmon::target::gpio(), tempmon::target::eeprom(), tempmon::target::clock()};

int main() {
  board_init();
  static_cast<void>(tempmon::target::eeprom().init());
  static_cast<void>(tempmon::target::gpio().init());

  // failure here is not fatal the fault is annunciated
  static_cast<void>(g_monitor.init());

  for (;;) {
    g_monitor.poll();
    wdt_kick();

    // sleep until something happens the acquisition runs with the core stopped
    __asm volatile("wfi");
  }
}

#endif  // !HOST_BUILD
