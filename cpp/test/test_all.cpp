#include "Classifier.hpp"
#include "DeviceInfo.hpp"
#include "HostHal.hpp"
#include "Indicator.hpp"
#include "MedianFilter.hpp"
#include "SampleQueue.hpp"
#include "TemperatureMonitor.hpp"
#include "TemperatureSensor.hpp"
#include "test.hpp"

using namespace tempmon;
using namespace tempmon::host;

int g_checks   = 0;
int g_failures = 0;

struct Fixture {
  HostAdc adc;
  HostGpio gpio;
  HostEeprom eeprom;
  HostClock clock;
  TemperatureMonitor monitor {adc, gpio, eeprom, clock};

  // one block then one pass of the main loop
  void step(MilliCelsius temp, std::uint32_t dtMs = 7) {
    adc.setRaw(rawForTemperature(*monitor.sensor(), temp));
    adc.produceBlock();
    clock.advance(dtMs);
    monitor.poll();
  }
};

static void test_device_info() {
  SECTION("DeviceInfo: identity record");

  {
    HostEeprom ee;
    DeviceInfo info;
    DeviceInfoReader reader {ee};

    ee.programValid(HwRevision::RevA, "ABC1234");
    CHECK(reader.load(info) == DeviceInfoStatus::Ok);
    CHECK(info.revision == HwRevision::RevA);
    CHECK_STR(info.serial.data(), "ABC1234");

    ee.programValid(HwRevision::RevB, "XYZ9876");
    CHECK(reader.load(info) == DeviceInfoStatus::Ok);
    CHECK(info.revision == HwRevision::RevB);
    CHECK_STR(info.serial.data(), "XYZ9876");

    // a failed read must not leave a record behind
    ee.setBusFail(true);
    CHECK(reader.load(info) == DeviceInfoStatus::BusError);
    ee.setBusFail(false);

    // blank device the magic check catches it
    std::array<std::uint8_t, kEepromRecLen> blank {};
    blank.fill(0xFF);
    ee.programRaw(blank.data(), blank.size());
    CHECK(reader.load(info) == DeviceInfoStatus::BadMagic);

    ee.programValid(HwRevision::RevB, "ABC1234");
    ee.corruptCrc();
    CHECK(reader.load(info) == DeviceInfoStatus::BadCrc);

    // good record but unknown revision must not fall back to Rev-A
    std::array<std::uint8_t, kEepromRecLen> rec {0x5Au, 0xC5u, 0x07u, 'Q', 'Q', 'Q', '0', '0', '0', '1', 0x00u};
    rec[kEepromRecLen - 1] = DeviceInfoReader::crc8(rec.data(), kEepromRecLen - 1);
    ee.programRaw(rec.data(), rec.size());
    CHECK(reader.load(info) == DeviceInfoStatus::UnsupportedRevision);
  }
}

static void test_sensors() {
  SECTION("TemperatureSensor: both revisions scale to one unit");

  const ITemperatureSensor* senA = sensorFor(HwRevision::RevA);
  const ITemperatureSensor* senB = sensorFor(HwRevision::RevB);
  CHECK(senA != nullptr);
  CHECK(senB != nullptr);

  // the two examples from the requirement
  CHECK(senA->toTemperature(10) == degC(10));
  CHECK(senB->toTemperature(100) == degC(10));

  CHECK(senA->toTemperature(85) == degC(85));
  CHECK(senA->toTemperature(105) == degC(105));
  CHECK(senB->toTemperature(850) == degC(85));
  CHECK(senB->toTemperature(1050) == degC(105));

  CHECK(senA->resolution() == degC(1));
  CHECK(senB->resolution() == MilliCelsius {100});

  // top of the converter must not overflow
  CHECK(senB->toTemperature(config::kAdcRawMax) == MilliCelsius {409500});

  // an unknown revision has no sensor
  CHECK(sensorFor(static_cast<HwRevision>(7)) == nullptr);
}

