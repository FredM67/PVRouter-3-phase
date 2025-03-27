#include <Arduino.h>
#include <unity.h>

#include "utils_pins.h"

void setUp(void)
{
  // set stuff up here
}

void tearDown(void)
{
  // clean stuff up here
}

void test_setPinON(void)
{
  setPinON(LED_BUILTIN);
  delay(100);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(LED_BUILTIN));
}

void test_setPinOFF(void)
{
  setPinOFF(LED_BUILTIN);
  delay(100);
  TEST_ASSERT_EQUAL(LOW, digitalRead(LED_BUILTIN));
}

void test_togglePin(void)
{
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  togglePin(LED_BUILTIN);
  delay(100);
  TEST_ASSERT_EQUAL(LOW, digitalRead(LED_BUILTIN));

  togglePin(LED_BUILTIN);
  delay(100);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(LED_BUILTIN));
}

void test_setPinState(void)
{
  setPinState(LED_BUILTIN, true);
  delay(100);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(LED_BUILTIN));
  setPinState(LED_BUILTIN, false);
  delay(100);
  TEST_ASSERT_EQUAL(LOW, digitalRead(LED_BUILTIN));
}

void test_setPinsON(void)
{
  const uint16_t pinsToSet{ 0b11111111111100 };

  setPinsON(pinsToSet);
  delay(100);

  for (uint8_t pin = 2; pin < 14; ++pin)
  {
    TEST_ASSERT_EQUAL(HIGH, digitalRead(pin));
  }
}

void test_setPinsOFF(void)
{
  const uint16_t pinsToSet{ 0b11111111111100 };

  setPinsOFF(pinsToSet);
  delay(100);

  for (uint8_t pin = 2; pin < 14; ++pin)
  {
    TEST_ASSERT_EQUAL(LOW, digitalRead(pin));
  }
}


void setup()
{
  delay(1000);

  for (uint8_t pin = 2; pin < 14; ++pin)
  {
    pinMode(pin, OUTPUT);
  }

  UNITY_BEGIN();  // IMPORTANT LINE!
}

uint8_t i = 0;
uint8_t max_blinks = 2;

void loop()
{
  if (i < max_blinks)
  {
    RUN_TEST(test_setPinON);
    delay(100);
    RUN_TEST(test_setPinOFF);
    delay(100);
    RUN_TEST(test_togglePin);
    delay(100);
    RUN_TEST(test_setPinState);
    delay(100);
    RUN_TEST(test_setPinsON);
    delay(100);
    RUN_TEST(test_setPinsOFF);
    delay(100);
    ++i;
  }
  else if (i == max_blinks)
  {
    UNITY_END();  // stop unit testing
  }
}