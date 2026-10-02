#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "../halt_after_tests.h"

#include "serial_output.h"
#include "teleinfo.h"

/**
 * @brief A Print that captures the text, with a transmit buffer that fills up as it is written
 */
class CapturePrint : public Print
{
public:
  char text[400]{};
  size_t length{ 0 };
  int room{ 63 };         /**< free space in the "transmit buffer" */
  bool overflow{ false }; /**< a write went beyond the free space */

  size_t write(uint8_t c) override
  {
    if (room <= 0)
    {
      overflow = true;
    }
    --room;
    if (length < sizeof text - 1)
    {
      text[length++] = static_cast< char >(c);
      text[length] = '\0';
    }
    return 1;
  }
  int availableForWrite() override
  {
    return room;
  }
  void clear()
  {
    length = 0;
    text[0] = '\0';
    room = 63;
    overflow = false;
  }
};

CapturePrint capture;

/* an output of 10 fields and a line end */
bool fieldsStep(Print& out, uint8_t step)
{
  if (step < 10)
  {
    out.print(F("field"));
    out.print(step);
    out.print(F(" = some value; "));
    return true;
  }
  out.println(F("end"));
  return false;
}

void setUp(void)
{
  capture.clear();
  SerialOutput::current = nullptr;
}

void tearDown(void)
{
}

void test_poll_waits_for_room(void)
{
  TEST_ASSERT_TRUE(SerialOutput::start(fieldsStep));

  capture.room = SerialOutput::MAX_STEP_LENGTH - 1;
  SerialOutput::poll(capture);

  TEST_ASSERT_EQUAL(0, capture.length);
  TEST_ASSERT_TRUE(SerialOutput::busy());
}

void test_poll_never_overfills_and_completes(void)
{
  TEST_ASSERT_TRUE(SerialOutput::start(fieldsStep));

  uint8_t polls{ 0 };
  while (SerialOutput::busy() && polls < 50)
  {
    SerialOutput::poll(capture);
    capture.room = 63;  // the UART sent everything meanwhile
    ++polls;
  }

  TEST_ASSERT_FALSE(SerialOutput::busy());
  TEST_ASSERT_FALSE(capture.overflow);
  TEST_ASSERT_GREATER_THAN(1, polls);  // it did take several passes
  TEST_ASSERT_EQUAL_STRING("field0 = some value; field1 = some value; field2 = some value; field3 = some value; "
                           "field4 = some value; field5 = some value; field6 = some value; field7 = some value; "
                           "field8 = some value; field9 = some value; end\r\n",
                           capture.text);
}

void test_start_while_busy_is_refused(void)
{
  TEST_ASSERT_TRUE(SerialOutput::start(fieldsStep));
  TEST_ASSERT_FALSE(SerialOutput::start(fieldsStep));
}

/* the JSON format used to be written by ArduinoJson: the same numbers, in the shortest
 * exact form (ArduinoJson showed float noise for about 2 % of them, -9.969999 for -9.97) */
void test_printHundredths_matches_arduinojson(void)
{
  uint16_t mismatches{ 0 };
  uint16_t noise{ 0 };
  char message[80]{};

  for (int32_t value = -5500; value <= 12500; ++value)
  {
    char json[24];
    StaticJsonDocument< 16 > doc;
    doc.set(static_cast< float >(value) * 0.01F);
    serializeJson(doc, json, sizeof json);

    capture.clear();
    SerialOutput::printHundredths(capture, static_cast< int16_t >(value));

    const char* point{ strchr(capture.text, '.') };
    const size_t decimals{ point ? strlen(point + 1) : 0 };
    const bool shortest{ decimals <= 2 && (!point || capture.text[capture.length - 1] != '0') };
    const bool sameValue{ fabs(atof(json) - atof(capture.text)) <= 1e-5 * (1 + fabs(atof(json))) };

    noise += strcmp(json, capture.text) != 0;
    if (!(shortest && sameValue) && !mismatches++)
    {
      snprintf(message, sizeof message, "%ld: ArduinoJson '%s', printHundredths '%s'", static_cast< long >(value), json,
               capture.text);
    }
  }
  TEST_ASSERT_EQUAL_MESSAGE(0, mismatches, message);
  TEST_ASSERT_LESS_THAN(1000, noise);  // otherwise the two formats really differ
}

void test_printHundredths_extremes(void)
{
  SerialOutput::printHundredths(capture, -32768);
  TEST_ASSERT_EQUAL_STRING("-327.68", capture.text);

  capture.clear();
  SerialOutput::printHundredths(capture, 32767);
  TEST_ASSERT_EQUAL_STRING("327.67", capture.text);

  capture.clear();
  SerialOutput::printHundredths(capture, -5);
  TEST_ASSERT_EQUAL_STRING("-0.05", capture.text);
}

