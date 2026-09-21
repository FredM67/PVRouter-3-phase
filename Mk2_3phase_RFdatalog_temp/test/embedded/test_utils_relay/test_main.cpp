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
  uint16_t overrideBitmask = 0;  // No override active

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
  uint16_t overrideBitmask = 0;  // No override active

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

  uint16_t overrideBitmask = (1U << my_relay.get_pin());
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

// ============================================================================
// RelayEngine settle_change tests
// ============================================================================

// Use a different D parameter (1 minute instead of 10) to:
// 1. Create a separate EWMA instance (different template instantiation)
// 2. Have faster EWMA convergence (fewer samples needed)
// EWMA window = D * 60 / DATALOG_PERIOD_IN_SECONDS = 1 * 60 / 5 = 12 samples
constexpr RelayEngine< 2, 1 > settleTestRelays{ integral_constant< uint8_t, 1 >{},
                                                { { 8, 500, 100, 1, 1 },
                                                  { 9, 800, 150, 1, 1 } } };

void test_settle_change_blocks_initial_relay_changes(void)
{
  // RelayEngine initializes settle_change to 60 seconds
  // proceed_relays() should return early and not change any relay state

  // Ensure relays are OFF initially
  TEST_ASSERT_FALSE(settleTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_FALSE(settleTestRelays.get_relay(1).isRelayON());

  // Feed surplus power - need enough samples for EWMA to converge
  // With D=1, window is 12 samples, so ~50 samples should be enough
  for (uint8_t i = 0; i < 50; ++i)
  {
    settleTestRelays.update_average(-600);  // 600W surplus
  }

  // Try to proceed relays - should be blocked by settle_change
  uint16_t overrideBitmask = 0;
  settleTestRelays.proceed_relays(overrideBitmask);

  // Relays should still be OFF because settle_change blocks changes
  TEST_ASSERT_FALSE(settleTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_FALSE(settleTestRelays.get_relay(1).isRelayON());
}

void test_settle_change_allows_changes_after_60_seconds(void)
{
  // After 60 calls to inc_duration(), settle_change should reach 0
  // and allow relay state changes

  // Wait for minOFF to elapse (60 seconds) AND settle_change to reach 0
  for (uint8_t i = 0; i < 60; ++i)
  {
    settleTestRelays.inc_duration();
  }

  // Feed more surplus to ensure EWMA is well below threshold
  for (uint8_t i = 0; i < 50; ++i)
  {
    settleTestRelays.update_average(-600);  // 600W surplus > 500W threshold
  }

  // Now proceed_relays should be able to turn ON relay 0
  uint16_t overrideBitmask = 0;
  settleTestRelays.proceed_relays(overrideBitmask);

  // Relay 0 should now be ON (it has the lower threshold)
  TEST_ASSERT_TRUE(settleTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_FALSE(settleTestRelays.get_relay(1).isRelayON());
}

void test_settle_change_resets_after_relay_change(void)
{
  // After a relay changes state, settle_change is reset to 60
  // This should block the next relay from changing immediately

  // Relay 0 is ON from previous test
  TEST_ASSERT_TRUE(settleTestRelays.get_relay(0).isRelayON());

  // Feed more surplus to try to turn ON relay 1
  for (uint8_t i = 0; i < 50; ++i)
  {
    settleTestRelays.update_average(-1000);  // 1000W surplus > 800W threshold
  }

  // Try to proceed relays immediately - should be blocked by settle_change
  uint16_t overrideBitmask = 0;
  settleTestRelays.proceed_relays(overrideBitmask);

  // Relay 1 should still be OFF because settle_change was reset to 60
  TEST_ASSERT_FALSE(settleTestRelays.get_relay(1).isRelayON());

  // Wait for settle_change to elapse (60 seconds)
  for (uint8_t i = 0; i < 60; ++i)
  {
    settleTestRelays.inc_duration();
  }

  // Keep feeding surplus
  for (uint8_t i = 0; i < 50; ++i)
  {
    settleTestRelays.update_average(-1000);
  }

  // Now relay 1 should be able to turn ON
  overrideBitmask = 0;
  settleTestRelays.proceed_relays(overrideBitmask);

  TEST_ASSERT_TRUE(settleTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_TRUE(settleTestRelays.get_relay(1).isRelayON());
}

void test_settle_change_blocks_turn_off_as_well(void)
{
  // Both relays are ON from previous test
  TEST_ASSERT_TRUE(settleTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_TRUE(settleTestRelays.get_relay(1).isRelayON());

  // Feed import power to try to turn OFF relays
  for (uint8_t i = 0; i < 50; ++i)
  {
    settleTestRelays.update_average(300);  // 300W import > both import thresholds
  }

  // Try to proceed relays immediately - should be blocked
  uint16_t overrideBitmask = 0;
  settleTestRelays.proceed_relays(overrideBitmask);

  // Both relays should still be ON (blocked by settle_change)
  TEST_ASSERT_TRUE(settleTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_TRUE(settleTestRelays.get_relay(1).isRelayON());

  // Wait for settle_change AND minON to elapse
  for (uint8_t i = 0; i < 60; ++i)
  {
    settleTestRelays.inc_duration();
  }

  // Keep feeding import
  for (uint8_t i = 0; i < 50; ++i)
  {
    settleTestRelays.update_average(300);
  }

  // Now relay 1 should turn OFF first (reverse order for import)
  overrideBitmask = 0;
  settleTestRelays.proceed_relays(overrideBitmask);

  TEST_ASSERT_TRUE(settleTestRelays.get_relay(0).isRelayON());   // Still ON
  TEST_ASSERT_FALSE(settleTestRelays.get_relay(1).isRelayON());  // Turned OFF
}

void test_proceed_settle_change(void)
{
  RUN_TEST(test_settle_change_blocks_initial_relay_changes);
  delay(100);
  RUN_TEST(test_settle_change_allows_changes_after_60_seconds);
  delay(100);
  RUN_TEST(test_settle_change_resets_after_relay_change);
  delay(100);
  RUN_TEST(test_settle_change_blocks_turn_off_as_well);
}

// ============================================================================
// Relay ordering tests
// ============================================================================

// Use 3 relays to clearly demonstrate ordering behavior
// D=3 for separate EWMA instance
// All relays have same thresholds to focus on ordering, not threshold differences
constexpr RelayEngine< 3, 3 > orderingTestRelays{ integral_constant< uint8_t, 3 >{},
                                                  { { 11, 500, 100, 1, 1 },
                                                    { 12, 500, 100, 1, 1 },
                                                    { 13, 500, 100, 1, 1 } } };

void feed_surplus_to_ordering_relays(void)
{
  // Feed enough samples for EWMA to converge to surplus
  for (uint8_t i = 0; i < 50; ++i)
  {
    orderingTestRelays.update_average(-600);  // 600W surplus
  }
}

void feed_import_to_ordering_relays(void)
{
  // Feed enough samples for EWMA to converge to import
  for (uint8_t i = 0; i < 50; ++i)
  {
    orderingTestRelays.update_average(200);  // 200W import
  }
}

void wait_for_relay_change_allowed(void)
{
  // Wait for settle_change (60s) and minON/minOFF (60s) to elapse
  for (uint8_t i = 0; i < 60; ++i)
  {
    orderingTestRelays.inc_duration();
  }
}

void test_relay_ordering_surplus_turns_on_ascending(void)
{
  // With surplus power, relays should turn ON in ascending order: 0 → 1 → 2
  // This ensures lower-priority loads are activated first

  // Verify all relays start OFF
  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(1).isRelayON());
  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(2).isRelayON());

  // Wait for initial settle_change to expire
  wait_for_relay_change_allowed();

  // Feed surplus power
  feed_surplus_to_ordering_relays();

  // First proceed_relays call - relay 0 should turn ON
  uint16_t overrideBitmask = 0;
  orderingTestRelays.proceed_relays(overrideBitmask);

  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(0).isRelayON());  // ON first
  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(1).isRelayON());
  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(2).isRelayON());

  // Wait for settle_change to expire
  wait_for_relay_change_allowed();
  feed_surplus_to_ordering_relays();

  // Second proceed_relays call - relay 1 should turn ON
  overrideBitmask = 0;
  orderingTestRelays.proceed_relays(overrideBitmask);

  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(1).isRelayON());  // ON second
  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(2).isRelayON());

  // Wait for settle_change to expire
  wait_for_relay_change_allowed();
  feed_surplus_to_ordering_relays();

  // Third proceed_relays call - relay 2 should turn ON
  overrideBitmask = 0;
  orderingTestRelays.proceed_relays(overrideBitmask);

  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(1).isRelayON());
  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(2).isRelayON());  // ON third
}

