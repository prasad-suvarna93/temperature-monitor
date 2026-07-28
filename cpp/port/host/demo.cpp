#ifdef HOST_BUILD

#include <unistd.h>

#include <cstdio>
#include <cstdlib>

#include "HostHal.hpp"
#include "Indicator.hpp"
#include "TemperatureMonitor.hpp"

using namespace tempmon;
using namespace tempmon::host;

namespace {

constexpr const char* kAnsiReset  = "\033[0m";
constexpr const char* kAnsiDim    = "\033[2m";
constexpr const char* kAnsiGreen  = "\033[1;32m";
constexpr const char* kAnsiYellow = "\033[1;33m";
constexpr const char* kAnsiRed    = "\033[1;31m";

// colour only for a human at a terminal
bool useColour = false;

void colourInit() { useColour = isatty(STDOUT_FILENO) != 0 && std::getenv("NO_COLOR") == nullptr; }

void printLamp(bool lit, char glyph, const char* colour) {
  if (!useColour) {
    std::putchar(lit ? glyph : '.');
  } else if (lit) {
    std::printf("%s%c%s", colour, glyph, kAnsiReset);
  } else {
    std::printf("%s.%s", kAnsiDim, kAnsiReset);
  }
}

struct Device {
  HostAdc adc;
  HostGpio gpio;
  HostEeprom eeprom;
  HostClock clock;
  TemperatureMonitor monitor {adc, gpio, eeprom, clock};
};

void printTemp(MilliCelsius temp) {
  const long whole = temp.count() / kMdcPerDegC;
  long frac        = temp.count() % kMdcPerDegC;
  if (frac < 0) frac = -frac;
  std::printf("%4ld.%03ld", whole, frac);
}

void printRow(const Device& dev, AdcRaw raw, const char* note) {
  const auto pattern = Indicator::patternFor(dev.monitor.condition(), dev.clock.nowMs());

  std::printf("  %6u ms | raw %5u | ", dev.clock.nowMs(), raw);
  printTemp(dev.monitor.temperature());
  std::printf(" degC | %-15s | ", toString(dev.monitor.condition()));
  printLamp(pattern[static_cast<std::size_t>(LedId::Green)], 'G', kAnsiGreen);
  std::putchar(' ');
  printLamp(pattern[static_cast<std::size_t>(LedId::Yellow)], 'Y', kAnsiYellow);
  std::putchar(' ');
  printLamp(pattern[static_cast<std::size_t>(LedId::Red)], 'R', kAnsiRed);
  std::printf(" | %s\n", note ? note : "");
}

// one block then one pass of the main loop
void step(Device& dev, MilliCelsius temp, const char* note = nullptr) {
  const AdcRaw raw = rawForTemperature(*dev.monitor.sensor(), temp);

  dev.adc.setRaw(raw);
  dev.adc.produceBlock();
  dev.clock.advance(config::kBlockPeriodUs / 1000);
  dev.monitor.poll();
  printRow(dev, raw, note);
}

void header(const char* title) {
  std::printf("\n=== %s\n", title);
  std::printf("     time   |    raw    |    temperature | condition       | lamps |\n");
}

void runProfile(HwRevision rev, const char* label) {
  Device dev;
  dev.eeprom.programValid(rev, "ABC1234");

  if (!dev.monitor.init()) {
    std::printf("  init failed: %s\n", toString(dev.monitor.fault()));
    return;
  }

  const MilliCelsius lsb  = dev.monitor.sensor()->resolution();
  const MilliCelsius hyst = dev.monitor.classifier().hysteresis();

  header(label);
  std::printf("  serial %s, %s\n", dev.monitor.info().serial.data(), dev.monitor.sensor()->name());
  std::printf("  resolution %ld.%03ld degC, hysteresis %ld.%03ld degC\n",
              static_cast<long>(lsb.count() / 1000),
              static_cast<long>(lsb.count() % 1000),
              static_cast<long>(hyst.count() / 1000),
              static_cast<long>(hyst.count() % 1000));

  step(dev, degC(20), "cold start, normal");
  step(dev, degC(70));
  step(dev, config::kWarning - lsb, "one count under the warning threshold");
  step(dev, config::kWarning, "exactly 85.000 -- requirement says >=, so warn");
  step(dev, degC(95));
  step(dev, config::kCriticalHot - lsb, "one count under critical");
  step(dev, config::kCriticalHot, "exactly 105.000 -- critical");
  step(dev, degC(112));

  step(dev, config::kCriticalHot - hyst, "at the release point: still critical");
  step(dev, config::kCriticalHot - hyst - lsb, "one count below it: released to warning");
  step(dev, config::kWarning - hyst, "at the release point: still warning");
  step(dev, config::kWarning - hyst - lsb, "one count below it: released to normal");

  step(dev, degC(10));
  step(dev, config::kCriticalCold, "exactly 5.000 -- requirement says <, so normal");
  step(dev, config::kCriticalCold - lsb, "one count below 5 degC: critical, cold");
  step(dev, config::kCriticalCold + hyst - lsb, "at the release point: still critical");
  step(dev, config::kCriticalCold + hyst, "released to normal");
  step(dev, degC(25));
}

void runFaults() {
  Device dev;
  dev.eeprom.programValid(HwRevision::RevB, "ABC1234");
  static_cast<void>(dev.monitor.init());

  header("glitch rejection");
  step(dev, degC(50), "steady at 50 degC");

  dev.adc.setRaw(rawForTemperature(*dev.monitor.sensor(), degC(50)));
  dev.adc.injectSpike(config::kAdcRawMax, 20);
  dev.adc.produceBlock();
  dev.clock.advance(config::kBlockPeriodUs / 1000);
  dev.monitor.poll();
  printRow(dev,
           rawForTemperature(*dev.monitor.sensor(), degC(50)),
           "20 of 64 conversions slammed to full scale -- median ignores them");

  header("sensor open or shorted");
  dev.adc.setRaw(config::kAdcRawMax);
  dev.adc.produceBlock();
  dev.clock.advance(config::kBlockPeriodUs / 1000);
  dev.monitor.poll();
  printRow(dev, config::kAdcRawMax, toString(dev.monitor.fault()));

  step(dev, degC(25), "signal restored -- recovers on its own");

  header("acquisition chain goes quiet");
  dev.clock.advance(50);
  dev.monitor.poll();
  printRow(dev, 0, "50 ms of silence: inside the timeout, holds last reading");

  dev.clock.advance(100);
  dev.monitor.poll();
  printRow(dev, 0, toString(dev.monitor.fault()));

  std::printf("     (red lamp is blinking at 2 Hz here -- sample the phase)\n");
  for (int i = 0; i < 4; ++i) {
    dev.clock.advance(125);
    dev.monitor.poll();
    printRow(dev, 0, nullptr);
  }

  header("identity record unreadable at boot");
  {
    Device bad;
    bad.eeprom.setBusFail(true);
    if (!bad.monitor.init()) std::printf("  init failed: %s\n", toString(bad.monitor.fault()));

    std::printf("  acquisition started: %s\n", bad.adc.running() ? "yes" : "no");
    std::printf(
        "  -- the revision is unknown, so no digit can be turned into a\n"
        "     temperature. The device annunciates and does not pretend.\n");
  }

  header("identity record names an unsupported revision");
  {
    Device bad;
    std::array<std::uint8_t, kEepromRecLen> rec {0x5Au, 0xC5u, 0x07u, 'Q', 'Q', 'Q', '0', '0', '0', '1', 0x00u};
    rec[kEepromRecLen - 1] = DeviceInfoReader::crc8(rec.data(), kEepromRecLen - 1);
    bad.eeprom.programRaw(rec.data(), rec.size());

    if (!bad.monitor.init()) std::printf("  init failed: %s\n", toString(bad.monitor.fault()));

    std::printf(
        "  -- refusing to run beats defaulting to Rev-A: a Rev-B board read\n"
        "     as Rev-A shows 8.5 degC while it sits at 85 degC.\n");
  }
}

// two devices in one process
void runTwoAtOnce() {
  header("two devices, different revisions, one process");

  Device devA;
  Device devB;
  devA.eeprom.programValid(HwRevision::RevA, "AAA0001");
  devB.eeprom.programValid(HwRevision::RevB, "BBB0002");
  static_cast<void>(devA.monitor.init());
  static_cast<void>(devB.monitor.init());

  step(devA, degC(90), "device A (Rev-A) at 90 degC");
  step(devB, degC(20), "device B (Rev-B) at 20 degC");
}

}  // namespace

int main() {
  colourInit();

  std::printf("temperature monitor -- PC demonstration (C++)\n");
  std::printf("sampling %u us, %zu samples per block, block every %u us\n",
              config::kSamplePeriodUs,
              config::kSamplesPerBlock,
              config::kBlockPeriodUs);

  runProfile(HwRevision::RevB, "Rev-B  (0.1 degC per digit)");
  runProfile(HwRevision::RevA, "Rev-A  (1.0 degC per digit)");
  runFaults();
  runTwoAtOnce();

  std::printf("\n");
  return 0;
}

#endif  // HOST_BUILD