/* the datalog used to print voltages and temperatures with print(value_x100 * 0.01F): the same text */
void test_printDecimals_matches_print_float(void)
{
  uint16_t mismatches{ 0 };
  char message[80]{};
  char reference[16]{};  // not a second CapturePrint: 2 kB of RAM

  for (int32_t value = -5500; value <= 12500; ++value)
  {
    capture.clear();
    capture.print(static_cast< float >(value) * 0.01F);
    strlcpy(reference, capture.text, sizeof reference);

    capture.clear();
    SerialOutput::printDecimals(capture, value, 2);

    if (strcmp(reference, capture.text) && !mismatches++)
    {
      snprintf_P(message, sizeof message, PSTR("%ld: print(float) '%s', printDecimals '%s'"), static_cast< long >(value),
                 reference, capture.text);
    }
  }
  TEST_ASSERT_EQUAL_MESSAGE(0, mismatches, message);
}

/* the captured text, compared with a string kept in flash: the test is short of RAM */
void assertCaptured(PGM_P expected)
{
  char text[16];
  strlcpy_P(text, expected, sizeof text);
  TEST_ASSERT_EQUAL_STRING(text, capture.text);
}

void test_printDecimals_other_decimals_and_extremes(void)
{
  SerialOutput::printDecimals(capture, 50000, 6);
  assertCaptured(PSTR("0.050000"));

  capture.clear();
  SerialOutput::printDecimals(capture, 81510, 5);
  assertCaptured(PSTR("0.81510"));

  capture.clear();
  SerialOutput::printDecimals(capture, -10, 2);
  assertCaptured(PSTR("-0.10"));

  capture.clear();
  SerialOutput::printDecimals(capture, 42, 0);
  assertCaptured(PSTR("42"));

  capture.clear();
  SerialOutput::printDecimals(capture, INT32_MIN, 2);
  assertCaptured(PSTR("-21474836.48"));
}

/* the calibration constants of the startup summary, converted at compile time */
void test_toDecimals_at_compile_time(void)
{
  static_assert(SerialOutput::toDecimals(0.05F, 6) == 50000, "f_powerCal");
  static_assert(SerialOutput::toDecimals(0.8151F, 5) == 81510, "f_voltageCal");
  static_assert(SerialOutput::toDecimals(1.0F, 2) == 100, "f_phaseCal");
  static_assert(SerialOutput::toDecimals(-0.1F, 2) == -10, "rounded away from zero");

  constexpr float values[]{ 0.05F, 0.0612F };
  constexpr auto table{ SerialOutput::toDecimals(values, 6) };
  static_assert(table.value[0] == 50000 && table.value[1] == 61200, "array version");

  TEST_PASS();
}

/* a frame written a part at a time is the same as written at once */
void test_teleinfo_written_in_parts(void)
{
  static TeleInfo teleinfo;
  char whole[200];

  auto buildFrame = []()
  {
    teleinfo.startFrame();
    teleinfo.send("P", -1234);
    teleinfo.send("V", 23012, 1);
    teleinfo.send("T", 2137, 2);
    teleinfo.endFrame();
  };

  buildFrame();
  capture.room = 1000;
  TEST_ASSERT_FALSE(teleinfo.writeNext(capture, 255));
  TEST_ASSERT_LESS_THAN(sizeof whole, capture.length);
  memcpy(whole, capture.text, capture.length + 1);
  TEST_ASSERT_EQUAL_CHAR(0x02, whole[0]);
  TEST_ASSERT_EQUAL_CHAR(0x03, whole[capture.length - 1]);

  buildFrame();
  capture.clear();
  uint8_t parts{ 1 };
  capture.room = 1000;
  while (teleinfo.writeNext(capture, 7))
  {
    ++parts;
  }
  TEST_ASSERT_GREATER_THAN(3, parts);
  TEST_ASSERT_EQUAL_STRING(whole, capture.text);
}

void setup()
{
  delay(1000);

  UNITY_BEGIN();

  RUN_TEST(test_poll_waits_for_room);
  RUN_TEST(test_poll_never_overfills_and_completes);
  RUN_TEST(test_start_while_busy_is_refused);
  RUN_TEST(test_printHundredths_matches_arduinojson);
  RUN_TEST(test_printHundredths_extremes);
  RUN_TEST(test_printDecimals_matches_print_float);
  RUN_TEST(test_printDecimals_other_decimals_and_extremes);
  RUN_TEST(test_toDecimals_at_compile_time);
  RUN_TEST(test_teleinfo_written_in_parts);

  UNITY_END();
  haltAfterTests();
}

void loop()
{
}