static void test_filter() {
  SECTION("MedianFilter: rejects what a mean would not");

  MedianFilter<9> small;
  {
    const std::array<AdcRaw, 9> vals {7, 2, 9, 4, 1, 8, 3, 6, 5};
    CHECK_EQ(small.apply(vals.data()), 5);
  }

  // even count takes the upper of the two middle values
  MedianFilter<4> even;
  {
    const std::array<AdcRaw, 4> vals {40, 10, 30, 20};
    CHECK_EQ(even.apply(vals.data()), 30);
  }

  MedianFilter<config::kSamplesPerBlock> full;

  // three spikes a mean would trip but the median does not
  {
    std::array<AdcRaw, config::kSamplesPerBlock> vals;
    vals.fill(850);
    vals[3] = vals[17] = vals[40] = config::kAdcRawMax;
    CHECK_EQ(full.apply(vals.data()), 850);
  }

  // corrupt more than half and the median follows
  {
    std::array<AdcRaw, config::kSamplesPerBlock> vals;
    for (std::size_t i = 0; i < vals.size(); ++i) vals[i] = (i < 33) ? 4000 : 850;
    CHECK_EQ(full.apply(vals.data()), 4000);
  }

  SECTION("MedianFilter: converter end stops");
  using F = MedianFilter<config::kSamplesPerBlock>;
  CHECK(F::atRail(0, config::kAdcRawMax));
  CHECK(F::atRail(config::kAdcRawMax, config::kAdcRawMax));
  CHECK(!F::atRail(1, config::kAdcRawMax));
  CHECK(!F::atRail(2000, config::kAdcRawMax));
}

// start in prev feed one temperature report where it lands
static Condition from(Condition prev, MilliCelsius temp, Classifier::Config cfg = Classifier::kDefault) {
  Classifier clsf {cfg, prev};
  return clsf.update(temp);
}

// the requirement with hysteresis removed
static const Classifier::Config kLiteral {
    config::kWarning, config::kCriticalHot, config::kCriticalCold, MilliCelsius {0}};

static void test_classifier_thresholds() {
  SECTION("Classifier: the thresholds exactly as specified");

  // 85 degC is the warning edge one milli below is still normal
  CHECK(from(Condition::Normal, MilliCelsius {84999}) == Condition::Normal);
  CHECK(from(Condition::Normal, MilliCelsius {85000}) == Condition::Warning);

  CHECK(from(Condition::Warning, MilliCelsius {104999}) == Condition::Warning);
  CHECK(from(Condition::Warning, MilliCelsius {105000}) == Condition::CriticalHot);

  // 5 degC is the cold edge the threshold itself is normal
  CHECK(from(Condition::Normal, MilliCelsius {5000}) == Condition::Normal);
  CHECK(from(Condition::Normal, MilliCelsius {4999}) == Condition::CriticalCold);

  SECTION("Classifier: overlapping rules resolve by severity");

  // both rules hold at 110 degC red wins
  CHECK(from(Condition::Normal, degC(110)) == Condition::CriticalHot);

  // both rules hold at 3 degC red wins
  CHECK(from(Condition::Normal, degC(3)) == Condition::CriticalCold);

  // severity beats history too
  CHECK(from(Condition::Warning, degC(200)) == Condition::CriticalHot);
  CHECK(from(Condition::CriticalHot, degC(1)) == Condition::CriticalCold);
}

