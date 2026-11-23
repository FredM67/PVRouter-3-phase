/**
 * @file main.cpp
 * @brief CT recorder: streams the router's own ADC chain (one voltage, one current) over serial.
 *
 * The whole chain runs as the router runs it, without calibration: free-running ADC, prescaler 128,
 * 13 clocks per conversion, ADMUX written last in the ISR for the conversion after next, same
 * reference, left-aligned result. The ISR runs the firmware's voltage path (sample_path.h), the
 * current minus DC, and the CT filter when it is on, and pushes one 8-byte frame per sample set.
 * loop() handles the commands and streams the frames.
 *
 * Commands (one per line, 500 000 baud), each answered with #OK or #ERR <reason>:
 *   F <alpha> <gain>          the CT filter (gain 0: off; off at power-up)
 *   B <on>,<off>[,<on>,<off>] the load's burst pattern, in mains cycles
 *   R <seconds>               record: #-header lines, #BEGIN, binary frames, #END frames=<n> overruns=<n>
 *
 * Flavour: -D CTR_ROUTER, CTR_BOARD, CTR_PAIR (platformio.ini).
 */

#include <Arduino.h>
#include <avr/io.h>
#include <stdlib.h>
#include <string.h>

#include "burst_pattern.h"
#include "chain_layout.h"
#include "frame.h"
#include "ring_buffer.h"
#include "sample_path.h"

#include "ct_filter.h"  // the firmware's, from -I ../../Mk2_3phase_RFdatalog_temp
#include "load_map.h"   // LoadStates

#ifndef CTR_ROUTER
#error "define CTR_ROUTER, CTR_BOARD and CTR_PAIR (see platformio.ini)"
#endif

#define CTR_STR_(x) #x
#define CTR_STR(x) CTR_STR_(x)

namespace
{
constexpr char VERSION[]{ "1" };
constexpr uint32_t BAUD{ 500000UL };

constexpr ChainLayout LAYOUT{ layoutOf(Router::CTR_ROUTER, Board::CTR_BOARD, Pair::CTR_PAIR) };

// the ADMUX bit numbers of chain_layout.h are the ATmega328P's
static_assert(ADMUX_REFS0 == REFS0 && ADMUX_REFS1 == REFS1 && ADMUX_ADLAR == ADLAR, "ADMUX bit numbers");

constexpr uint8_t RING_SIZE{ 128 };
static_assert((RING_SIZE & (RING_SIZE - 1)) == 0, "the ring buffer size is a power of two");
static_assert(sizeof(FrameData) == 7, "a frame in the ring takes 7 bytes");

// load pins: the first load of both shipped configs is driven by the pattern, the others are held OFF
constexpr uint8_t LOAD_PIN{ 5 };
constexpr uint8_t OTHER_LOAD_PINS[]{ 6, 7 };
constexpr uint8_t LEVEL_ON{ static_cast< uint8_t >(LoadStates::LOAD_ON) };
constexpr uint8_t LEVEL_OFF{ static_cast< uint8_t >(LoadStates::LOAD_OFF) };

/**
 * The DC-offset integrator of the firmware settles with a time constant of about 20 s at the 3-phase
 * spacing (about 10 s single-phase). A recording is refused until 6 x 20 s = 120 s after power-up,
 * about 5 time constants (under 1 % of the initial error left), so that the offset has settled.
 */
constexpr uint32_t WARMUP_S{ 120 };

constexpr uint32_t MAX_RECORD_S{ 3600 };

// ADMUX is written at least this many CPU cycles after the ISR was entered: more than one ADC clock (128), as the
// firmware, which does real work on every channel
constexpr uint16_t ADMUX_MIN_DELAY_CYCLES{ 160 };

// ADMUX to write after each conversion: the one for the conversion after next
struct AdmuxTable
{
  uint8_t value[6];
};
constexpr AdmuxTable makeAdmuxTable()
{
  AdmuxTable t{};
  for (uint8_t k = 0; k != LAYOUT.conversions; ++k)
  {
    t.value[k] = admuxOf(LAYOUT, indexAfterNext(LAYOUT, k));
  }
  return t;
}
constexpr AdmuxTable ADMUX_TABLE{ makeAdmuxTable() };
uint8_t admuxWrittenAfter[6];  // RAM copy: no PROGMEM read in the ISR

// shared between the ISR and loop()
SamplePath samplePath;
RingBuffer< RING_SIZE > ring;
BurstPattern pattern;
BurstPattern::Cursor cursor;

CtFilter::LowPassBoost lowPass;
CtFilter::Coefficients coefficients{};
float filterAlpha{ 0.0F };
float filterGain{ 0.0F };
volatile bool filterOn{ false };

volatile bool recording{ false };
volatile bool loadOn{ false };
volatile uint32_t setsConverted{ 0 };  // voltage conversions so far: one per sample set

// ISR state
uint8_t conversionIndex{ 0 };
uint16_t lastV{ 0 };
bool lastCycleStart{ false };

inline void driveLoad(const bool on)
{
  loadOn = on;
  digitalWrite(LOAD_PIN, on ? LEVEL_ON : LEVEL_OFF);
}
}  // namespace

