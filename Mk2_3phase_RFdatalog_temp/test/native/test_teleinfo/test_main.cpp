// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file test_main.cpp
 * @brief Native tests for teleinfo.h: the IoT frame, streamed a line per step
 *
 * @details The frame is written by TeleInfo::writeFrameStep(), one line per step of the serial
 *          output (STX before the first line, ETX after the last), from a data source: here, test
 *          sources with feature sets of the firmware. It must give, byte for byte, what the
 *          buffered TeleInfo class of the firmware wrote before (Reference below, its code kept
 *          as it was, utoa() replaced by snprintf()), with N and V sent unsigned.
 */

#include <unity.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "teleinfo.h"  // Real header - pure C++

void setUp(void) {}
void tearDown(void) {}

/** @brief The values of a datalog, shared by every test source. */
struct Values
{
  int16_t power;
  int16_t phasePower[3];
  uint16_t phaseVoltage_x100[3];
  int16_t relayAverage;
  bool relayOn[4];
  int16_t loadOnPercent[4];
  int16_t temperature_x100[4];
  bool temperatureValid[4];
  uint16_t absenceCount;
  bool offPeak;
  uint16_t sampleSets;
  uint8_t lowestSampleSetsPerCycle;
};

static Values values;

/** @brief A data source of writeFrameStep(), with a feature set of the firmware. */
template< uint8_t PHASES, uint8_t RELAYS, uint8_t LOADS, uint8_t TEMPS, bool DUAL >
struct TestSource
{
  static constexpr uint8_t PHASE_COUNT{ PHASES };
  static constexpr bool RELAY_DIVERSION{ RELAYS != 0 };
  static constexpr uint8_t RELAY_COUNT{ RELAYS };
  static constexpr uint8_t LOAD_COUNT{ LOADS };
  static constexpr uint8_t TEMP_COUNT{ TEMPS };
  static constexpr bool DUAL_TARIFF{ DUAL };

  static int16_t power()
  {
    return values.power;
  }
  static int16_t phasePower(uint8_t idx)
  {
    return values.phasePower[idx];
  }
  static uint16_t phaseVoltage(uint8_t idx)
  {
    return values.phaseVoltage_x100[idx];
  }
  static int16_t relayAverage()
  {
    return values.relayAverage;
  }
  static bool relayOn(uint8_t idx)
  {
    return values.relayOn[idx];
  }
  static int16_t loadOnPercent(uint8_t idx)
  {
    return values.loadOnPercent[idx];
  }
  static bool temperatureValid(uint8_t idx)
  {
    return values.temperatureValid[idx];
  }
  static int16_t temperature(uint8_t idx)
  {
    return values.temperature_x100[idx];
  }
  static uint16_t absenceCount()
  {
    return values.absenceCount;
  }
  static bool offPeak()
  {
    return values.offPeak;
  }
  static uint16_t sampleSets()
  {
    return values.sampleSets;
  }
  static uint8_t lowestSampleSetsPerCycle()
  {
    return values.lowestSampleSetsPerCycle;
  }
};

/**
 * @brief The buffered TeleInfo class of the firmware, as it was (utoa() aside): the reference of
 *        the frame. Its frame differs in two places only (referenceFrame()): N and each V are
 *        sent unsigned.
 */
class Reference
{
private:
  static const char STX{ 0x02 };
  static const char ETX{ 0x03 };
  static const char LF{ 0x0A };
  static const char CR{ 0x0D };
  static const char TAB{ 0x09 };

  char buffer[512]{};
  size_t bufferPos{ 0 };

  [[nodiscard]] uint8_t calculateChecksum(size_t startPos, size_t endPos) const
  {
    uint8_t sum{ 0 };
    for (const char* ptr = buffer + startPos; ptr != buffer + endPos; ++ptr)
    {
      sum += *ptr;
    }

    return (sum & 0x3F) + 0x20;
  }

