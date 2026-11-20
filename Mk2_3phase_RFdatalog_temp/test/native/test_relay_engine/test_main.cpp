/**
 * @file test_main.cpp
 * @brief Native unit tests for relay_engine.h: the decisions of the relay diversion
 *
 * @details The real code, no stubs: RelayLogic::Relay on an output that records each switch
 *          (pin, new state) instead of driving a pin, and RelayLogic::Engine on those relays.
 *          As in loop(), the engine gets inc_duration() then proceed_relays() once per second.
 *          The average is negative on a surplus, positive on an import.
 *
 *          Each relay pin differs from its index, so that a test tells an override bit read by
 *          index from one read by pin.
 */

#include <unity.h>

#include "relay_engine.h"  // Real header - pure C++

namespace
{
// ----------------------------------------------------------------------------
// An output that records the switches
// ----------------------------------------------------------------------------
struct Switch
{
  uint8_t pin;
  bool on;
};

Switch switches[16];
uint8_t switchCount{ 0 };

struct Recorder
{
  static void switched(const uint8_t pin, const bool on)
  {
    if (switchCount < sizeof(switches) / sizeof(switches[0]))
    {
      switches[switchCount] = { pin, on };
    }
    ++switchCount;
  }
};

using Relay = RelayLogic::Relay< Recorder >;

// a 1-minute sliding average: 12 datalogs of 5 s, quick to settle
template< uint8_t N >
using Engine = RelayLogic::Engine< Relay, N, 1 >;

constexpr uint16_t NO_OVERRIDE{ 0 };

constexpr uint16_t forced(const uint8_t pin)
{
  return static_cast< uint16_t >(1U << pin);
}

void assertSwitch(const uint8_t i, const uint8_t pin, const bool on)
{
  TEST_ASSERT_TRUE_MESSAGE(i < switchCount, "switch not recorded");
  TEST_ASSERT_EQUAL_UINT8(pin, switches[i].pin);
  TEST_ASSERT_EQUAL(on, switches[i].on);
}

/** A relay that has spent n seconds in its current state. */
void age(const Relay& r, uint32_t n)
{
  while (n--)
  {
    r.inc_duration();
  }
}

/**
 * Brings the sliding average of the engine to @p power, exactly. The average is static, shared
 * by all the engines of the same type: each test sets it.
 */
template< uint8_t N >
void setAverage(const int16_t power)
{
  for (uint16_t i = 0; i < 1000; ++i)
  {
    Engine< N >::update_average(power);
  }
  TEST_ASSERT_EQUAL_INT32(power, Engine< N >::get_average());
}

/** n seconds of loop(): inc_duration() then proceed_relays(). */
template< uint8_t N >
void seconds(const Engine< N >& e, uint16_t n, const uint16_t overrideBitmask = NO_OVERRIDE, const bool diversionEnabled = true)
{
  while (n--)
  {
    e.inc_duration();
    e.proceed_relays(overrideBitmask, diversionEnabled);
  }
}

/** The first minute, without decisions: the settle time at start-up is over. */
template< uint8_t N >
void skipStartUp(const Engine< N >& e)
{
  for (uint8_t i = 0; i < 60; ++i)
  {
    e.inc_duration();
  }
}

/** Each relay ON, one per settle time (60 s), then the settle time of the last one over. */
template< uint8_t N >
void allOn(const Engine< N >& e)
{
  setAverage< N >(-10000);
  skipStartUp(e);
  seconds(e, 60 * N);
  for (uint8_t i = 0; i < 60; ++i)
  {
    e.inc_duration();
  }
  for (uint8_t i = 0; i < N; ++i)
  {
    TEST_ASSERT_TRUE(e.get_relay(i).isRelayON());
  }
  switchCount = 0;
}
}  // namespace

void setUp(void)
{
  switchCount = 0;
}

void tearDown(void) {}

// ============================================================================
// Relay: configuration
// ============================================================================

void test_the_minimum_times_are_given_in_minutes_and_kept_in_seconds(void)
{
  const Relay r{ 4, 500, 100, 2, 3 };

  TEST_ASSERT_EQUAL_UINT16(120, r.get_minON());
  TEST_ASSERT_EQUAL_UINT16(180, r.get_minOFF());
}

