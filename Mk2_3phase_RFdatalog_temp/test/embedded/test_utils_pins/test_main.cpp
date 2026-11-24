// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <unity.h>

#include "../halt_after_tests.h"

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

void test_setPinsState(void)
{
  constexpr uint16_t allPins{ 0b11111111111100 };  // pins 2..13
  constexpr uint16_t evenPins{ 0b01010101010100 };
  constexpr uint16_t oddPins{ 0b10101010101000 };

  setPinsState< allPins >(allPins, 0);
  delay(100);
  for (uint8_t pin = 2; pin < 14; ++pin)
  {
    TEST_ASSERT_EQUAL(HIGH, digitalRead(pin));
  }

  setPinsState< allPins >(evenPins, oddPins);
  delay(100);
  for (uint8_t pin = 2; pin < 14; ++pin)
  {
    TEST_ASSERT_EQUAL((pin % 2) ? LOW : HIGH, digitalRead(pin));
  }

  setPinsState< allPins >(0, allPins);
  delay(100);
  for (uint8_t pin = 2; pin < 14; ++pin)
  {
    TEST_ASSERT_EQUAL(LOW, digitalRead(pin));
  }
}

void test_setPinsState_skipsUnusedPort(void)
{
  constexpr uint16_t portDPins{ 0b00000011111100 };  // pins 2..7
  constexpr uint16_t portBPins{ 0b11111100000000 };  // pins 8..13

  setPinsState< portBPins >(portBPins, 0);
  delay(100);
  // PORTD is outside usedPins: even asked to, the call leaves it alone
  setPinsState< portBPins >(portDPins, 0);
  delay(100);
  for (uint8_t pin = 2; pin < 14; ++pin)
  {
    TEST_ASSERT_EQUAL((pin < 8) ? LOW : HIGH, digitalRead(pin));
  }

  setPinsState< portBPins >(0, portBPins);
  delay(100);
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
    RUN_TEST(test_setPinsState);
    delay(100);
    RUN_TEST(test_setPinsState_skipsUnusedPort);
    delay(100);
    ++i;
  }
  else if (i == max_blinks)
  {
    UNITY_END();  // stop unit testing
    haltAfterTests();
  }
}
