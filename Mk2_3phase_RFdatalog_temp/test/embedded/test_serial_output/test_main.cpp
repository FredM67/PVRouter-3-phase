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
  RUN_TEST(test_teleinfo_written_in_parts);

  UNITY_END();
  haltAfterTests();
}

void loop()
{
}