ISR(ADC_vect)
{
  const uint16_t entry{ TCNT1 };  // Timer1 counts CPU cycles: the time base of the ADMUX margin below
  const uint16_t raw{ ADCW };     // left-aligned
  const uint8_t index{ conversionIndex };

  if (index == LAYOUT.vIndex)
  {
    samplePath.onVoltage(raw);
    lastV = raw;
    lastCycleStart = samplePath.cycleStarted();
    ++setsConverted;
    if (lastCycleStart)
    {
      // one burst step per mains cycle, and only at a cycle start; OFF whenever not recording
      driveLoad(recording ? cursor.next(pattern) : false);
    }
  }
  else if (index == LAYOUT.iIndex)
  {
    // as processCurrentRawSample(): the current minus the voltage's offset, then the CT filter
    int16_t iOut{ samplePath.currentMinusDc(raw) };
    const bool filtered{ filterOn };
    if (filtered)
    {
      iOut = lowPass.apply(iOut, coefficients);
    }
    if (recording)
    {
      FrameData f{};
      f.v = static_cast< uint16_t >(lastV >> 6);
      f.i = static_cast< uint16_t >(raw >> 6);
      f.iOut = iOut;
      f.loadOn = loadOn;
      f.filterOn = filtered;
      f.cycleStart = lastCycleStart;
      ring.push(f);
    }
  }

  // ADMUX applies to the conversion after next, and the datasheet only allows writing it one ADC clock (128 CPU
  // cycles) or more after the conversion started: earlier, the running conversion may take the new channel. The
  // paths that do no work (the channels not recorded) are shorter than that, so wait until ADMUX_MIN_DELAY_CYCLES
  // cycles after the entry of the ISR (itself 10 or more cycles after the conversion started: interrupt latency,
  // vector, register saves). The firmware has real work on every channel; the recorder has not.
  while (static_cast< uint16_t >(TCNT1 - entry) < ADMUX_MIN_DELAY_CYCLES)
  {
  }
  ADMUX = admuxWrittenAfter[index];
  conversionIndex = static_cast< uint8_t >((index + 1 == LAYOUT.conversions) ? 0 : index + 1);
}

