#ifdef HOST_BUILD

// scripted walk through the bands and the fault paths

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "hal_time.h"
#include "host_hal.h"
#include "indicator.h"
#include "monitor.h"

#define ANSI_RESET "\033[0m"
#define ANSI_DIM "\033[2m"
#define ANSI_GREEN "\033[1;32m"
#define ANSI_YELLOW "\033[1;33m"
#define ANSI_RED "\033[1;31m"

// plain output when not a terminal
static bool use_colour = false;

static void ColourInit(void) { use_colour = isatty(STDOUT_FILENO) && (getenv("NO_COLOR") == NULL); }

static void PrintLamp(bool lit, char glyph, const char* colour) {
  if (!use_colour) {
    putchar(lit ? glyph : '.');
  } else if (lit) {
    printf("%s%c" ANSI_RESET, colour, glyph);
  } else {
    printf(ANSI_DIM "." ANSI_RESET);
  }
}

static void PrintTemp(temp_mdc_t temp) {
  const long whole = (long)(temp / MDC_PER_DEGC);
  long frac        = (long)(temp % MDC_PER_DEGC);
  if (frac < 0) frac = -frac;
  printf("%4ld.%03ld", whole, frac);
}

static void PrintRow(const monitor_t* mon, adc_raw_t raw, const char* note) {
  const led_pattern_t pattern = IndicatorPattern(MonitorCondition(mon), HalTimeNowMs());

  printf("  %6u ms | raw %5u | ", HalTimeNowMs(), raw);
  PrintTemp(MonitorLastTempMdc(mon));
  printf(" degC | %-15s | ", ConditionStr(MonitorCondition(mon)));
  PrintLamp(pattern.on[LED_GREEN], 'G', ANSI_GREEN);
  putchar(' ');
  PrintLamp(pattern.on[LED_YELLOW], 'Y', ANSI_YELLOW);
  putchar(' ');
  PrintLamp(pattern.on[LED_RED], 'R', ANSI_RED);
  printf(" | %s\n", note ? note : "");
}

// one block and one pass of the main loop
static void Step(monitor_t* mon, hw_rev_e rev, temp_mdc_t temp, const char* note) {
  const adc_raw_t raw = HostRawForTemp(rev, temp);

  HostAdcSetRaw(raw);
  HostAdcProduceBlock();
  HostTimeAdvanceMs(BLOCK_PERIOD_US / 1000u);
  MonitorPoll(mon, HalTimeNowMs());
  PrintRow(mon, raw, note);
}

static void Header(const char* title) {
  printf("\n=== %s\n", title);
  printf("     time   |    raw    |    temperature | condition       | lamps |\n");
}

static void RunProfile(hw_rev_e rev, const char* label) {
  monitor_t mon;

  HostReset();
  HostEepromProgramValid(rev, "ABC1234");

  if (!MonitorInit(&mon, HalTimeNowMs())) {
    printf("  init failed: %s\n", FaultReasonStr(MonitorFault(&mon)));
    return;
  }

  // profile is written in counts not fixed temperatures
  const temp_mdc_t lsb  = mon.cal.num / mon.cal.den;
  const temp_mdc_t hyst = mon.class_cfg.hysteresis_mdc;

  Header(label);
  printf("  serial %s  %s\n", mon.info.serial, mon.cal.name);
  printf("  resolution %ld.%03ld degC  hysteresis %ld.%03ld degC\n",
         (long)(lsb / 1000),
         (long)(lsb % 1000),
         (long)(hyst / 1000),
         (long)(hyst % 1000));

  // warm-up through the bands
  Step(&mon, rev, 20000, "cold start normal");
  Step(&mon, rev, 70000, NULL);
  Step(&mon, rev, 85000 - lsb, "one count under the warning threshold");
  Step(&mon, rev, 85000, "exactly 85.000 and the rule says warn at 85");
  Step(&mon, rev, 95000, NULL);
  Step(&mon, rev, 105000 - lsb, "one count under critical");
  Step(&mon, rev, 105000, "exactly 105.000 so critical");
  Step(&mon, rev, 112000, NULL);

  // cooling down through the hysteresis bands
  Step(&mon, rev, 105000 - hyst, "at the release point still critical");
  Step(&mon, rev, 105000 - hyst - lsb, "one count below it released to warning");
  Step(&mon, rev, 85000 - hyst, "at the release point still warning");
  Step(&mon, rev, 85000 - hyst - lsb, "one count below it released to normal");

  // cold excursion the other red band
  Step(&mon, rev, 10000, NULL);
  Step(&mon, rev, 5000, "exactly 5.000 and the rule says normal at 5");
  Step(&mon, rev, 5000 - lsb, "one count below 5 degC so critical cold");
  Step(&mon, rev, 5000 + hyst - lsb, "at the release point still critical");
  Step(&mon, rev, 5000 + hyst, "released to normal");
  Step(&mon, rev, 25000, NULL);
}