void test_a_minimum_time_beyond_1092_minutes_is_marked_for_validation(void)
{
  // 1093 min = 65580 s does not fit in 16 bits: kept as UINT16_MAX, which no number of
  // minutes gives (it is no multiple of 60), so that validation.h can refuse it
  const Relay r{ 5, 1000, 200, 1093, 1092 };
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, r.get_minON());
  TEST_ASSERT_EQUAL_UINT16(65520, r.get_minOFF());
  TEST_ASSERT_TRUE(r.hasValidMinimumTimes() == false);

  const Relay s{ 5, 1000, 200, 1092, 1092 };
  TEST_ASSERT_TRUE(s.hasValidMinimumTimes());
}

void test_the_surplus_threshold_is_positive_whatever_its_sign_in_the_config(void)
{
  const Relay plus{ 4, 500, 100 };
  const Relay minus{ 4, -500, -100 };

  TEST_ASSERT_EQUAL_INT16(500, plus.get_surplusThreshold());
  TEST_ASSERT_EQUAL_INT16(500, minus.get_surplusThreshold());
  // the import threshold keeps its sign: negative is the battery mode
  TEST_ASSERT_EQUAL_INT16(100, plus.get_importThreshold());
  TEST_ASSERT_EQUAL_INT16(-100, minus.get_importThreshold());
}

void test_the_defaults(void)
{
  const Relay r{ 4 };

  TEST_ASSERT_EQUAL_UINT8(4, r.get_pin());
  TEST_ASSERT_EQUAL_INT16(1000, r.get_surplusThreshold());
  TEST_ASSERT_EQUAL_INT16(200, r.get_importThreshold());
  TEST_ASSERT_EQUAL_UINT16(300, r.get_minON());
  TEST_ASSERT_EQUAL_UINT16(300, r.get_minOFF());
  TEST_ASSERT_FALSE(r.isRelayON());
}

// ============================================================================
// Relay: thresholds
// ============================================================================

void test_exactly_the_surplus_threshold_does_not_turn_on(void)
{
  const Relay r{ 5, 1000, 200, 0, 0 };

  TEST_ASSERT_FALSE(r.proceed_relay(-1000, NO_OVERRIDE));
  TEST_ASSERT_FALSE(r.isRelayON());
  TEST_ASSERT_EQUAL_UINT8(0, switchCount);

  TEST_ASSERT_TRUE(r.proceed_relay(-1001, NO_OVERRIDE));
  TEST_ASSERT_TRUE(r.isRelayON());
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  assertSwitch(0, 5, true);
}

void test_exactly_the_import_threshold_does_not_turn_off(void)
{
  const Relay r{ 5, 1000, 200, 0, 0 };
  r.proceed_relay(-2000, NO_OVERRIDE);
  switchCount = 0;

  TEST_ASSERT_FALSE(r.proceed_relay(200, NO_OVERRIDE));
  TEST_ASSERT_TRUE(r.isRelayON());

  TEST_ASSERT_TRUE(r.proceed_relay(201, NO_OVERRIDE));
  TEST_ASSERT_FALSE(r.isRelayON());
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  assertSwitch(0, 5, false);
}

void test_between_the_thresholds_nothing_changes(void)
{
  const Relay off{ 5, 1000, 200, 0, 0 };
  const Relay on{ 6, 1000, 200, 0, 0 };
  on.proceed_relay(-2000, NO_OVERRIDE);
  switchCount = 0;

  const int32_t powers[]{ -1000, -999, -1, 0, 1, 199, 200 };
  for (const auto power : powers)
  {
    TEST_ASSERT_FALSE(off.proceed_relay(power, NO_OVERRIDE));
    TEST_ASSERT_FALSE(on.proceed_relay(power, NO_OVERRIDE));
  }
  TEST_ASSERT_FALSE(off.isRelayON());
  TEST_ASSERT_TRUE(on.isRelayON());
  TEST_ASSERT_EQUAL_UINT8(0, switchCount);
}