void test_relay_ordering_import_turns_off_descending(void)
{
  // With import power, relays should turn OFF in descending order: 2 → 1 → 0
  // This ensures higher-priority loads stay on longer

  // All relays are ON from previous test
  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(1).isRelayON());
  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(2).isRelayON());

  // Wait for settle_change to expire
  wait_for_relay_change_allowed();

  // Feed import power
  feed_import_to_ordering_relays();

  // First proceed_relays call - relay 2 should turn OFF (last ON, first OFF)
  uint16_t overrideBitmask = 0;
  orderingTestRelays.proceed_relays(overrideBitmask);

  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(1).isRelayON());
  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(2).isRelayON());  // OFF first

  // Wait for settle_change to expire
  wait_for_relay_change_allowed();
  feed_import_to_ordering_relays();

  // Second proceed_relays call - relay 1 should turn OFF
  overrideBitmask = 0;
  orderingTestRelays.proceed_relays(overrideBitmask);

  TEST_ASSERT_TRUE(orderingTestRelays.get_relay(0).isRelayON());
  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(1).isRelayON());  // OFF second
  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(2).isRelayON());

  // Wait for settle_change to expire
  wait_for_relay_change_allowed();
  feed_import_to_ordering_relays();

  // Third proceed_relays call - relay 0 should turn OFF
  overrideBitmask = 0;
  orderingTestRelays.proceed_relays(overrideBitmask);

  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(0).isRelayON());  // OFF third
  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(1).isRelayON());
  TEST_ASSERT_FALSE(orderingTestRelays.get_relay(2).isRelayON());
}

