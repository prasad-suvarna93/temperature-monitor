#ifndef HOST_BUILD

// target entry point with the whole loop driven by interrupts
// nothing here is time critical
#include "hal_eeprom.h"
#include "hal_time.h"
#include "monitor.h"

// static storage no heap
static monitor_t g_monitor;

extern void BoardInit(void);  // clocks pins NVIC from startup code
extern void WdtKick(void);

int main(void) {
  BoardInit();
  (void)HalTimeInit();
  (void)HalEepromInit();

  // a fault here is annunciated not fatal
  (void)MonitorInit(&g_monitor, HalTimeNowMs());

  for (;;) {
    MonitorPoll(&g_monitor, HalTimeNowMs());
    WdtKick();

    // sleep acquisition runs on timer and DMA with the core stopped
    __asm volatile("wfi");
  }
}

#endif  // !HOST_BUILD