void test_a_negative_import_threshold_turns_off_on_a_small_surplus(void)
{
  // battery mode: OFF when the surplus drops below 50 W
  const Relay r{ 5, 1000, -50, 0, 0 };
  r.proceed_relay(-2000, NO_OVERRIDE);

  TEST_ASSERT_FALSE(r.proceed_relay(-50, NO_OVERRIDE));
  TEST_ASSERT_TRUE(r.isRelayON());

  TEST_ASSERT_TRUE(r.proceed_relay(-49, NO_OVERRIDE));
  TEST_ASSERT_FALSE(r.isRelayON());
}

// ============================================================================
// Relay: minimum ON and OFF times
// ============================================================================

void test_a_new_relay_waits_its_minimum_off_time(void)
{
  // at start-up, the relay is OFF since 0 s
  const Relay r{ 5, 1000, 200, 2, 3 };

  age(r, 179);
  TEST_ASSERT_FALSE(r.proceed_relay(-2000, NO_OVERRIDE));
  TEST_ASSERT_EQUAL_UINT8(0, switchCount);

  age(r, 1);
  TEST_ASSERT_TRUE(r.proceed_relay(-2000, NO_OVERRIDE));
  TEST_ASSERT_TRUE(r.isRelayON());
}

void test_the_minimum_on_time_ends_after_exactly_min_on_seconds(void)
{
  const Relay r{ 5, 1000, 200, 2, 3 };
  age(r, 180);
  r.proceed_relay(-2000, NO_OVERRIDE);
  switchCount = 0;

  age(r, 119);
  TEST_ASSERT_FALSE(r.proceed_relay(5000, NO_OVERRIDE));
  TEST_ASSERT_TRUE(r.isRelayON());

  age(r, 1);
  TEST_ASSERT_TRUE(r.proceed_relay(5000, NO_OVERRIDE));
  TEST_ASSERT_FALSE(r.isRelayON());
}

void test_the_minimum_off_time_restarts_at_each_switch_off(void)
{
  const Relay r{ 5, 1000, 200, 2, 3 };
  age(r, 180);
  r.proceed_relay(-2000, NO_OVERRIDE);
  age(r, 120);
  r.proceed_relay(5000, NO_OVERRIDE);
  switchCount = 0;

  age(r, 179);
  TEST_ASSERT_FALSE(r.proceed_relay(-2000, NO_OVERRIDE));

  age(r, 1);
  TEST_ASSERT_TRUE(r.proceed_relay(-2000, NO_OVERRIDE));
}

void test_the_duration_stops_at_its_maximum_instead_of_wrapping(void)
{
  // 100 minutes OFF minimum: 6000 s; a wrapped duration would be back to 100 s
  const Relay r{ 5, 1000, 200, 1, 100 };

  age(r, 65536UL + 100);
  TEST_ASSERT_TRUE(r.proceed_relay(-2000, NO_OVERRIDE));
}

// ============================================================================
// Relay: override
// ============================================================================

void test_a_forced_relay_turns_on_whatever_the_power(void)
{
  const Relay r{ 5, 1000, 200, 0, 0 };

  // the bit of the pin (5), not that of the index (0) or of a neighbour
  TEST_ASSERT_FALSE(r.proceed_relay(5000, forced(0)));
  TEST_ASSERT_FALSE(r.proceed_relay(5000, forced(4) | forced(6)));
  TEST_ASSERT_FALSE(r.isRelayON());

  TEST_ASSERT_TRUE(r.proceed_relay(5000, forced(5)));
  TEST_ASSERT_TRUE(r.isRelayON());
  assertSwitch(0, 5, true);
}

void test_a_forced_relay_is_not_turned_off_by_an_import(void)
{
  const Relay r{ 5, 1000, 200, 0, 0 };
  r.proceed_relay(5000, forced(5));

  TEST_ASSERT_FALSE(r.proceed_relay(5000, forced(5)));
  TEST_ASSERT_TRUE(r.isRelayON());

  // released: the import turns it OFF
  TEST_ASSERT_TRUE(r.proceed_relay(5000, NO_OVERRIDE));
  TEST_ASSERT_FALSE(r.isRelayON());
}