static void test_classifier_hysteresis() {
  SECTION("Classifier: hysteresis holds on the way out only");

  // escalation is never delayed 85 warns at once
  CHECK(from(Condition::Normal, MilliCelsius {85000}) == Condition::Warning);

  // de escalation needs real recovery
  CHECK(from(Condition::Warning, MilliCelsius {84999}) == Condition::Warning);
  CHECK(from(Condition::Warning, MilliCelsius {84500}) == Condition::Warning);
  CHECK(from(Condition::Warning, MilliCelsius {84499}) == Condition::Normal);

  CHECK(from(Condition::CriticalHot, MilliCelsius {104500}) == Condition::CriticalHot);
  CHECK(from(Condition::CriticalHot, MilliCelsius {104499}) == Condition::Warning);

  CHECK(from(Condition::CriticalCold, MilliCelsius {5499}) == Condition::CriticalCold);
  CHECK(from(Condition::CriticalCold, MilliCelsius {5500}) == Condition::Normal);

  // coming out of a fault there is no history
  CHECK(from(Condition::Fault, MilliCelsius {84999}) == Condition::Normal);
  CHECK(from(Condition::Fault, degC(90)) == Condition::Warning);

  SECTION("Classifier: literal specification with hysteresis disabled");

  // band at zero is the literal requirement
  CHECK(from(Condition::Warning, MilliCelsius {84999}, kLiteral) == Condition::Normal);
  CHECK(from(Condition::CriticalHot, MilliCelsius {104999}, kLiteral) == Condition::Warning);
  CHECK(from(Condition::CriticalCold, MilliCelsius {5000}, kLiteral) == Condition::Normal);
}

// sit a noisy signal on the threshold and count lamp changes
static void test_classifier_chatter() {
  SECTION("Classifier: hysteresis stops the lamp chatter");

  Classifier withHyst {};
  Classifier without {kLiteral};

  std::uint32_t rng      = 0x2468ACE0u;
  std::uint32_t nHyst    = 0;
  std::uint32_t nLiteral = 0;

  for (int i = 0; i < 1000; ++i) {
    rng = (rng * 1103515245u) + 12345u;
    const MilliCelsius temp {85000 + static_cast<std::int32_t>((rng >> 16) % 601u) - 300};

    const Condition beforeH = withHyst.condition();
    const Condition beforeL = without.condition();

    if (withHyst.update(temp) != beforeH) ++nHyst;
    if (without.update(temp) != beforeL) ++nLiteral;
  }

  std::printf(
      "      lamp changes over 1000 samples on the threshold:"
      " %u with hysteresis and %u without\n",
      nHyst,
      nLiteral);

  CHECK_EQ(nHyst, 1);     // enters warning once and stays
  CHECK(nLiteral > 100);  // a strobe
}

// walk up through every band and back down
static void test_classifier_sweep() {
  SECTION("Classifier: full sweep up and down");

  Classifier clsf;

  struct Step {
    MilliCelsius t;
    Condition want;
  };

  const Step up[] {
      {degC(20), Condition::Normal},
      {MilliCelsius {84900}, Condition::Normal},
      {MilliCelsius {85000}, Condition::Warning},
      {degC(95), Condition::Warning},
      {MilliCelsius {104900}, Condition::Warning},
      {MilliCelsius {105000}, Condition::CriticalHot},
      {degC(150), Condition::CriticalHot},
  };
  for (const auto& step : up) CHECK(clsf.update(step.t) == step.want);

  const Step down[] {
      {MilliCelsius {104900}, Condition::CriticalHot},  // release band holds it
      {MilliCelsius {104500}, Condition::CriticalHot},
      {MilliCelsius {104400}, Condition::Warning},
      {MilliCelsius {84600}, Condition::Warning},
      {MilliCelsius {84400}, Condition::Normal},
      {degC(20), Condition::Normal},
      {degC(5), Condition::Normal},
      {MilliCelsius {4999}, Condition::CriticalCold},
      {MilliCelsius {5400}, Condition::CriticalCold},  // other side
      {MilliCelsius {5500}, Condition::Normal},
  };
  for (const auto& step : down) CHECK(clsf.update(step.t) == step.want);
}