  void writeTag(const char* tag, uint8_t index)
  {
    auto* ptr{ tag };
    while (*ptr) buffer[bufferPos++] = *ptr++;

    if (index != 0)
    {
      buffer[bufferPos++] = static_cast< char >('0' + index);
    }

    buffer[bufferPos++] = TAB;
  }

  void writeLine(const char* tag, uint8_t index, uint16_t magnitude, bool negative)
  {
    buffer[bufferPos++] = LF;

    const auto startPos{ bufferPos };

    writeTag(tag, index);
    if (negative)
    {
      buffer[bufferPos++] = '-';
    }
    char* str{ buffer + bufferPos };
    snprintf(str, 12, "%u", static_cast< unsigned >(magnitude));  // utoa()
    bufferPos += strlen(str);
    buffer[bufferPos++] = TAB;

    const auto crc{ calculateChecksum(startPos, bufferPos) };
    buffer[bufferPos++] = crc;

    buffer[bufferPos++] = CR;
  }

public:
  void startFrame()
  {
    bufferPos = 0;
    buffer[bufferPos++] = STX;
  }

  void send(const char* tag, int16_t value, uint8_t index = 0)
  {
    const auto magnitude{ value < 0 ? static_cast< uint16_t >(0U - static_cast< uint16_t >(value)) : static_cast< uint16_t >(value) };
    writeLine(tag, index, magnitude, value < 0);
  }

  void sendUnsigned(const char* tag, uint16_t value, uint8_t index = 0)
  {
    writeLine(tag, index, value, false);
  }

  void endFrame()
  {
    buffer[bufferPos++] = ETX;
  }

  [[nodiscard]] const char* data() const
  {
    return buffer;
  }
  [[nodiscard]] size_t size() const
  {
    return bufferPos;
  }
};

/** @brief The frame of sendTelemetryData() as it was, N and V sent unsigned. */
template< typename Source >
static void referenceFrame(Reference& teleInfo)
{
  uint8_t idx{ 0 };

  teleInfo.startFrame();

  teleInfo.send("P", Source::power());

  if constexpr (Source::PHASE_COUNT > 1)
  {
    idx = Source::PHASE_COUNT;
    do
    {
      --idx;
      teleInfo.send("P", Source::phasePower(idx), idx + 1);
      teleInfo.sendUnsigned("V", Source::phaseVoltage(idx), idx + 1);  // unsigned, was send() (negative above 327.67 V)
    } while (idx);
  }

  if constexpr (Source::RELAY_DIVERSION)
  {
    teleInfo.send("R", Source::relayAverage());

    idx = Source::RELAY_COUNT;
    do
    {
      --idx;
      teleInfo.send("R", Source::relayOn(idx), idx + 1);
    } while (idx);
  }

  idx = Source::LOAD_COUNT;
  do
  {
    --idx;
    teleInfo.send("D", Source::loadOnPercent(idx), idx + 1);
  } while (idx);

  for (uint8_t idx = 0; idx < Source::TEMP_COUNT; ++idx)
  {
    if (!Source::temperatureValid(idx))
    {
      continue;
    }
    teleInfo.send("T", Source::temperature(idx), idx + 1);
  }

  teleInfo.sendUnsigned("N", Source::absenceCount());  // unsigned, was send() (negative beyond 32767 s)

  if constexpr (Source::DUAL_TARIFF)
  {
    teleInfo.send("TA", static_cast< int16_t >(Source::offPeak() ? 1 : 0));
  }

  teleInfo.sendUnsigned("S", Source::sampleSets());
  teleInfo.send("S_MC", Source::lowestSampleSetsPerCycle());

  teleInfo.endFrame();
}

/** @brief A frame as the serial output writes it: every step, one after the other. */
struct Streamed
{
  char text[512]{};
  size_t length{ 0 };
  uint8_t steps{ 0 };
  uint8_t longestStep{ 0 };
};

