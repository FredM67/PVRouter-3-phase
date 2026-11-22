// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file test_main.cpp
 * @brief Embedded tests for teleinfo.h: the IoT frame on the ATmega328P (16-bit int)
 *
 * @details The code of the firmware, run on the target: the lines (digits of 16-bit values,
 *          checksum) and whole frames written step by step, with and without the per-phase lines.
 *          The expected text is built independently, its checksums summed by appendLine(). The
 *          native tests (test/native/test_teleinfo) check the frame byte for byte against the
 *          buffered frame of the former firmware.
 */

#include <Arduino.h>
#include <unity.h>

#include "../halt_after_tests.h"

#include "serial_output.h"
#include "teleinfo.h"

void setUp(void)
{
  // Setup for each test
}

void tearDown(void)
{
  // Clean up after each test
}

/* one line as the router writes it: LF tag TAB value TAB checksum CR */
static void appendLine(char* expected, const char* tagAndValue)
{
  uint8_t sum{ 0 };
  for (const char* p = tagAndValue; *p; ++p)
  {
    sum += *p;
  }
  sum += '\t';
  const char line[]{ '\n', '\0' };
  strcat(expected, line);
  strcat(expected, tagAndValue);
  const char end[]{ '\t', static_cast< char >((sum & 0x3F) + 0x20), '\r', '\0' };
  strcat(expected, end);
}

/* the expected text of a single line */
static const char* expectedLine(const char* tagAndValue)
{
  static char expected[TeleInfo::MAX_STEP_LENGTH + 1];
  expected[0] = '\0';
  appendLine(expected, tagAndValue);
  return expected;
}

void test_lineSize_calculation(void)
{
  // LF + tag + TAB + value + TAB + checksum + CR = 5 + tagLength + valueLength
  TEST_ASSERT_EQUAL(5 + 1 + 3, TeleInfo::lineSize(1, 3));
  TEST_ASSERT_EQUAL(5 + 2 + 5, TeleInfo::lineSize(2, 5));
  TEST_ASSERT_EQUAL(5 + 4 + 2, TeleInfo::lineSize(4, 2));

  // a step fits the serial output's (SerialOutput::MAX_STEP_LENGTH)
  TEST_ASSERT_LESS_OR_EQUAL(SerialOutput::MAX_STEP_LENGTH, TeleInfo::MAX_STEP_LENGTH);
}

/* the values that used to be cut or wrong: a 5-character temperature, a voltage and a sample
 * count above 32767, an indexed relay state, the extremes of 16 bits */
void test_teleinfo_line_contents(void)
{
  char text[TeleInfo::MAX_STEP_LENGTH + 1];

  *TeleInfo::line(text, "T", -5500, 1) = '\0';
  TEST_ASSERT_EQUAL_STRING(expectedLine("T1\t-5500"), text);

  *TeleInfo::lineUnsigned(text, "S", 64000) = '\0';
  TEST_ASSERT_EQUAL_STRING(expectedLine("S\t64000"), text);

  *TeleInfo::lineUnsigned(text, "V", 46000, 2) = '\0';
  TEST_ASSERT_EQUAL_STRING(expectedLine("V2\t46000"), text);

  *TeleInfo::line(text, "R", 1, 2) = '\0';
  TEST_ASSERT_EQUAL_STRING(expectedLine("R2\t1"), text);

  *TeleInfo::line(text, "P", -32768) = '\0';
  TEST_ASSERT_EQUAL_STRING(expectedLine("P\t-32768"), text);

  *TeleInfo::line(text, "S_MC", 0) = '\0';
  TEST_ASSERT_EQUAL_STRING(expectedLine("S_MC\t0"), text);
}

/** @brief A data source of writeFrameStep(): 2 loads, 1 relay, 2 temperature sensors (the 2nd invalid). */
template< uint8_t PHASES >
struct TestSource
{
  static constexpr uint8_t PHASE_COUNT{ PHASES };
  static constexpr bool RELAY_DIVERSION{ true };
  static constexpr uint8_t RELAY_COUNT{ 1 };
  static constexpr uint8_t LOAD_COUNT{ 2 };
  static constexpr uint8_t TEMP_COUNT{ 2 };
  static constexpr bool DUAL_TARIFF{ true };