static void test_indicator() {
  SECTION("Indicator: one lamp per condition");

  const auto green  = static_cast<std::size_t>(LedId::Green);
  const auto yellow = static_cast<std::size_t>(LedId::Yellow);
  const auto red    = static_cast<std::size_t>(LedId::Red);

  auto pattern = Indicator::patternFor(Condition::Normal, 0);
  CHECK(pattern[green] && !pattern[yellow] && !pattern[red]);

  pattern = Indicator::patternFor(Condition::Warning, 0);
  CHECK(!pattern[green] && pattern[yellow] && !pattern[red]);

  // both critical bands share the lamp
  pattern = Indicator::patternFor(Condition::CriticalHot, 0);
  CHECK(!pattern[green] && !pattern[yellow] && pattern[red]);

  pattern = Indicator::patternFor(Condition::CriticalCold, 0);
  CHECK(!pattern[green] && !pattern[yellow] && pattern[red]);

  SECTION("Indicator: fault blinks and never shows green");

  CHECK(!Indicator::patternFor(Condition::Fault, 0)[red]);
  CHECK(Indicator::patternFor(Condition::Fault, 250)[red]);
  CHECK(!Indicator::patternFor(Condition::Fault, 500)[red]);
  CHECK(Indicator::patternFor(Condition::Fault, 750)[red]);

  // a device that cannot measure never shows green
  for (std::uint32_t t = 0; t < 2000; t += 37) {
    const auto faultPattern = Indicator::patternFor(Condition::Fault, t);
    CHECK(!faultPattern[green] && !faultPattern[yellow]);
  }

  SECTION("Indicator: only changed lamps reach the pin");

  HostGpio gpio;
  Indicator ind {gpio};
  ind.invalidate();

  const auto normal = Indicator::patternFor(Condition::Normal, 0);
  ind.apply(normal);
  CHECK_EQ(gpio.writeCount(), kLedCount);  // invalidated all three

  ind.apply(normal);
  CHECK_EQ(gpio.writeCount(), kLedCount);  // unchanged none

  ind.apply(Indicator::patternFor(Condition::Warning, 0));
  CHECK_EQ(gpio.writeCount(), kLedCount + 2);  // green off yellow on
}

static void test_sample_queue() {
  SECTION("SampleQueue: fills and drains and drops instead of blocking");

  SampleQueue<4> queue;
  std::uint8_t val = 0;

  CHECK(!queue.pop(val));

  for (std::uint8_t i = 0; i < 4; ++i) CHECK(queue.push(i));

  // full so the producer drops and counts
  CHECK(!queue.push(99));
  CHECK_EQ(queue.overruns(), 1);
  CHECK(!queue.push(99));
  CHECK_EQ(queue.overruns(), 2);

  for (std::uint8_t i = 0; i < 4; ++i) {
    CHECK(queue.pop(val));
    CHECK_EQ(val, i);  // order preserved
  }
  CHECK(!queue.pop(val));

  // many trips the masked counters must not drift
  SampleQueue<4> queue2;
  for (std::uint32_t n = 0; n < 1000; ++n) {
    CHECK(queue2.push(static_cast<std::uint8_t>(n & 0xFFu)));
    CHECK(queue2.pop(val));
    CHECK_EQ(val, static_cast<std::uint8_t>(n & 0xFFu));
  }
  CHECK_EQ(queue2.overruns(), 0);
}

static void test_integration_revb() {
  SECTION("integration: Rev-B from sensor to lamp");

  Fixture fix;
  fix.eeprom.programValid(HwRevision::RevB, "ABC1234");
  CHECK(fix.monitor.init());
  CHECK(fix.adc.running());

  // before any block the device says fault not green
  CHECK(fix.monitor.condition() == Condition::Fault);
  CHECK(fix.monitor.fault() == FaultReason::NoSamples);

  fix.step(degC(25));
  CHECK(fix.monitor.condition() == Condition::Normal);
  CHECK(fix.monitor.temperature() == degC(25));
  CHECK(fix.gpio.led(LedId::Green) && !fix.gpio.led(LedId::Yellow) && !fix.gpio.led(LedId::Red));

  fix.step(degC(85));  // exactly the threshold
  CHECK(fix.monitor.condition() == Condition::Warning);
  CHECK(!fix.gpio.led(LedId::Green) && fix.gpio.led(LedId::Yellow) && !fix.gpio.led(LedId::Red));

  fix.step(degC(110));
  CHECK(fix.monitor.condition() == Condition::CriticalHot);
  CHECK(!fix.gpio.led(LedId::Green) && !fix.gpio.led(LedId::Yellow) && fix.gpio.led(LedId::Red));

  // inside the release band still critical
  fix.step(MilliCelsius {104600});
  CHECK(fix.monitor.condition() == Condition::CriticalHot);

  fix.step(MilliCelsius {104400});
  CHECK(fix.monitor.condition() == Condition::Warning);

  fix.step(degC(20));
  CHECK(fix.monitor.condition() == Condition::Normal);

  // cold excursion the other red band
  fix.step(degC(3));
  CHECK(fix.monitor.condition() == Condition::CriticalCold);
  CHECK(fix.gpio.led(LedId::Red));

  fix.step(degC(20));
  CHECK(fix.monitor.condition() == Condition::Normal);
}