void test_a_forced_relay_waits_its_minimum_off_time(void)
{
  const Relay r{ 5, 1000, 200, 1, 2 };

  age(r, 119);
  TEST_ASSERT_FALSE(r.proceed_relay(0, forced(5)));
  TEST_ASSERT_FALSE(r.proceed_forced_only(forced(5)));

  age(r, 1);
  TEST_ASSERT_TRUE(r.proceed_relay(0, forced(5)));
}

void test_forced_only_turns_an_unforced_relay_off_after_its_minimum_on_time(void)
{
  const Relay r{ 5, 1000, 200, 1, 0 };
  TEST_ASSERT_TRUE(r.proceed_forced_only(forced(5)));
  TEST_ASSERT_FALSE(r.proceed_forced_only(forced(5)));

  // released: OFF, whatever the power, once ON for 60 s
  age(r, 59);
  TEST_ASSERT_FALSE(r.proceed_forced_only(NO_OVERRIDE));
  age(r, 1);
  TEST_ASSERT_TRUE(r.proceed_forced_only(NO_OVERRIDE));
  TEST_ASSERT_FALSE(r.isRelayON());

  TEST_ASSERT_EQUAL_UINT8(2, switchCount);
  assertSwitch(0, 5, true);
  assertSwitch(1, 5, false);
}

// ============================================================================
// Engine: settle time
// ============================================================================

void test_nothing_switches_during_the_first_minute(void)
{
  const Engine< 2 > e{ { { 5, 1000, 200, 0, 0 }, { 6, 1000, 200, 0, 0 } } };
  setAverage< 2 >(-5000);

  seconds(e, 59);
  TEST_ASSERT_EQUAL_UINT8(0, switchCount);

  seconds(e, 1);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  assertSwitch(0, 5, true);
}

void test_a_change_of_any_relay_holds_every_relay_for_a_minute(void)
{
  const Engine< 2 > e{ { { 5, 1000, 200, 0, 0 }, { 6, 1000, 200, 0, 0 } } };
  setAverage< 2 >(-5000);
  skipStartUp(e);

  seconds(e, 1);  // relay #1 ON
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);

  seconds(e, 59);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  TEST_ASSERT_FALSE(e.get_relay(1).isRelayON());

  seconds(e, 1);  // relay #2 ON
  TEST_ASSERT_EQUAL_UINT8(2, switchCount);
  assertSwitch(1, 6, true);
}

void test_the_settle_time_holds_a_switch_off_after_a_switch_on(void)
{
  const Engine< 2 > e{ { { 5, 1000, 200, 0, 0 }, { 6, 3000, 200, 0, 0 } } };
  setAverage< 2 >(-5000);
  skipStartUp(e);
  seconds(e, 1);  // relay #1 ON
  switchCount = 0;

  setAverage< 2 >(5000);
  seconds(e, 59);
  TEST_ASSERT_TRUE(e.get_relay(0).isRelayON());

  seconds(e, 1);
  TEST_ASSERT_FALSE(e.get_relay(0).isRelayON());
}

void test_no_change_starts_no_settle_time(void)
{
  const Engine< 2 > e{ { { 5, 1000, 200, 0, 0 }, { 6, 1000, 200, 0, 0 } } };
  setAverage< 2 >(0);
  skipStartUp(e);

  seconds(e, 10);  // nothing to switch
  seconds(e, 10, NO_OVERRIDE, false);
  TEST_ASSERT_EQUAL_UINT8(0, switchCount);

  setAverage< 2 >(-5000);
  e.proceed_relays(NO_OVERRIDE);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
}

// ============================================================================
// Engine: order
// ============================================================================

void test_on_a_surplus_the_relays_come_in_one_at_a_time_by_increasing_index(void)
{
  const Engine< 3 > e{ { { 7, 500, 100, 0, 0 }, { 5, 500, 100, 0, 0 }, { 6, 500, 100, 0, 0 } } };
  setAverage< 3 >(-5000);
  skipStartUp(e);

  e.proceed_relays(NO_OVERRIDE);
  e.proceed_relays(NO_OVERRIDE);  // held by the settle time
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);

  seconds(e, 120);
  TEST_ASSERT_EQUAL_UINT8(3, switchCount);
  assertSwitch(0, 7, true);
  assertSwitch(1, 5, true);
  assertSwitch(2, 6, true);
}

