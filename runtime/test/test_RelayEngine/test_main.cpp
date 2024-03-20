#include <Arduino.h>
#include <unity.h>

#include "utils_relay.h"

// Assuming you have a RelayOutput array defined somewhere
const RelayOutput relayArray[2]{ { 5, 1000, 200, 5, 5 }, { 6, 1000, 200, 5, 5 } };

RelayEngine< 2 >* relayEngineTest;

void setUp(void)
{
  relayEngineTest = new RelayEngine< 2 >(relayArray);
}

void tearDown(void)
{
  delete relayEngineTest;
}

void test_get_size(void)
{
  TEST_ASSERT_EQUAL(2, relayEngineTest->get_size());
}

void test_get_relay(void)
{
    for(uint8_t i = 0; i < relayEngineTest->get_size(); ++i) {
        TEST_ASSERT_EQUAL(true, relayArray[i].get_pin() == relayEngineTest->get_relay(i).get_pin());
        TEST_ASSERT_EQUAL(true, relayArray[i].get_minOFF() == relayEngineTest->get_relay(i).get_minOFF());
        TEST_ASSERT_EQUAL(true, relayArray[i].get_minON() == relayEngineTest->get_relay(i).get_minON());
        TEST_ASSERT_EQUAL(true, relayArray[i].get_surplusThreshold() == relayEngineTest->get_relay(i).get_surplusThreshold());
        TEST_ASSERT_EQUAL(true, relayArray[i].get_importThreshold() == relayEngineTest->get_relay(i).get_importThreshold());
    }
}

void test_get_average(void)
{
  // Assuming initial average is 0
  TEST_ASSERT_EQUAL(0, RelayEngine< 2 >::get_average());
}

void test_update_average(void)
{
  RelayEngine< 2 >::update_average(100);
  // Assuming the average changes after update
  TEST_ASSERT_NOT_EQUAL(0, RelayEngine< 2 >::get_average());
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_get_size);
  RUN_TEST(test_get_relay);
  RUN_TEST(test_get_average);
  RUN_TEST(test_update_average);
  UNITY_END();
}

void loop()
{
  // Do nothing here
}