template< typename Source >
static Streamed streamFrame()
{
  Streamed frame;
  bool more{ true };
  while (more)
  {
    char step[TeleInfo::MAX_STEP_LENGTH];
    uint8_t length{ 0 };
    more = TeleInfo::writeFrameStep< Source >(frame.steps++, step, length);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(TeleInfo::MAX_STEP_LENGTH, length);
    memcpy(frame.text + frame.length, step, length);
    frame.length += length;
    if (length > frame.longestStep)
    {
      frame.longestStep = length;
    }
    TEST_ASSERT_LESS_THAN_UINT8(100, frame.steps);  // never endless
  }
  return frame;
}

template< typename Source >
static void assertSameAsReference(const char* message)
{
  Reference reference;
  referenceFrame< Source >(reference);
  const Streamed streamed{ streamFrame< Source >() };

  TEST_ASSERT_EQUAL_UINT32_MESSAGE(reference.size(), streamed.length, message);
  TEST_ASSERT_EQUAL_MEMORY_MESSAGE(reference.data(), streamed.text, reference.size(), message);
}

/** @brief A datalog with every value set, temperature 2 invalid. */
static void typicalValues()
{
  values = Values{ -1234, { 399, 399, -2032 }, { 23045, 23110, 22987 }, -567, { true, false, true, false }, { 100, 37, 0, 5 }, { 2137, -12700, 6500, -5 }, { true, false, true, true }, 731, true, 250, 44 };
}

/** @brief The extremes of every value. */
static void extremeValues()
{
  values = Values{ -32768, { 32767, -32768, 0 }, { 65535, 0, 32768 }, 32767, { false, true, false, true }, { 0, 100, 1, 99 }, { -32768, 32767, 0, 12500 }, { true, true, true, true }, 65535, false, 65535, 255 };
}

using SinglePhase = TestSource< 1, 0, 1, 0, false >;  // NO_OF_PHASES 1: no per-phase lines
using Minimal = TestSource< 3, 0, 1, 0, false >;
using Default = TestSource< 3, 0, 3, 0, false >;
using Relays = TestSource< 3, 2, 3, 0, false >;
using Temperatures = TestSource< 3, 0, 3, 2, false >;
using DualTariff = TestSource< 3, 0, 3, 0, true >;
using Everything = TestSource< 3, 2, 4, 4, true >;

/* the streamed frame is the buffered one, byte for byte, for every feature set */
void test_streamed_frame_equals_buffered(void)
{
  typicalValues();
  assertSameAsReference< SinglePhase >("single phase, typical");
  assertSameAsReference< Minimal >("minimal, typical");
  assertSameAsReference< Default >("default, typical");
  assertSameAsReference< Relays >("relays, typical");
  assertSameAsReference< Temperatures >("temperatures, typical");
  assertSameAsReference< DualTariff >("dual tariff, typical");
  assertSameAsReference< Everything >("every feature, typical");

  extremeValues();
  assertSameAsReference< SinglePhase >("single phase, extremes");
  assertSameAsReference< Minimal >("minimal, extremes");
  assertSameAsReference< Default >("default, extremes");
  assertSameAsReference< Relays >("relays, extremes");
  assertSameAsReference< Temperatures >("temperatures, extremes");
  assertSameAsReference< DualTariff >("dual tariff, extremes");
  assertSameAsReference< Everything >("every feature, extremes");

  // every temperature invalid: the temperature steps write nothing
  values.temperatureValid[0] = values.temperatureValid[1] = values.temperatureValid[2] = values.temperatureValid[3] = false;
  assertSameAsReference< Everything >("every feature, no valid temperature");
}

/* a step fits the serial output's (SerialOutput::MAX_STEP_LENGTH, 32): one line, STX or ETX */
void test_step_fits_serial_output(void)
{
  extremeValues();
  // 32: SerialOutput::MAX_STEP_LENGTH, out of reach here (serial_output.h needs Arduino.h);
  // utils.h checks it with a static_assert, the embedded test against the constant
  TEST_ASSERT_LESS_OR_EQUAL_UINT8(32, TeleInfo::MAX_STEP_LENGTH);
  TEST_ASSERT_LESS_OR_EQUAL_UINT8(TeleInfo::MAX_STEP_LENGTH, streamFrame< Everything >().longestStep);
}