static void RunFaults(void) {
  monitor_t mon;

  // glitched conversions

  HostReset();
  HostEepromProgramValid(HW_REV_B, "ABC1234");
  (void)MonitorInit(&mon, HalTimeNowMs());

  Header("glitch rejection");
  Step(&mon, HW_REV_B, 50000, "steady at 50 degC");

  HostAdcSetRaw(HostRawForTemp(HW_REV_B, 50000));
  HostAdcInjectSpike((adc_raw_t)ADC_RAW_MAX, 20u);
  HostAdcProduceBlock();
  HostTimeAdvanceMs(BLOCK_PERIOD_US / 1000u);
  MonitorPoll(&mon, HalTimeNowMs());
  PrintRow(&mon, HostRawForTemp(HW_REV_B, 50000), "20 of 64 samples forced to full scale and the median ignores them");

  // sensor at an end stop

  Header("sensor open or shorted");
  HostAdcSetRaw((adc_raw_t)ADC_RAW_MAX);
  HostAdcProduceBlock();
  HostTimeAdvanceMs(BLOCK_PERIOD_US / 1000u);
  MonitorPoll(&mon, HalTimeNowMs());
  PrintRow(&mon, (adc_raw_t)ADC_RAW_MAX, FaultReasonStr(MonitorFault(&mon)));

  Step(&mon, HW_REV_B, 25000, "signal restored and it recovers on its own");

  // acquisition stops

  Header("acquisition chain goes quiet");
  HostTimeAdvanceMs(50u);
  MonitorPoll(&mon, HalTimeNowMs());
  PrintRow(&mon, 0u, "50 ms of silence is inside the timeout so hold the last reading");

  HostTimeAdvanceMs(100u);
  MonitorPoll(&mon, HalTimeNowMs());
  PrintRow(&mon, 0u, FaultReasonStr(MonitorFault(&mon)));

  printf("     (red lamp blinks at 2 Hz here)\n");
  for (uint32_t i = 0u; i < 4u; ++i) {
    HostTimeAdvanceMs(125u);
    MonitorPoll(&mon, HalTimeNowMs());
    PrintRow(&mon, 0u, NULL);
  }

  // unreadable identity record

  Header("identity record unreadable at boot");
  HostReset();
  HostEepromSetBusFail(true);
  if (!MonitorInit(&mon, HalTimeNowMs())) printf("  MonitorInit failed: %s\n", FaultReasonStr(MonitorFault(&mon)));

  printf("  acquisition started: %s\n", HostAdcRunning() ? "yes" : "no");
  printf("  revision unknown so a digit cannot become a temperature\n");
  printf("  the device faults instead of guessing\n");

  // a revision this build does not know

  Header("identity record names an unsupported revision");
  HostReset();
  {
    uint8_t rec[EE_RECORD_LEN] = {0x5Au, 0xC5u, 0x07u, 'Q', 'Q', 'Q', '0', '0', '0', '1', 0x00u};
    rec[EE_RECORD_LEN - 1u]    = DeviceInfoCrc8(rec, EE_RECORD_LEN - 1u);
    HostEepromProgramRaw(rec, sizeof rec);
  }
  if (!MonitorInit(&mon, HalTimeNowMs())) printf("  MonitorInit failed: %s\n", FaultReasonStr(MonitorFault(&mon)));

  printf("  refuses to run instead of guessing Rev-A\n");
  printf("  a Rev-B board read as Rev-A shows 8.5 degC at a real 85 degC\n");
}

int main(void) {
  ColourInit();

  printf("temperature monitor PC demonstration (C)\n");
  printf("sampling %u us  %u samples per block  block every %u us\n",
         SAMPLE_PERIOD_US,
         SAMPLES_PER_BLOCK,
         BLOCK_PERIOD_US);

  RunProfile(HW_REV_B, "Rev-B  (0.1 degC per digit)");
  RunProfile(HW_REV_A, "Rev-A  (1.0 degC per digit)");
  RunFaults();

  printf("\n");
  return 0;
}

#endif  // HOST_BUILD