// hysteresis floored at one count so Rev-A stays usable
static void test_hysteresis_floor() {
  SECTION("Monitor: hysteresis is floored at one sensor count");

  {
    Fixture fix;
    fix.eeprom.programValid(HwRevision::RevB, "ABC1234");
    CHECK(fix.monitor.init());
    CHECK(fix.monitor.classifier().hysteresis() == config::kHysteresis);  // 0.5 over 0.1 kept
  }

  Fixture fix;
  fix.eeprom.programValid(HwRevision::RevA, "ABC1234");
  CHECK(fix.monitor.init());
  CHECK(fix.monitor.classifier().hysteresis() == degC(1));  // 0.5 under 1.0 raised

  // Rev-A holds critical at 104 degC the first count below
  fix.step(degC(110));
  CHECK(fix.monitor.condition() == Condition::CriticalHot);

  fix.step(degC(104));
  CHECK(fix.monitor.condition() == Condition::CriticalHot);

  fix.step(degC(103));
  CHECK(fix.monitor.condition() == Condition::Warning);
}

static void test_integration_reva() {
  SECTION("integration: Rev-A reaches the same lamps from different digits");

  Fixture fix;
  fix.eeprom.programValid(HwRevision::RevA, "ABC1234");
  CHECK(fix.monitor.init());
  CHECK(fix.monitor.info().revision == HwRevision::RevA);

  // 90 degC is raw 90 on Rev-A and 900 on Rev-B same lamp
  fix.step(degC(90));
  CHECK(fix.monitor.temperature() == degC(90));
  CHECK(fix.monitor.condition() == Condition::Warning);

  fix.step(degC(30));
  CHECK(fix.monitor.condition() == Condition::Normal);
}

static void test_integration_noise_and_spikes() {
  SECTION("integration: noise and glitches do not move the lamp");

  Fixture fix;
  fix.eeprom.programValid(HwRevision::RevB, "ABC1234");
  CHECK(fix.monitor.init());

  // noisy 84.5 degC stays under 85 so the lamp must not move
  fix.adc.setNoise(4);
  fix.adc.setRaw(rawForTemperature(*fix.monitor.sensor(), MilliCelsius {84500}));

  for (int i = 0; i < 200; ++i) {
    fix.adc.produceBlock();
    fix.clock.advance(7);
    fix.monitor.poll();
  }
  CHECK(fix.monitor.condition() == Condition::Normal);

  // a burst of glitches the median throws away
  fix.adc.setNoise(0);
  fix.adc.setRaw(rawForTemperature(*fix.monitor.sensor(), degC(50)));
  fix.adc.injectSpike(config::kAdcRawMax, 20);
  fix.adc.produceBlock();
  fix.clock.advance(7);
  fix.monitor.poll();

  CHECK(fix.monitor.condition() == Condition::Normal);
  CHECK(fix.monitor.temperature() == degC(50));
}