/* one step per line: invalid temperatures included, they write nothing */
void test_one_line_per_step(void)
{
  typicalValues();
  // P D1 N S S_MC
  TEST_ASSERT_EQUAL_UINT8(5, streamFrame< SinglePhase >().steps);
  // P P3 V3 P2 V2 P1 V1 D1 N S S_MC
  TEST_ASSERT_EQUAL_UINT8(11, streamFrame< Minimal >().steps);
  // P (P3 V3 P2 V2 P1 V1) R R2 R1 D4..D1 T1..T4 N TA S S_MC
  TEST_ASSERT_EQUAL_UINT8(1 + 6 + 3 + 4 + 4 + 1 + 1 + 2, streamFrame< Everything >().steps);
}

/* a frame written independently of the code, checksums computed by hand */
void test_golden_frame(void)
{
  values = Values{};
  values.power = -25;
  values.phasePower[0] = 399;
  values.phasePower[1] = 399;
  values.phasePower[2] = -823;
  values.phaseVoltage_x100[0] = 23012;
  values.phaseVoltage_x100[1] = 23100;
  values.phaseVoltage_x100[2] = 22950;
  values.loadOnPercent[0] = 40;
  values.absenceCount = 7;
  values.sampleSets = 250;
  values.lowestSampleSetsPerCycle = 44;

  // checksum: the sum of the bytes from the tag to the second TAB, & 0x3F, + 0x20
  // P\t-25\t      0x0F6 -> 0x36 + 0x20 = 'V'
  // P3\t-823\t    0x15F -> 0x1F + 0x20 = '?'
  // V3\t22950\t   0x19D -> 0x1D + 0x20 = '='
  // P2\t399\t     0x139 -> 0x39 + 0x20 = 'Y'
  // V2\t23100\t   0x190 -> 0x10 + 0x20 = '0'
  // P1\t399\t     0x138 -> 0x38 + 0x20 = 'X'
  // V1\t23012\t   0x191 -> 0x11 + 0x20 = '1'
  // D1\t40\t      0x0EB -> 0x2B + 0x20 = 'K'
  // N\t7\t        0x097 -> 0x17 + 0x20 = '7'
  // S\t250\t      0x0FC -> 0x3C + 0x20 = '\'
  // S_MC\t44\t    0x1BC -> 0x3C + 0x20 = '\'
  static const char golden[]{ "\x02"
                              "\nP\t-25\tV\r"
                              "\nP3\t-823\t?\r"
                              "\nV3\t22950\t=\r"
                              "\nP2\t399\tY\r"
                              "\nV2\t23100\t0\r"
                              "\nP1\t399\tX\r"
                              "\nV1\t23012\t1\r"
                              "\nD1\t40\tK\r"
                              "\nN\t7\t7\r"
                              "\nS\t250\t\\\r"
                              "\nS_MC\t44\t\\\r"
                              "\x03" };

  const Streamed streamed{ streamFrame< Minimal >() };
  TEST_ASSERT_EQUAL_UINT32(sizeof golden - 1, streamed.length);
  TEST_ASSERT_EQUAL_MEMORY(golden, streamed.text, sizeof golden - 1);
}

/** @brief A line of a frame: its tag (with the index) and its value. */
struct Line
{
  char tag[8];
  char value[16];
};

/**
 * @brief Splits a frame into its lines, as a receiver does, checking STX, ETX and every
 *        line's format and checksum.
 * @return the number of lines
 */