void test_on_an_import_the_relays_go_out_one_at_a_time_by_decreasing_index(void)
{
  const Engine< 3 > e{ { { 7, 500, 100, 0, 0 }, { 5, 500, 100, 0, 0 }, { 6, 500, 100, 0, 0 } } };
  allOn(e);

  setAverage< 3 >(5000);
  e.proceed_relays(NO_OVERRIDE);
  e.proceed_relays(NO_OVERRIDE);  // held by the settle time
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);

  // the settle time after a switch OFF: 60 s too
  seconds(e, 59);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  seconds(e, 1);
  TEST_ASSERT_EQUAL_UINT8(2, switchCount);

  seconds(e, 60);
  TEST_ASSERT_EQUAL_UINT8(3, switchCount);
  assertSwitch(0, 6, false);
  assertSwitch(1, 5, false);
  assertSwitch(2, 7, false);
}

void test_a_relay_held_by_its_minimum_off_time_lets_the_next_one_in(void)
{
  const Engine< 2 > e{ { { 5, 500, 100, 0, 3 }, { 6, 500, 100, 0, 0 } } };
  setAverage< 2 >(-5000);
  skipStartUp(e);

  e.proceed_relays(NO_OVERRIDE);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  assertSwitch(0, 6, true);
  TEST_ASSERT_FALSE(e.get_relay(0).isRelayON());
}

void test_a_relay_held_by_its_minimum_on_time_lets_the_previous_one_out(void)
{
  const Engine< 2 > e{ { { 5, 500, 100, 0, 0 }, { 6, 500, 100, 2, 0 } } };
  allOn(e);  // relay #2 ON for 119 s, 1 s short of its minimum ON time

  setAverage< 2 >(5000);
  e.proceed_relays(NO_OVERRIDE);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  assertSwitch(0, 5, false);
  TEST_ASSERT_TRUE(e.get_relay(1).isRelayON());
}

void test_each_relay_comes_in_at_its_own_threshold(void)
{
  const Engine< 2 > e{ { { 5, 2000, 100, 0, 0 }, { 6, 500, 100, 0, 0 } } };
  setAverage< 2 >(-1000);
  skipStartUp(e);

  seconds(e, 120);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  assertSwitch(0, 6, true);
}

void test_battery_relays_go_out_by_decreasing_index_on_a_small_surplus(void)
{
  // Battery mode, both ON: on a surplus below 100 W, at 0 W, or on an import, the last relay
  // goes out first, as on an import in normal mode.
  const Engine< 2 > d{ { { 5, 500, -100, 0, 0 }, { 6, 500, -100, 0, 0 } } };
  allOn(d);

  setAverage< 2 >(-50);
  d.proceed_relays(NO_OVERRIDE);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  assertSwitch(0, 6, false);

  const Engine< 2 > e{ { { 5, 500, -100, 0, 0 }, { 6, 500, -100, 0, 0 } } };
  allOn(e);

  setAverage< 2 >(0);
  e.proceed_relays(NO_OVERRIDE);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  assertSwitch(0, 6, false);

  const Engine< 2 > f{ { { 5, 500, -100, 0, 0 }, { 6, 500, -100, 0, 0 } } };
  allOn(f);

  setAverage< 2 >(1);
  f.proceed_relays(NO_OVERRIDE);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  assertSwitch(0, 6, false);
}

// ============================================================================
// Engine: override
// ============================================================================

void test_a_forced_relay_comes_in_during_an_import(void)
{
  const Engine< 3 > e{ { { 5, 500, 100, 0, 0 }, { 6, 500, 100, 0, 0 }, { 7, 500, 100, 0, 0 } } };
  setAverage< 3 >(5000);
  skipStartUp(e);

  e.proceed_relays(forced(6));
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  assertSwitch(0, 6, true);
}