void test_relay_ordering(void)
{
  RUN_TEST(test_relay_ordering_surplus_turns_on_ascending);
  delay(100);
  RUN_TEST(test_relay_ordering_import_turns_off_descending);
}

// ============================================================================
// Duration overflow tests
// ============================================================================

// Use D=2 to create a separate EWMA instance from other tests
// Using pin 10 to avoid conflicts with other tests
constexpr RelayEngine< 1, 2 > overflowTestRelays{ integral_constant< uint8_t, 2 >{},
                                                  { { 10, 500, 100, 1, 1 } } };

void test_duration_overflow_saturates_at_uint16_max(void)
{
  // Test that duration saturates at UINT16_MAX and doesn't wrap to 0.
  // If it wrapped, the relay would be incorrectly blocked after overflow.
  //
  // The inc_duration() function has this protection:
  //   if (duration < UINT16_MAX) { ++duration; }

  const auto& relay = overflowTestRelays.get_relay(0);

  // Ensure relay is OFF initially
  TEST_ASSERT_FALSE(relay.isRelayON());

  // Increment duration to UINT16_MAX (65535 iterations)
  // This takes only a few seconds on Arduino - no actual waiting involved
  for (uint32_t i = 0; i < UINT16_MAX; ++i)
  {
    relay.inc_duration();
  }

  // At this point duration == UINT16_MAX (65535)
  // Relay should be able to turn ON (duration >> minOFF which is 60)
  uint16_t overrideBitmask = 0;
  const int32_t surplus = -600;  // 600W surplus > 500W threshold
  TEST_ASSERT_TRUE(relay.proceed_relay(surplus, overrideBitmask));
  TEST_ASSERT_TRUE(relay.isRelayON());
}