static uint8_t parseFrame(const Streamed& frame, Line (&lines)[32])
{
  TEST_ASSERT_GREATER_THAN_UINT32(2, frame.length);
  TEST_ASSERT_EQUAL_CHAR(0x02, frame.text[0]);
  TEST_ASSERT_EQUAL_CHAR(0x03, frame.text[frame.length - 1]);

  uint8_t count{ 0 };
  size_t pos{ 1 };
  while (pos < frame.length - 1)
  {
    TEST_ASSERT_EQUAL_CHAR('\n', frame.text[pos++]);
    const size_t start{ pos };
    const char* const tab1{ static_cast< const char* >(memchr(frame.text + pos, '\t', frame.length - pos)) };
    TEST_ASSERT_NOT_NULL(tab1);
    const char* const tab2{ static_cast< const char* >(memchr(tab1 + 1, '\t', frame.length - (tab1 + 1 - frame.text))) };
    TEST_ASSERT_NOT_NULL(tab2);

    uint8_t sum{ 0 };
    for (const char* p = frame.text + start; p != tab2 + 1; ++p)
    {
      sum += static_cast< uint8_t >(*p);
    }
    TEST_ASSERT_EQUAL_CHAR((sum & 0x3F) + 0x20, tab2[1]);
    TEST_ASSERT_EQUAL_CHAR('\r', tab2[2]);

    Line& line{ lines[count++] };
    memset(&line, 0, sizeof line);
    memcpy(line.tag, frame.text + start, tab1 - (frame.text + start));
    memcpy(line.value, tab1 + 1, tab2 - (tab1 + 1));
    pos = (tab2 + 3) - frame.text;
  }
  return count;
}

/** @brief How many lines have exactly this tag. */
static uint8_t countTag(const Line (&lines)[32], uint8_t count, const char* tag)
{
  uint8_t found{ 0 };
  for (uint8_t i = 0; i != count; ++i)
  {
    found += strcmp(lines[i].tag, tag) == 0 ? 1 : 0;
  }
  return found;
}

/** @brief The value of the line with exactly this tag. */
static const char* valueOf(const Line (&lines)[32], uint8_t count, const char* tag)
{
  for (uint8_t i = 0; i != count; ++i)
  {
    if (strcmp(lines[i].tag, tag) == 0)
    {
      return lines[i].value;
    }
  }
  return "";
}

/* every tag of a frame with every feature, once, in order, with its value */
void test_every_tag(void)
{
  typicalValues();
  Line lines[32];
  const uint8_t count{ parseFrame(streamFrame< Everything >(), lines) };

  static const char* const order[]{ "P", "P3", "V3", "P2", "V2", "P1", "V1", "R", "R2", "R1", "D4", "D3", "D2", "D1", "T1", "T3", "T4", "N", "TA", "S", "S_MC" };
  TEST_ASSERT_EQUAL_UINT8(sizeof order / sizeof order[0], count);
  for (uint8_t i = 0; i != count; ++i)
  {
    TEST_ASSERT_EQUAL_STRING(order[i], lines[i].tag);
  }

  TEST_ASSERT_EQUAL_STRING("-1234", valueOf(lines, count, "P"));
  TEST_ASSERT_EQUAL_STRING("399", valueOf(lines, count, "P1"));
  TEST_ASSERT_EQUAL_STRING("399", valueOf(lines, count, "P2"));
  TEST_ASSERT_EQUAL_STRING("-2032", valueOf(lines, count, "P3"));
  TEST_ASSERT_EQUAL_STRING("23045", valueOf(lines, count, "V1"));
  TEST_ASSERT_EQUAL_STRING("23110", valueOf(lines, count, "V2"));
  TEST_ASSERT_EQUAL_STRING("22987", valueOf(lines, count, "V3"));
  TEST_ASSERT_EQUAL_STRING("-567", valueOf(lines, count, "R"));
  TEST_ASSERT_EQUAL_STRING("1", valueOf(lines, count, "R1"));  // Rn is relayOn(n - 1)
  TEST_ASSERT_EQUAL_STRING("0", valueOf(lines, count, "R2"));
  TEST_ASSERT_EQUAL_STRING("100", valueOf(lines, count, "D1"));
  TEST_ASSERT_EQUAL_STRING("37", valueOf(lines, count, "D2"));
  TEST_ASSERT_EQUAL_STRING("0", valueOf(lines, count, "D3"));
  TEST_ASSERT_EQUAL_STRING("5", valueOf(lines, count, "D4"));
  TEST_ASSERT_EQUAL_STRING("2137", valueOf(lines, count, "T1"));
  TEST_ASSERT_EQUAL_UINT8(0, countTag(lines, count, "T2"));  // invalid: no line
  TEST_ASSERT_EQUAL_STRING("6500", valueOf(lines, count, "T3"));
  TEST_ASSERT_EQUAL_STRING("-5", valueOf(lines, count, "T4"));
  TEST_ASSERT_EQUAL_STRING("731", valueOf(lines, count, "N"));
  TEST_ASSERT_EQUAL_STRING("1", valueOf(lines, count, "TA"));
  TEST_ASSERT_EQUAL_STRING("250", valueOf(lines, count, "S"));
  TEST_ASSERT_EQUAL_STRING("44", valueOf(lines, count, "S_MC"));
}