void test_a_forced_relay_stays_on_while_the_others_go_out(void)
{
  const Engine< 3 > e{ { { 5, 500, 100, 0, 0 }, { 6, 500, 100, 0, 0 }, { 7, 500, 100, 0, 0 } } };
  allOn(e);

  setAverage< 3 >(5000);
  seconds(e, 180, forced(6));
  TEST_ASSERT_EQUAL_UINT8(2, switchCount);
  assertSwitch(0, 7, false);
  assertSwitch(1, 5, false);
  TEST_ASSERT_TRUE(e.get_relay(1).isRelayON());
}

void test_a_released_relay_is_left_to_the_engine(void)
{
  const Engine< 2 > e{ { { 5, 500, 100, 0, 0 }, { 6, 500, 100, 0, 0 } } };
  setAverage< 2 >(5000);
  skipStartUp(e);
  seconds(e, 60, forced(6));
  TEST_ASSERT_TRUE(e.get_relay(1).isRelayON());

  // released between the thresholds: stays ON
  setAverage< 2 >(0);
  seconds(e, 120);
  TEST_ASSERT_TRUE(e.get_relay(1).isRelayON());

  // then an import: OFF
  setAverage< 2 >(5000);
  e.proceed_relays(NO_OVERRIDE);
  TEST_ASSERT_FALSE(e.get_relay(1).isRelayON());
}

// ============================================================================
// Engine: diversion disabled
// ============================================================================

void test_diversion_off_turns_every_unforced_relay_off_in_one_go(void)
{
  const Engine< 3 > e{ { { 5, 500, 100, 0, 0 }, { 6, 500, 100, 0, 0 }, { 7, 500, 100, 0, 0 } } };
  allOn(e);

  // even on a surplus
  e.proceed_relays(forced(6), false);
  TEST_ASSERT_EQUAL_UINT8(2, switchCount);
  assertSwitch(0, 7, false);
  assertSwitch(1, 5, false);
  TEST_ASSERT_TRUE(e.get_relay(1).isRelayON());
}

void test_diversion_off_ignores_the_surplus_and_forces_on(void)
{
  const Engine< 3 > e{ { { 5, 500, 100, 0, 0 }, { 6, 500, 100, 0, 0 }, { 7, 500, 100, 0, 0 } } };
  setAverage< 3 >(-5000);
  skipStartUp(e);

  seconds(e, 120, NO_OVERRIDE, false);
  TEST_ASSERT_EQUAL_UINT8(0, switchCount);

  // both forced relays in one go
  e.proceed_relays(forced(5) | forced(7), false);
  TEST_ASSERT_EQUAL_UINT8(2, switchCount);
  assertSwitch(0, 7, true);
  assertSwitch(1, 5, true);
}

void test_diversion_off_keeps_the_minimum_times_and_the_settle_time(void)
{
  const Engine< 2 > e{ { { 5, 500, 100, 0, 0 }, { 6, 500, 100, 2, 0 } } };
  allOn(e);  // relay #2 ON for 119 s, 1 s short of its minimum ON time

  // relay #1 OFF at once, relay #2 held by its minimum ON time
  e.proceed_relays(NO_OVERRIDE, false);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
  assertSwitch(0, 5, false);

  // relay #2 may go OFF 1 s later, but the change of relay #1 holds it for 60 s
  seconds(e, 59, NO_OVERRIDE, false);
  TEST_ASSERT_TRUE(e.get_relay(1).isRelayON());
  seconds(e, 1, NO_OVERRIDE, false);
  TEST_ASSERT_FALSE(e.get_relay(1).isRelayON());

  // and that change holds a forced relay for 60 s
  seconds(e, 59, forced(5), false);
  TEST_ASSERT_FALSE(e.get_relay(0).isRelayON());
  seconds(e, 1, forced(5), false);
  TEST_ASSERT_TRUE(e.get_relay(0).isRelayON());
}

// ============================================================================
// Engine: sliding average
// ============================================================================

void test_the_engine_decides_on_the_average_not_on_the_last_value(void)
{
  const Engine< 1 > e{ { { 5, 1000, 200, 0, 0 } } };
  setAverage< 1 >(0);
  skipStartUp(e);

  // one datalog of twice the surplus threshold: the average stays above -1000 W
  Engine< 1 >::update_average(-2000);
  TEST_ASSERT_TRUE(Engine< 1 >::get_average() > -1000);
  TEST_ASSERT_TRUE(Engine< 1 >::get_average() < 0);
  e.proceed_relays(NO_OVERRIDE);
  TEST_ASSERT_EQUAL_UINT8(0, switchCount);

  // a lasting one: ON
  setAverage< 1 >(-2000);
  e.proceed_relays(NO_OVERRIDE);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
}