static void test_integration_faults() {
  SECTION("integration: a device that cannot measure never shows green");

  {
    // eeprom unreadable so acquisition never starts
    Fixture fix;
    fix.eeprom.setBusFail(true);
    CHECK(!fix.monitor.init());
    CHECK(fix.monitor.condition() == Condition::Fault);
    CHECK(fix.monitor.fault() == FaultReason::Config);
    CHECK(!fix.adc.running());

    // latched repairing the bus at runtime does not un boot the device
    fix.eeprom.setBusFail(false);
    fix.eeprom.programValid(HwRevision::RevB, "ABC1234");
    fix.clock.advance(1000);
    fix.monitor.poll();
    CHECK(fix.monitor.fault() == FaultReason::Config);
  }

  SECTION("integration: converter at an end stop is a wiring fault");

  Fixture fix;
  fix.eeprom.programValid(HwRevision::RevB, "ABC1234");
  CHECK(fix.monitor.init());

  fix.step(degC(25));
  CHECK(fix.monitor.condition() == Condition::Normal);

  fix.adc.setRaw(config::kAdcRawMax);  // input open or shorted pin
  fix.adc.produceBlock();
  fix.clock.advance(7);
  fix.monitor.poll();
  CHECK(fix.monitor.condition() == Condition::Fault);
  CHECK(fix.monitor.fault() == FaultReason::SensorRail);

  // recovers on its own once the signal returns
  fix.step(degC(25));
  CHECK(fix.monitor.condition() == Condition::Normal);
  CHECK(fix.monitor.fault() == FaultReason::None);

  SECTION("integration: silence from the acquisition chain is a fault");

  // inside the timeout the device holds its last reading
  fix.clock.advance(50);
  fix.monitor.poll();
  CHECK(fix.monitor.condition() == Condition::Normal);

  // past it the silence itself is the diagnosis
  fix.clock.advance(100);
  fix.monitor.poll();
  CHECK(fix.monitor.condition() == Condition::Fault);
  CHECK(fix.monitor.fault() == FaultReason::NoSamples);
}

// samples are hardware triggered so a late loop just gets a backlog
static void test_integration_backlog() {
  SECTION("integration: a late main loop takes the newest block");

  Fixture fix;
  fix.eeprom.programValid(HwRevision::RevB, "ABC1234");
  CHECK(fix.monitor.init());

  const ITemperatureSensor& sensor = *fix.monitor.sensor();

  fix.adc.setRaw(rawForTemperature(sensor, degC(20)));
  fix.adc.produceBlock();
  fix.adc.setRaw(rawForTemperature(sensor, degC(40)));
  fix.adc.produceBlock();
  fix.adc.setRaw(rawForTemperature(sensor, degC(95)));
  fix.adc.produceBlock();

  fix.clock.advance(20);
  fix.monitor.poll();

  CHECK(fix.monitor.temperature() == degC(95));  // newest not first
  CHECK(fix.monitor.condition() == Condition::Warning);
  CHECK_EQ(fix.monitor.blocksSkipped(), 2);
}

// two monitors in one process
static void test_two_devices_at_once() {
  SECTION("integration: two devices with different revisions in one process");

  Fixture fixA;
  Fixture fixB;

  fixA.eeprom.programValid(HwRevision::RevA, "AAA0001");
  fixB.eeprom.programValid(HwRevision::RevB, "BBB0002");

  CHECK(fixA.monitor.init());
  CHECK(fixB.monitor.init());

  fixA.step(degC(90));
  fixB.step(degC(20));

  CHECK(fixA.monitor.condition() == Condition::Warning);
  CHECK(fixB.monitor.condition() == Condition::Normal);

  CHECK(fixA.gpio.led(LedId::Yellow));
  CHECK(fixB.gpio.led(LedId::Green));

  CHECK_STR(fixA.monitor.info().serial.data(), "AAA0001");
  CHECK_STR(fixB.monitor.info().serial.data(), "BBB0002");
}

int main() {
  std::printf("temperature monitor host tests (C++)\n");
  test_device_info();
  test_sensors();
  test_filter();
  test_classifier_thresholds();
  test_classifier_hysteresis();
  test_classifier_chatter();
  test_classifier_sweep();
  test_indicator();
  test_sample_queue();
  test_integration_revb();
  test_hysteresis_floor();
  test_integration_reva();
  test_integration_noise_and_spikes();
  test_integration_faults();
  test_integration_backlog();
  test_two_devices_at_once();

  std::printf("\n%d checks and %d failures\n", g_checks, g_failures);
  return (g_failures == 0) ? 0 : 1;
}