/* optional features on and off: their lines are there, or are not */
void test_optional_features(void)
{
  typicalValues();
  Line lines[32];

  uint8_t count{ parseFrame(streamFrame< Minimal >(), lines) };
  TEST_ASSERT_EQUAL_UINT8(0, countTag(lines, count, "R"));
  TEST_ASSERT_EQUAL_UINT8(0, countTag(lines, count, "R1"));
  TEST_ASSERT_EQUAL_UINT8(0, countTag(lines, count, "T1"));
  TEST_ASSERT_EQUAL_UINT8(0, countTag(lines, count, "TA"));
  TEST_ASSERT_EQUAL_UINT8(1, countTag(lines, count, "P3"));

  count = parseFrame(streamFrame< Relays >(), lines);
  TEST_ASSERT_EQUAL_UINT8(1, countTag(lines, count, "R"));
  TEST_ASSERT_EQUAL_UINT8(1, countTag(lines, count, "R1"));
  TEST_ASSERT_EQUAL_UINT8(1, countTag(lines, count, "R2"));
  TEST_ASSERT_EQUAL_UINT8(0, countTag(lines, count, "T1"));
  TEST_ASSERT_EQUAL_UINT8(0, countTag(lines, count, "TA"));

  count = parseFrame(streamFrame< Temperatures >(), lines);
  TEST_ASSERT_EQUAL_UINT8(1, countTag(lines, count, "T1"));
  TEST_ASSERT_EQUAL_UINT8(0, countTag(lines, count, "T2"));  // temperature 2 is invalid
  TEST_ASSERT_EQUAL_UINT8(0, countTag(lines, count, "R"));

  count = parseFrame(streamFrame< DualTariff >(), lines);
  TEST_ASSERT_EQUAL_STRING("1", valueOf(lines, count, "TA"));
  values.offPeak = false;
  count = parseFrame(streamFrame< DualTariff >(), lines);
  TEST_ASSERT_EQUAL_STRING("0", valueOf(lines, count, "TA"));

  // one phase: the total only, no P1/V1
  count = parseFrame(streamFrame< SinglePhase >(), lines);
  TEST_ASSERT_EQUAL_UINT8(0, countTag(lines, count, "P1"));
  TEST_ASSERT_EQUAL_UINT8(0, countTag(lines, count, "V1"));
  TEST_ASSERT_EQUAL_STRING("P", lines[0].tag);
}

/* N, the seconds without diverted energy, is unsigned: beyond 32767 s (9.1 h) it no longer
 * reads negative */