void test_the_engine_decides_on_the_triple_ema(void)
{
  const Engine< 1 > e{ { { 5, 1000, 200, 0, 0 } } };
  setAverage< 1 >(0);
  skipStartUp(e);

  // One datalog of -4000 W: the triple EMA (TEMA) takes about 30 % of a step at once,
  // -1188 W; a plain EMA, 1/8 of it, -500 W, would leave the relay OFF.
  Engine< 1 >::update_average(-4000);
  TEST_ASSERT_EQUAL_INT32(-1188, Engine< 1 >::get_average());
  e.proceed_relays(NO_OVERRIDE);
  TEST_ASSERT_EQUAL_UINT8(1, switchCount);
}

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_the_minimum_times_are_given_in_minutes_and_kept_in_seconds);
  RUN_TEST(test_a_minimum_time_beyond_1092_minutes_is_marked_for_validation);
  RUN_TEST(test_the_surplus_threshold_is_positive_whatever_its_sign_in_the_config);
  RUN_TEST(test_the_defaults);

  RUN_TEST(test_exactly_the_surplus_threshold_does_not_turn_on);
  RUN_TEST(test_exactly_the_import_threshold_does_not_turn_off);
  RUN_TEST(test_between_the_thresholds_nothing_changes);
  RUN_TEST(test_a_negative_import_threshold_turns_off_on_a_small_surplus);

  RUN_TEST(test_a_new_relay_waits_its_minimum_off_time);
  RUN_TEST(test_the_minimum_on_time_ends_after_exactly_min_on_seconds);
  RUN_TEST(test_the_minimum_off_time_restarts_at_each_switch_off);
  RUN_TEST(test_the_duration_stops_at_its_maximum_instead_of_wrapping);

  RUN_TEST(test_a_forced_relay_turns_on_whatever_the_power);
  RUN_TEST(test_a_forced_relay_is_not_turned_off_by_an_import);
  RUN_TEST(test_a_forced_relay_waits_its_minimum_off_time);
  RUN_TEST(test_forced_only_turns_an_unforced_relay_off_after_its_minimum_on_time);

  RUN_TEST(test_nothing_switches_during_the_first_minute);
  RUN_TEST(test_a_change_of_any_relay_holds_every_relay_for_a_minute);
  RUN_TEST(test_the_settle_time_holds_a_switch_off_after_a_switch_on);
  RUN_TEST(test_no_change_starts_no_settle_time);

  RUN_TEST(test_on_a_surplus_the_relays_come_in_one_at_a_time_by_increasing_index);
  RUN_TEST(test_on_an_import_the_relays_go_out_one_at_a_time_by_decreasing_index);
  RUN_TEST(test_a_relay_held_by_its_minimum_off_time_lets_the_next_one_in);
  RUN_TEST(test_a_relay_held_by_its_minimum_on_time_lets_the_previous_one_out);
  RUN_TEST(test_each_relay_comes_in_at_its_own_threshold);
  RUN_TEST(test_battery_relays_go_out_by_decreasing_index_on_a_small_surplus);

  RUN_TEST(test_a_forced_relay_comes_in_during_an_import);
  RUN_TEST(test_a_forced_relay_stays_on_while_the_others_go_out);
  RUN_TEST(test_a_released_relay_is_left_to_the_engine);

  RUN_TEST(test_diversion_off_turns_every_unforced_relay_off_in_one_go);
  RUN_TEST(test_diversion_off_ignores_the_surplus_and_forces_on);
  RUN_TEST(test_diversion_off_keeps_the_minimum_times_and_the_settle_time);

  RUN_TEST(test_the_engine_decides_on_the_average_not_on_the_last_value);
  RUN_TEST(test_the_engine_decides_on_the_triple_ema);

  return UNITY_END();
}
