#include <Arduino.h>

#include <unity.h>

#include "utils_pins.h"
#include "utils_relay.h"

constexpr RelayEngine relays{ { { 2, 1000, 200, 1, 1 },
                                { 3, 100, 20, 2, 3 } } };

void setUp(void)
{
  // Setup for each test
}

void tearDown(void)
{
  // clean stuff up here
}

void test_relay_initialization(void)
{
  relayOutput relay(4, 500, 100);
  TEST_ASSERT_EQUAL(4, relay.get_pin());
  TEST_ASSERT_EQUAL(500, relay.get_surplusThreshold());
  TEST_ASSERT_EQUAL(100, relay.get_importThreshold());
}

void test_relay_initialization_with_positive_thresholds(void)
{
  relayOutput relay(4, 500, 100);
  TEST_ASSERT_EQUAL(4, relay.get_pin());
  TEST_ASSERT_EQUAL(500, relay.get_surplusThreshold());
  TEST_ASSERT_EQUAL(100, relay.get_importThreshold());
}

void test_relay_initialization_with_negative_thresholds(void)
{
  relayOutput relay(4, -500, -100);
  TEST_ASSERT_EQUAL(4, relay.get_pin());
  TEST_ASSERT_EQUAL(500, relay.get_surplusThreshold());
  TEST_ASSERT_EQUAL(-100, relay.get_importThreshold());
}

void test_get_pin(void)
{
  TEST_ASSERT_EQUAL(2, relays.get_relay(0).get_pin());
  TEST_ASSERT_EQUAL(3, relays.get_relay(1).get_pin());
}

void test_get_surplusThreshold(void)
{
  TEST_ASSERT_EQUAL(1000, relays.get_relay(0).get_surplusThreshold());
  TEST_ASSERT_EQUAL(100, relays.get_relay(1).get_surplusThreshold());
}

void test_get_importThreshold(void)
{
  TEST_ASSERT_EQUAL(200, relays.get_relay(0).get_importThreshold());
  TEST_ASSERT_EQUAL(20, relays.get_relay(1).get_importThreshold());
}

void test_get_minON(void)
{
  TEST_ASSERT_EQUAL(1 * 60, relays.get_relay(0).get_minON());
  TEST_ASSERT_EQUAL(2 * 60, relays.get_relay(1).get_minON());
}

void test_get_minOFF(void)
{
  TEST_ASSERT_EQUAL(1 * 60, relays.get_relay(0).get_minOFF());
  TEST_ASSERT_EQUAL(3 * 60, relays.get_relay(1).get_minOFF());
}

void test_isRelayON(void)
{
  TEST_ASSERT_FALSE(relays.get_relay(1).isRelayON());
}

void test_relay_turnON(void)
{
  const auto& my_relay{ relays.get_relay(1) };

  TEST_ASSERT_FALSE(my_relay.isRelayON());

  /* The relay is OFF, test the "TurnON" case */
  const auto surplus{ -my_relay.get_surplusThreshold() - 1 };
  uint16_t overrideBitmask{ 0 };  // No override active

  TEST_ASSERT_FALSE(my_relay.proceed_relay(surplus, overrideBitmask));
  delay(100);
  TEST_ASSERT_FALSE(my_relay.isRelayON());

  for (uint8_t timer = 0; timer < my_relay.get_minOFF() - 1; ++timer)
  {
    my_relay.inc_duration();
  }
  overrideBitmask = 0;  // Reset bitmask
  TEST_ASSERT_FALSE(my_relay.proceed_relay(surplus, overrideBitmask));
  delay(100);

  my_relay.inc_duration();

  overrideBitmask = 0;  // Reset bitmask
  TEST_ASSERT_TRUE(my_relay.proceed_relay(surplus, overrideBitmask));
  TEST_ASSERT_TRUE(my_relay.isRelayON());
}

void test_relay_turnOFF(void)
{
  const auto& my_relay{ relays.get_relay(1) };

  TEST_ASSERT_TRUE(my_relay.isRelayON());

  /* The relay is ON, test the "TurnOFF" case */
  const auto consum{ my_relay.get_importThreshold() + 1 };
  uint16_t overrideBitmask{ 0 };  // No override active

  TEST_ASSERT_FALSE(my_relay.proceed_relay(consum, overrideBitmask));
  delay(100);
  TEST_ASSERT_TRUE(my_relay.isRelayON());

  for (uint8_t timer = 0; timer < my_relay.get_minON() - 1; ++timer)
  {
    my_relay.inc_duration();
  }
  overrideBitmask = 0;  // Reset bitmask
  TEST_ASSERT_FALSE(my_relay.proceed_relay(consum, overrideBitmask));
  delay(100);

  my_relay.inc_duration();

  overrideBitmask = 0;  // Reset bitmask
  TEST_ASSERT_TRUE(my_relay.proceed_relay(consum, overrideBitmask));
  TEST_ASSERT_FALSE(my_relay.isRelayON());
}