namespace
{
void startAdc()
{
  for (uint8_t k = 0; k != LAYOUT.conversions; ++k)
  {
    admuxWrittenAfter[k] = ADMUX_TABLE.value[k];
  }

  ADCSRA &= static_cast< uint8_t >(~(1U << ADEN));  // stop the ADC
  ADCSRB = 0x00;                                    // free-running
  ADMUX = admuxOf(LAYOUT, 0);                       // first conversion: first channel

  ADCSRA |= (1U << ADPS0) | (1U << ADPS1) | (1U << ADPS2);  // system clock / 128
  ADCSRA |= (1U << ADATE);                                  // auto trigger: free-running
  ADCSRA |= (1U << ADIE);
  ADCSRA |= (1U << ADEN);
  ADCSRA |= (1U << ADSC);
  sei();
}

void printHeaderLine(const __FlashStringHelper *key, const char *value)
{
  Serial.print('#');
  Serial.print(key);
  Serial.print(' ');
  Serial.println(value);
}

void printHeaderLine(const __FlashStringHelper *key, uint32_t value)
{
  Serial.print('#');
  Serial.print(key);
  Serial.print(' ');
  Serial.println(value);
}

void printPattern()
{
  Serial.print(F("#pattern "));
  for (uint8_t k = 0; k != pattern.count(); ++k)
  {
    if (k != 0)
    {
      Serial.print(',');
    }
    Serial.print(pattern.step(k).onCycles);
    Serial.print(',');
    Serial.print(pattern.step(k).offCycles);
  }
  Serial.println();
}

uint32_t setsNow()
{
  noInterrupts();
  const uint32_t n{ setsConverted };
  interrupts();
  return n;
}

/** The sample sets last 1 s: the mean time between two sets, in microseconds (rounded). */
uint32_t measureSpacingUs()
{
  const uint32_t sets0{ setsNow() };
  const uint32_t t0{ micros() };
  while (static_cast< uint32_t >(micros() - t0) < 1000000UL)
  {
  }
  const uint32_t sets1{ setsNow() };
  const uint32_t elapsed{ static_cast< uint32_t >(micros() - t0) };
  const uint32_t sets{ sets1 - sets0 };
  return (sets == 0) ? 0 : (elapsed + sets / 2) / sets;
}

void setFilter(const float alpha, const float gain)
{
  noInterrupts();
  filterAlpha = alpha;
  filterGain = gain;
  coefficients = CtFilter::coefficientsOf(gain, alpha);
  lowPass = CtFilter::LowPassBoost{};  // a new filter starts from rest
  filterOn = (gain != 0.0F);
  interrupts();
}

bool parseFloat(const char *text, float &value, const char *&end)
{
  char *e{ nullptr };
  value = static_cast< float >(strtod(text, &e));
  end = e;
  return e != text;
}

void commandFilter(const char *args)
{
  float alpha;
  float gain;
  const char *end;
  if (!parseFloat(args, alpha, end) || !parseFloat(end, gain, end))
  {
    Serial.println(F("#ERR usage: F <alpha> <gain>"));
    return;
  }
  while (*end == ' ')
  {
    ++end;
  }
  if (*end != '\0')
  {
    Serial.println(F("#ERR usage: F <alpha> <gain>"));
    return;
  }
  // the range validation.h accepts; a gain of 0 is "off" whatever alpha is
  if (!(gain >= 0.0F) || !(gain < 64.0F))
  {
    Serial.println(F("#ERR gain must be from 0 to below 64"));
    return;
  }
  if (gain != 0.0F && !CtFilter::isAccurate(gain, alpha))
  {
    Serial.println(F("#ERR alpha must be from 0.00005 to below 0.0625"));
    return;
  }
  setFilter(alpha, gain);
  Serial.println(F("#OK"));
}

void commandPattern(const char *args)
{
  BurstPattern::Step steps[BurstPattern::MAX_STEPS];
  uint8_t numbers{ 0 };
  const char *p{ args };
  while (true)
  {
    char *end{ nullptr };
    const long n{ strtol(p, &end, 10) };
    if (end == p || n < 0 || n > 65535L || numbers == 2 * BurstPattern::MAX_STEPS)
    {
      Serial.println(F("#ERR usage: B <on>,<off>[,<on>,<off>...] (cycles, up to 8 pairs)"));
      return;
    }
    if ((numbers & 1) == 0)
    {
      steps[numbers / 2].onCycles = static_cast< uint16_t >(n);
    }
    else
    {
      steps[numbers / 2].offCycles = static_cast< uint16_t >(n);
    }
    ++numbers;
    p = end;
    if (*p == ',')
    {
      ++p;
      continue;
    }
    break;
  }
  while (*p == ' ')
  {
    ++p;
  }
  if (*p != '\0' || (numbers & 1) != 0)
  {
    Serial.println(F("#ERR usage: B <on>,<off>[,<on>,<off>...] (cycles, up to 8 pairs)"));
    return;
  }
  noInterrupts();
  const bool ok{ pattern.set(steps, static_cast< uint8_t >(numbers / 2)) };
  interrupts();
  if (ok)
  {
    Serial.println(F("#OK"));
  }
  else
  {
    Serial.println(F("#ERR a pair of 0 on and 0 off cycles"));
  }
}

void sendFrame(const FrameData &f)
{
  uint8_t bytes[FRAME_SIZE];
  encodeFrame(bytes, f);
  Serial.write(bytes, FRAME_SIZE);
}

void commandRecord(const char *args)
{
  char *end{ nullptr };
  const long seconds{ strtol(args, &end, 10) };
  if (end == args)
  {
    Serial.println(F("#ERR usage: R <seconds> (1 to 3600)"));
    return;
  }
  while (*end == ' ')
  {
    ++end;
  }
  if (*end != '\0' || seconds < 1 || seconds > static_cast< long >(MAX_RECORD_S))
  {
    Serial.println(F("#ERR usage: R <seconds> (1 to 3600)"));
    return;
  }
  if (millis() / 1000UL < WARMUP_S)
  {
    Serial.println(F("#ERR warming up"));
    return;
  }

  Serial.println(F("#OK"));
  printHeaderLine(F("version"), VERSION);
  printHeaderLine(F("router"), CTR_STR(CTR_ROUTER));
  printHeaderLine(F("board"), CTR_STR(CTR_BOARD));
  printHeaderLine(F("pair"), CTR_STR(CTR_PAIR));
  printHeaderLine(F("conversions"), LAYOUT.conversions);
  printHeaderLine(F("spacing_us"), sampleSpacingUs(LAYOUT));
  printHeaderLine(F("measured_spacing_us"), measureSpacingUs());
  Serial.print(F("#alpha "));
  Serial.println(filterAlpha, 6);
  Serial.print(F("#gain "));
  Serial.println(filterGain, 4);
  printPattern();
  printHeaderLine(F("load_pin"), LOAD_PIN);
  noInterrupts();
  const uint16_t dcOffset{ samplePath.dcOffset() };
  interrupts();
  printHeaderLine(F("dc_offset"), dcOffset);
  printHeaderLine(F("uptime_s"), millis() / 1000UL);
  Serial.println(F("#BEGIN"));

  noInterrupts();
  ring.clear();
  cursor.reset(pattern);  // the first cycle start after this is cycle 0 of the pattern
  recording = true;
  interrupts();

  uint32_t frames{ 0 };
  FrameData f;
  const uint32_t t0{ millis() };
  const uint32_t durationMs{ static_cast< uint32_t >(seconds) * 1000UL };
  while (static_cast< uint32_t >(millis() - t0) < durationMs)
  {
    if (ring.pop(f))
    {
      sendFrame(f);
      ++frames;
    }
  }

  noInterrupts();
  recording = false;
  interrupts();
  while (ring.pop(f))  // the frames still in the ring
  {
    sendFrame(f);
    ++frames;
  }
  noInterrupts();
  driveLoad(false);  // the load is left OFF
  const uint32_t overruns{ ring.overruns() };
  interrupts();

  Serial.print(F("#END frames="));
  Serial.print(frames);
  Serial.print(F(" overruns="));
  Serial.println(overruns);
}

void handleLine(char *line)
{
  while (*line == ' ')
  {
    ++line;
  }
  const char command{ *line };
  const char *args{ (command == '\0') ? line : line + 1 };
  switch (command)
  {
    case '\0':
      break;
    case 'F':
      commandFilter(args);
      break;
    case 'B':
      commandPattern(args);
      break;
    case 'R':
      commandRecord(args);
      break;
    default:
      Serial.println(F("#ERR unknown command"));
      break;
  }
}
}  // namespace

void setup()
{
  Serial.begin(BAUD);

  pinMode(LOAD_PIN, OUTPUT);
  digitalWrite(LOAD_PIN, LEVEL_OFF);
  for (const uint8_t pin : OTHER_LOAD_PINS)
  {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LEVEL_OFF);
  }

  TCCR1A = 0;
  TCCR1B = (1U << CS10);  // Timer1 free-running at the CPU clock, read by the ISR (ADMUX margin)
  startAdc();
}

void loop()
{
  static char line[40];
  static uint8_t length{ 0 };

  while (Serial.available() > 0)
  {
    const char c{ static_cast< char >(Serial.read()) };
    if (c == '\n' || c == '\r')
    {
      line[length] = '\0';
      length = 0;
      handleLine(line);
    }
    else if (length + 1U < sizeof(line))
    {
      line[length++] = c;
    }
    else
    {
      length = 0;  // too long: dropped
      Serial.println(F("#ERR line too long"));
    }
  }
}