  static int16_t power()
  {
    return -1234;
  }
  static int16_t phasePower(uint8_t idx)
  {
    return static_cast< int16_t >(400 + idx);
  }
  static uint16_t phaseVoltage(uint8_t idx)
  {
    return static_cast< uint16_t >(46000 + idx);  // above 32767: unsigned
  }
  static int16_t relayAverage()
  {
    return -567;
  }
  static bool relayOn(uint8_t)
  {
    return true;
  }
  static int16_t loadOnPercent(uint8_t idx)
  {
    return idx == 0 ? 100 : 37;
  }
  static bool temperatureValid(uint8_t idx)
  {
    return idx == 0;
  }
  static int16_t temperature(uint8_t)
  {
    return 2137;
  }
  static uint16_t absenceCount()
  {
    return 40000;
  }
  static bool offPeak()
  {
    return true;
  }
  static uint16_t sampleSets()
  {
    return 64000;
  }
  static uint8_t lowestSampleSetsPerCycle()
  {
    return 44;
  }
};

/* captures a frame, written step by step as the serial output does */
class FramePrint : public Print
{
public:
  char text[240]{};
  size_t length{ 0 };

  using Print::write;

  size_t write(uint8_t c) override
  {
    if (length < sizeof text - 1)
    {
      text[length++] = static_cast< char >(c);
    }
    return 1;
  }
};

template< typename Source >
static void streamFrame(FramePrint& out)
{
  uint8_t step{ 0 };
  bool more{ true };
  while (more)
  {
    char text[TeleInfo::MAX_STEP_LENGTH];
    uint8_t length{ 0 };
    more = TeleInfo::writeFrameStep< Source >(step++, text, length);
    TEST_ASSERT_LESS_OR_EQUAL(TeleInfo::MAX_STEP_LENGTH, length);
    out.write(text, length);
  }
}

/* the frame of a 3-phase router: P, then P and V of each phase, the last phase first */
void test_frame_three_phases(void)
{
  FramePrint out;
  streamFrame< TestSource< 3 > >(out);

  char expected[240]{ '\x02' };
  appendLine(expected, "P\t-1234");
  appendLine(expected, "P3\t402");
  appendLine(expected, "V3\t46002");
  appendLine(expected, "P2\t401");
  appendLine(expected, "V2\t46001");
  appendLine(expected, "P1\t400");
  appendLine(expected, "V1\t46000");
  appendLine(expected, "R\t-567");
  appendLine(expected, "R1\t1");
  appendLine(expected, "D2\t37");
  appendLine(expected, "D1\t100");
  appendLine(expected, "T1\t2137");
  appendLine(expected, "N\t40000");
  appendLine(expected, "TA\t1");
  appendLine(expected, "S\t64000");
  appendLine(expected, "S_MC\t44");
  const char etx[]{ '\x03', '\0' };
  strcat(expected, etx);

  TEST_ASSERT_EQUAL_STRING(expected, out.text);
}

/* the frame of a single-phase setup: no per-phase lines */
void test_frame_single_phase(void)
{
  FramePrint out;
  streamFrame< TestSource< 1 > >(out);

  char expected[240]{ '\x02' };
  appendLine(expected, "P\t-1234");
  appendLine(expected, "R\t-567");
  appendLine(expected, "R1\t1");
  appendLine(expected, "D2\t37");
  appendLine(expected, "D1\t100");
  appendLine(expected, "T1\t2137");
  appendLine(expected, "N\t40000");
  appendLine(expected, "TA\t1");
  appendLine(expected, "S\t64000");
  appendLine(expected, "S_MC\t44");
  const char etx[]{ '\x03', '\0' };
  strcat(expected, etx);

  TEST_ASSERT_EQUAL_STRING(expected, out.text);
}

void setup()
{
  delay(2000);  // Give time for Serial to initialize

  Serial.begin(9600);
  Serial.println("Starting TeleInfo embedded tests...");

  UNITY_BEGIN();
}

void loop()
{
  RUN_TEST(test_lineSize_calculation);
  RUN_TEST(test_teleinfo_line_contents);
  RUN_TEST(test_frame_three_phases);
  RUN_TEST(test_frame_single_phase);

  Serial.println("All TeleInfo tests completed!");

  UNITY_END();
  haltAfterTests();
}