void test_absence_count_unsigned(void)
{
  typicalValues();
  Line lines[32];

  values.absenceCount = 40000;
  uint8_t count{ parseFrame(streamFrame< Default >(), lines) };
  TEST_ASSERT_EQUAL_STRING("40000", valueOf(lines, count, "N"));
  assertSameAsReference< Everything >("every feature, N = 40000");

  values.absenceCount = 65535;
  count = parseFrame(streamFrame< Everything >(), lines);
  TEST_ASSERT_EQUAL_STRING("65535", valueOf(lines, count, "N"));
}

/* V, the RMS voltage in hundredths of a volt, is unsigned: beyond 327.67 V (a datalog period that
 * would hold more V^2 than samples) it no longer reads negative */
void test_voltage_unsigned(void)
{
  typicalValues();
  Line lines[32];

  values.phaseVoltage_x100[0] = 46000;
  values.phaseVoltage_x100[1] = 32768;
  values.phaseVoltage_x100[2] = 65535;
  const uint8_t count{ parseFrame(streamFrame< Default >(), lines) };
  TEST_ASSERT_EQUAL_STRING("46000", valueOf(lines, count, "V1"));  // was -19536
  TEST_ASSERT_EQUAL_STRING("32768", valueOf(lines, count, "V2"));  // was -32768
  TEST_ASSERT_EQUAL_STRING("65535", valueOf(lines, count, "V3"));  // was -1
  assertSameAsReference< Default >("V = 46000, 32768, 65535");
  assertSameAsReference< Everything >("every feature, V = 46000, 32768, 65535");
}

/* the sample count above 32767 (long datalog periods) is unsigned too */
void test_sample_count_unsigned(void)
{
  typicalValues();
  Line lines[32];

  values.sampleSets = 64000;
  const uint8_t count{ parseFrame(streamFrame< Default >(), lines) };
  TEST_ASSERT_EQUAL_STRING("64000", valueOf(lines, count, "S"));
}

/* a single line: LF, tag and index, TAB, value, TAB, checksum, CR */
void test_line(void)
{
  char text[TeleInfo::MAX_STEP_LENGTH];

  // T1\t-5500\t: 0x18E -> 0x0E + 0x20 = '.'
  const char* end{ TeleInfo::line(text, "T", -5500, 1) };
  TEST_ASSERT_EQUAL_UINT32(12, end - text);
  TEST_ASSERT_EQUAL_MEMORY("\nT1\t-5500\t.\r", text, 12);

  // V1\t65535: unsigned, not -1
  end = TeleInfo::lineUnsigned(text, "V", 65535, 1);
  TEST_ASSERT_EQUAL_UINT32(TeleInfo::lineSize(2, 5), end - text);
  TEST_ASSERT_EQUAL_MEMORY("\nV1\t65535\t", text, 10);

  // the longest: a 4-character tag and a signed 5-digit value
  end = TeleInfo::line(text, "S_MC", -32768);
  TEST_ASSERT_EQUAL_UINT32(TeleInfo::lineSize(4, 6), end - text);
  TEST_ASSERT_EQUAL_UINT8(TeleInfo::MAX_STEP_LENGTH - 1, TeleInfo::lineSize(4, 6));

  // 0 and -32768
  end = TeleInfo::line(text, "P", 0);
  TEST_ASSERT_EQUAL_MEMORY("\nP\t0\t", text, 5);
  end = TeleInfo::line(text, "P", -32768);
  TEST_ASSERT_EQUAL_MEMORY("\nP\t-32768\t", text, 10);
}

int main(int /* argc */, char** /* argv */)
{
  UNITY_BEGIN();

  RUN_TEST(test_line);
  RUN_TEST(test_golden_frame);
  RUN_TEST(test_streamed_frame_equals_buffered);
  RUN_TEST(test_step_fits_serial_output);
  RUN_TEST(test_one_line_per_step);
  RUN_TEST(test_every_tag);
  RUN_TEST(test_optional_features);
  RUN_TEST(test_absence_count_unsigned);
  RUN_TEST(test_voltage_unsigned);
  RUN_TEST(test_sample_count_unsigned);

  return UNITY_END();
}