void test_relay_override_turnON(void)
{
  const auto& my_relay{ relays.get_relay(1) };

  // Ensure relay is OFF initially
  TEST_ASSERT_FALSE(my_relay.isRelayON());

  // Test override with insufficient surplus (would normally not turn ON)
  const auto insufficient_surplus{ -my_relay.get_surplusThreshold() + 100 };

  // Test at half the minimum OFF time - should still be blocked
  for (uint8_t timer = 0; timer < my_relay.get_minOFF() / 2; ++timer)
  {
    my_relay.inc_duration();
  }

  uint16_t overrideBitmask{ (1U << my_relay.get_pin()) };
  TEST_ASSERT_FALSE(my_relay.proceed_relay(insufficient_surplus, overrideBitmask));
  TEST_ASSERT_FALSE(my_relay.isRelayON());
  TEST_ASSERT_EQUAL(0, overrideBitmask & (1U << my_relay.get_pin()));  // Bit cleared

  // Increment to just before minimum OFF time is reached
  for (uint8_t timer = my_relay.get_minOFF() / 2; timer < my_relay.get_minOFF() - 1; ++timer)
  {
    my_relay.inc_duration();
  }

  overrideBitmask = (1U << my_relay.get_pin());
  TEST_ASSERT_FALSE(my_relay.proceed_relay(insufficient_surplus, overrideBitmask));
  TEST_ASSERT_FALSE(my_relay.isRelayON());
  TEST_ASSERT_EQUAL(0, overrideBitmask & (1U << my_relay.get_pin()));  // Bit cleared

  // Increment one more time - now minimum OFF time is reached
  my_relay.inc_duration();

  overrideBitmask = (1U << my_relay.get_pin());
  TEST_ASSERT_TRUE(my_relay.proceed_relay(insufficient_surplus, overrideBitmask));
  TEST_ASSERT_TRUE(my_relay.isRelayON());
  TEST_ASSERT_EQUAL(0, overrideBitmask & (1U << my_relay.get_pin()));  // Bit cleared
}

void test_relay_override_minimum_ON_time(void)
{
  const auto& my_relay{ relays.get_relay(1) };

  // Relay should be ON from previous test
  TEST_ASSERT_TRUE(my_relay.isRelayON());

  // Test that relay respects minimum ON time even after override is released
  const auto high_import{ my_relay.get_importThreshold() + 100 };  // High import should turn OFF relay
  uint16_t overrideBitmask = 0;                                    // No override active anymore

  // Test at half the minimum ON time - should NOT turn OFF yet
  for (uint8_t timer = 0; timer < my_relay.get_minON() / 2; ++timer)
  {
    my_relay.inc_duration();
  }

  TEST_ASSERT_FALSE(my_relay.proceed_relay(high_import, overrideBitmask));
  TEST_ASSERT_TRUE(my_relay.isRelayON());  // Should stay ON

  // Test just before minimum ON time is reached
  for (uint8_t timer = my_relay.get_minON() / 2; timer < my_relay.get_minON() - 1; ++timer)
  {
    my_relay.inc_duration();
  }

  TEST_ASSERT_FALSE(my_relay.proceed_relay(high_import, overrideBitmask));
  TEST_ASSERT_TRUE(my_relay.isRelayON());  // Should still stay ON

  // Increment one more time - now minimum ON time is reached
  my_relay.inc_duration();

  TEST_ASSERT_TRUE(my_relay.proceed_relay(high_import, overrideBitmask));
  TEST_ASSERT_FALSE(my_relay.isRelayON());  // Now it can turn OFF
}

void test_proceed_relay(void)
{
  RUN_TEST(test_relay_turnON);
  delay(100);
  RUN_TEST(test_relay_turnOFF);
  delay(100);
  RUN_TEST(test_relay_override_turnON);
  delay(100);
  RUN_TEST(test_relay_override_minimum_ON_time);
}

void test_get_size(void)
{
  TEST_ASSERT_EQUAL(2, relays.size());
}

void setup()
{
  delay(1000);

  UNITY_BEGIN();  // IMPORTANT LINE!
}

void loop()
{
  RUN_TEST(test_relay_initialization);
  RUN_TEST(test_relay_initialization_with_positive_thresholds);
  RUN_TEST(test_relay_initialization_with_negative_thresholds);

  RUN_TEST(test_get_size);

  RUN_TEST(test_get_pin);

  RUN_TEST(test_get_surplusThreshold);
  RUN_TEST(test_get_importThreshold);

  RUN_TEST(test_get_minON);
  RUN_TEST(test_get_minOFF);

  RUN_TEST(test_isRelayON);

  RUN_TEST(test_proceed_relay);

  UNITY_END();  // stop unit testing
}