void test_duration_overflow_stays_at_max_after_more_increments(void)
{
  // After reaching UINT16_MAX, further increments should not wrap to 0
  // The relay should remain functional

  const auto& relay = overflowTestRelays.get_relay(0);

  // Relay is ON from previous test
  TEST_ASSERT_TRUE(relay.isRelayON());

  // Increment many more times beyond UINT16_MAX
  // If there was no saturation check, this would wrap duration to ~1000
  // and block the relay from turning OFF (duration < minON)
  for (uint16_t i = 0; i < 1000; ++i)
  {
    relay.inc_duration();
  }

  // Relay should still be able to turn OFF
  // If duration wrapped to ~1000, this would still work (1000 > 60)
  // So we need a more precise test...
  const int32_t import = 200;  // 200W import > 100W threshold
  uint16_t overrideBitmask = 0;
  TEST_ASSERT_TRUE(relay.proceed_relay(import, overrideBitmask));
  TEST_ASSERT_FALSE(relay.isRelayON());
}

void test_duration_overflow_wrap_would_block_relay(void)
{
  // This test proves that without saturation, wrapping WOULD cause issues.
  // We increment exactly to where a wrap would put duration below minOFF,
  // then verify the relay still works (proving no wrap occurred).

  const auto& relay = overflowTestRelays.get_relay(0);

  // Relay is OFF from previous test
  TEST_ASSERT_FALSE(relay.isRelayON());

  // Increment to UINT16_MAX
  for (uint32_t i = 0; i < UINT16_MAX; ++i)
  {
    relay.inc_duration();
  }

  // Now increment 50 more times
  // If duration wrapped: (65535 + 50) % 65536 = 49, which is < minOFF (60)
  // If duration saturates: it stays at 65535, which is > minOFF (60)
  for (uint8_t i = 0; i < 50; ++i)
  {
    relay.inc_duration();
  }

  // Try to turn ON the relay
  // If wrapped to 49: would FAIL (49 < 60, blocked by minOFF)
  // If saturated at 65535: should SUCCEED (65535 > 60)
  uint16_t overrideBitmask = 0;
  const int32_t surplus = -600;
  TEST_ASSERT_TRUE(relay.proceed_relay(surplus, overrideBitmask));
  TEST_ASSERT_TRUE(relay.isRelayON());

  // Similarly test turn OFF
  // Increment to UINT16_MAX again
  for (uint32_t i = 0; i < UINT16_MAX; ++i)
  {
    relay.inc_duration();
  }

  // Increment 50 more times (would wrap to 49 if not saturating)
  for (uint8_t i = 0; i < 50; ++i)
  {
    relay.inc_duration();
  }

  // Try to turn OFF
  // If wrapped to 49: would FAIL (49 < 60, blocked by minON)
  // If saturated: should SUCCEED
  const int32_t import = 200;
  overrideBitmask = 0;
  TEST_ASSERT_TRUE(relay.proceed_relay(import, overrideBitmask));
  TEST_ASSERT_FALSE(relay.isRelayON());
}

void test_duration_overflow(void)
{
  RUN_TEST(test_duration_overflow_saturates_at_uint16_max);
  delay(100);
  RUN_TEST(test_duration_overflow_stays_at_max_after_more_increments);
  delay(100);
  RUN_TEST(test_duration_overflow_wrap_would_block_relay);
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

  RUN_TEST(test_proceed_settle_change);

  RUN_TEST(test_relay_ordering);

  RUN_TEST(test_duration_overflow);

  UNITY_END();  // stop unit testing
}
