// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file test_main.cpp
 * @brief Native unit tests for load_priorities.h: the load priority state machine
 *
 * @details The priority array holds one logical load per element, the highest priority first:
 *          the physical load number in the low 7 bits, the ON state in the high bit. Once per
 *          mains cycle, Switching::onNewCycle() may switch one load, depending on the energy
 *          bucket; toPhysical() then gives the physical states. No config.h, no stubs: the real
 *          code, on a bucket of 1000 units.
 */

#include <unity.h>

#include "load_priorities.h"  // Real header - pure C++

namespace
{
constexpr uint8_t ON{ loadStateOnBit };

// a bucket of 1000 units; normal mode: both thresholds at the middle
constexpr LoadPriorities::Bucket bucket{ 1000, 500, 500, 500 };
// anti-flicker mode: thresholds away from the middle
constexpr LoadPriorities::Bucket bucketAF{ 1000, 500, 400, 600 };

constexpr uint8_t MAX_COUNT{ LoadPriorities::Switching< 3 >::POST_TRANSITION_MAX_COUNT };

bool isOn(const uint8_t entry)
{
  return (entry & ON) != 0;
}

/** Runs cycles at the same energy level. */
template< uint8_t N >
void cycles(LoadPriorities::Switching< N >& s, uint8_t (&prio)[N], const int32_t energy, uint8_t n,
            const LoadPriorities::Bucket& b = bucket)
{
  while (n--)
  {
    s.onNewCycle(prio, energy, b);
  }
}

/**
 * A state machine with both thresholds at their defaults, as after cycles in both halves of
 * the bucket. A new one starts with both at 0, as in the firmware: nothing would switch off
 * before the bucket has been in its upper half once.
 */
LoadPriorities::Switching< 3 > settled(const LoadPriorities::Bucket& b = bucket)
{
  LoadPriorities::Switching< 3 > s;
  s.lowerThreshold = b.lowerDefault;
  s.upperThreshold = b.upperDefault;
  return s;
}

bool noOverride(uint8_t /* iLoad */)
{
  return false;
}
}  // namespace

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// nextToAdd / nextToRemove
// ============================================================================

void test_next_to_add_is_the_first_load_off(void)
{
  uint8_t allOff[3]{ 0, 1, 2 };
  uint8_t firstOn[3]{ 0 | ON, 1, 2 };
  uint8_t gap[3]{ 0 | ON, 1, 2 | ON };

  TEST_ASSERT_EQUAL_UINT8(0, LoadPriorities::nextToAdd(allOff));
  TEST_ASSERT_EQUAL_UINT8(1, LoadPriorities::nextToAdd(firstOn));
  TEST_ASSERT_EQUAL_UINT8(1, LoadPriorities::nextToAdd(gap));
}

void test_next_to_add_none_when_all_on(void)
{
  uint8_t allOn[3]{ 0 | ON, 1 | ON, 2 | ON };

  TEST_ASSERT_EQUAL_UINT8(3, LoadPriorities::nextToAdd(allOn));
}

void test_next_to_remove_is_the_last_load_on(void)
{
  uint8_t allOn[3]{ 0 | ON, 1 | ON, 2 | ON };
  uint8_t twoOn[3]{ 0 | ON, 1 | ON, 2 };
  uint8_t gap[3]{ 0 | ON, 1, 2 | ON };

  TEST_ASSERT_EQUAL_UINT8(2, LoadPriorities::nextToRemove(allOn));
  TEST_ASSERT_EQUAL_UINT8(1, LoadPriorities::nextToRemove(twoOn));
  TEST_ASSERT_EQUAL_UINT8(2, LoadPriorities::nextToRemove(gap));
}

void test_next_to_remove_none_when_all_off(void)
{
  uint8_t allOff[3]{ 0, 1, 2 };

  TEST_ASSERT_EQUAL_UINT8(3, LoadPriorities::nextToRemove(allOff));
}

void test_next_ignores_the_load_numbers(void)
{
  // positions in priority order, whatever the physical load numbers
  uint8_t prio[3]{ 2 | ON, 0, 1 };

  TEST_ASSERT_EQUAL_UINT8(1, LoadPriorities::nextToAdd(prio));
  TEST_ASSERT_EQUAL_UINT8(0, LoadPriorities::nextToRemove(prio));
}

// ============================================================================
// Switching: one load at a time, in priority order
// ============================================================================

void test_high_energy_switches_the_first_load_on(void)
{
  LoadPriorities::Switching< 3 > s;
  uint8_t prio[3]{ 0, 1, 2 };

  s.onNewCycle(prio, 800, bucket);

  TEST_ASSERT_TRUE(isOn(prio[0]));
  TEST_ASSERT_FALSE(isOn(prio[1]));
  TEST_ASSERT_FALSE(isOn(prio[2]));
  TEST_ASSERT_EQUAL_UINT8(0, s.activeLoad);
  TEST_ASSERT_TRUE(s.recentTransition);
}

void test_switching_on_follows_the_priorities_not_the_load_numbers(void)
{
  LoadPriorities::Switching< 3 > s;
  uint8_t prio[3]{ 2, 0, 1 };  // load 2 first

  s.onNewCycle(prio, 800, bucket);

  TEST_ASSERT_EQUAL_HEX8(2 | ON, prio[0]);
  TEST_ASSERT_EQUAL_HEX8(0, prio[1]);
  TEST_ASSERT_EQUAL_HEX8(1, prio[2]);
}

void test_the_next_load_waits_for_the_end_of_the_post_transition_period(void)
{
  LoadPriorities::Switching< 3 > s;
  uint8_t prio[3]{ 0, 1, 2 };

  // the bucket keeps rising: there is still surplus once the first load is on
  int32_t energy{ 800 };
  s.onNewCycle(prio, energy, bucket);
  for (uint8_t cycle = 1; cycle != MAX_COUNT; ++cycle)
  {
    s.onNewCycle(prio, energy += 10, bucket);
    TEST_ASSERT_FALSE_MESSAGE(isOn(prio[1]), "second load switched during the post-transition period");
  }

  s.onNewCycle(prio, energy += 10, bucket);

  TEST_ASSERT_TRUE(isOn(prio[1]));
  TEST_ASSERT_FALSE(isOn(prio[2]));
  TEST_ASSERT_EQUAL_UINT8(1, s.activeLoad);
}

void test_the_upper_threshold_follows_the_bucket_during_the_post_transition_period(void)
{
  LoadPriorities::Switching< 3 > s;
  uint8_t prio[3]{ 0, 1, 2 };

  s.onNewCycle(prio, 800, bucket);
  s.onNewCycle(prio, 850, bucket);

  TEST_ASSERT_EQUAL_INT32(850, s.upperThreshold);

  // once the period is over, a bucket that stays at that level switches nothing more
  cycles(s, prio, 850, 5);

  TEST_ASSERT_FALSE(isOn(prio[1]));
}

void test_the_upper_threshold_stays_within_the_bucket(void)
{
  LoadPriorities::Switching< 3 > s;
  uint8_t prio[3]{ 0, 1, 2 };

  s.onNewCycle(prio, 800, bucket);
  // a bucket above its capacity: onNewCycle() clamps the threshold it takes from it
  // (the ISR clamps the bucket itself, afterwards)
  s.onNewCycle(prio, 1200, bucket);

  TEST_ASSERT_EQUAL_INT32(1000, s.upperThreshold);
}

void test_low_energy_switches_the_last_load_off(void)
{
  auto s{ settled() };
  uint8_t prio[3]{ 0 | ON, 1 | ON, 2 | ON };

  s.onNewCycle(prio, 200, bucket);

  TEST_ASSERT_TRUE(isOn(prio[0]));
  TEST_ASSERT_TRUE(isOn(prio[1]));
  TEST_ASSERT_FALSE(isOn(prio[2]));
  TEST_ASSERT_EQUAL_UINT8(2, s.activeLoad);
}

void test_the_previous_load_waits_for_the_end_of_the_post_transition_period(void)
{
  auto s{ settled() };
  uint8_t prio[3]{ 0 | ON, 1 | ON, 2 | ON };

  // the bucket keeps falling: still importing once the last load is off
  int32_t energy{ 200 };
  s.onNewCycle(prio, energy, bucket);
  for (uint8_t cycle = 1; cycle != MAX_COUNT; ++cycle)
  {
    s.onNewCycle(prio, energy -= 10, bucket);
    TEST_ASSERT_TRUE_MESSAGE(isOn(prio[1]), "second load switched off during the post-transition period");
  }

  s.onNewCycle(prio, energy -= 10, bucket);

  TEST_ASSERT_FALSE(isOn(prio[1]));
  TEST_ASSERT_TRUE(isOn(prio[0]));
}

void test_the_lower_threshold_stays_within_the_bucket(void)
{
  auto s{ settled() };
  uint8_t prio[3]{ 0 | ON, 1 | ON, 2 | ON };

  s.onNewCycle(prio, 200, bucket);
  // a bucket below 0: onNewCycle() clamps the threshold it takes from it
  // (the ISR clamps the bucket itself, afterwards)
  s.onNewCycle(prio, -300, bucket);

  TEST_ASSERT_EQUAL_INT32(0, s.lowerThreshold);
}

void test_the_load_switched_last_may_switch_back_at_once(void)
{
  LoadPriorities::Switching< 3 > s;
  uint8_t prio[3]{ 0 | ON, 1, 2 };

  s.onNewCycle(prio, 800, bucket);  // load 1 on
  TEST_ASSERT_TRUE(isOn(prio[1]));

  s.onNewCycle(prio, 200, bucket);  // the next cycle, the bucket falls: load 1 off again

  TEST_ASSERT_FALSE(isOn(prio[1]));
  TEST_ASSERT_TRUE(isOn(prio[0]));
}

void test_the_middle_of_the_bucket_counts_as_the_lower_half(void)
{
  LoadPriorities::Switching< 3 > s;
  uint8_t prio[3]{ 0, 1, 2 };

  s.onNewCycle(prio, 500, bucket);

  TEST_ASSERT_FALSE(isOn(prio[0]));
  TEST_ASSERT_EQUAL_INT32(500, s.upperThreshold);  // the "opposite" threshold, reset
}

void test_each_half_resets_the_opposite_threshold(void)
{
  LoadPriorities::Switching< 3 > s;
  uint8_t prio[3]{ 0 | ON, 1, 2 };

  s.onNewCycle(prio, 800, bucket);  // load 1 on
  s.onNewCycle(prio, 900, bucket);  // upper threshold follows: 900
  TEST_ASSERT_EQUAL_INT32(900, s.upperThreshold);

  s.onNewCycle(prio, 450, bucket);  // lower half: upper threshold back to its default

  TEST_ASSERT_EQUAL_INT32(500, s.upperThreshold);

  s.onNewCycle(prio, 600, bucket);  // upper half: lower threshold back to its default

  TEST_ASSERT_EQUAL_INT32(500, s.lowerThreshold);
}

void test_anti_flicker_thresholds_leave_a_dead_band(void)
{
  LoadPriorities::Switching< 3 > s;
  uint8_t prio[3]{ 0 | ON, 1, 2 };

  // between the thresholds (400..600), nothing switches, on either side of the middle
  cycles(s, prio, 450, 5, bucketAF);
  cycles(s, prio, 550, 5, bucketAF);
  TEST_ASSERT_EQUAL_HEX8(0 | ON, prio[0]);
  TEST_ASSERT_EQUAL_HEX8(1, prio[1]);

  s.onNewCycle(prio, 601, bucketAF);
  TEST_ASSERT_TRUE(isOn(prio[1]));
}

void test_nothing_to_switch_starts_no_post_transition_period(void)
{
  LoadPriorities::Switching< 3 > s;
  uint8_t allOn[3]{ 0 | ON, 1 | ON, 2 | ON };
  uint8_t allOff[3]{ 0, 1, 2 };

  cycles(s, allOn, 900, MAX_COUNT);
  TEST_ASSERT_FALSE(s.recentTransition);
  TEST_ASSERT_EQUAL_UINT8(3, s.activeLoad);

  cycles(s, allOff, 100, MAX_COUNT);
  TEST_ASSERT_FALSE(s.recentTransition);
  TEST_ASSERT_EQUAL_HEX8(0, allOff[0]);
}

void test_a_surplus_brings_every_load_on_then_its_end_every_load_off(void)
{
  LoadPriorities::Switching< 3 > s;
  uint8_t prio[3]{ 0, 1, 2 };

  int32_t energy{ 600 };
  for (uint8_t cycle = 0; cycle != 20; ++cycle)
  {
    s.onNewCycle(prio, energy += 10, bucket);
  }
  TEST_ASSERT_EQUAL_UINT8(3, LoadPriorities::nextToAdd(prio));

  energy = 400;
  for (uint8_t cycle = 0; cycle != 20; ++cycle)
  {
    s.onNewCycle(prio, energy -= 10, bucket);
  }
  TEST_ASSERT_EQUAL_UINT8(3, LoadPriorities::nextToRemove(prio));
}

// ============================================================================
// rotate
// ============================================================================

void test_rotate_moves_the_last_load_first(void)
{
  uint8_t prio[3]{ 0, 1, 2 };

  LoadPriorities::rotate(prio);

  TEST_ASSERT_EQUAL_HEX8(2, prio[0]);
  TEST_ASSERT_EQUAL_HEX8(0, prio[1]);
  TEST_ASSERT_EQUAL_HEX8(1, prio[2]);
}

void test_rotate_keeps_each_load_state(void)
{
  uint8_t prio[3]{ 0 | ON, 1, 2 | ON };

  LoadPriorities::rotate(prio);

  TEST_ASSERT_EQUAL_HEX8(2 | ON, prio[0]);
  TEST_ASSERT_EQUAL_HEX8(0 | ON, prio[1]);
  TEST_ASSERT_EQUAL_HEX8(1, prio[2]);
}

void test_rotate_n_times_is_back_to_the_start(void)
{
  uint8_t prio[4]{ 3, 1, 0, 2 };

  for (uint8_t i = 0; i != 4; ++i)
  {
    LoadPriorities::rotate(prio);
  }

  TEST_ASSERT_EQUAL_HEX8(3, prio[0]);
  TEST_ASSERT_EQUAL_HEX8(1, prio[1]);
  TEST_ASSERT_EQUAL_HEX8(0, prio[2]);
  TEST_ASSERT_EQUAL_HEX8(2, prio[3]);
}

void test_rotate_a_single_load_changes_nothing(void)
{
  // guards around the array: rotate() must write nothing outside it
  struct
  {
    uint8_t before{ 0xAA };
    uint8_t prio[1]{ 0 | ON };
    uint8_t after[4]{ 0x55, 0x55, 0x55, 0x55 };
  } mem;

  LoadPriorities::rotate(mem.prio);

  TEST_ASSERT_EQUAL_HEX8(0 | ON, mem.prio[0]);
  TEST_ASSERT_EQUAL_HEX8(0xAA, mem.before);
  TEST_ASSERT_EQUAL_HEX8(0x55, mem.after[0]);
  TEST_ASSERT_EQUAL_HEX8(0x55, mem.after[3]);
}

// ============================================================================
// toPhysical
// ============================================================================

void test_physical_states_follow_the_load_numbers(void)
{
  const uint8_t prio[3]{ 2 | ON, 0, 1 | ON };
  LoadStates physical[3]{};

  LoadPriorities::toPhysical< false >(prio, physical, true, noOverride);

  TEST_ASSERT_TRUE(LoadStates::LOAD_OFF == physical[0]);
  TEST_ASSERT_TRUE(LoadStates::LOAD_ON == physical[1]);
  TEST_ASSERT_TRUE(LoadStates::LOAD_ON == physical[2]);
}

void test_diversion_off_switches_every_load_off(void)
{
  const uint8_t prio[3]{ 0 | ON, 1 | ON, 2 | ON };
  LoadStates physical[3]{ LoadStates::LOAD_ON, LoadStates::LOAD_ON, LoadStates::LOAD_ON };

  LoadPriorities::toPhysical< false >(prio, physical, false, noOverride);

  TEST_ASSERT_TRUE(LoadStates::LOAD_OFF == physical[0]);
  TEST_ASSERT_TRUE(LoadStates::LOAD_OFF == physical[1]);
  TEST_ASSERT_TRUE(LoadStates::LOAD_OFF == physical[2]);
}

void test_an_override_forces_its_load_on(void)
{
  const uint8_t prio[3]{ 0, 1, 2 };
  LoadStates physical[3]{};
  const auto overrideLoad1 = [](const uint8_t iLoad)
  {
    return iLoad == 1;
  };

  // diversion on or off: the forced load is on, the others follow their logical state
  LoadPriorities::toPhysical< false >(prio, physical, true, overrideLoad1);
  TEST_ASSERT_TRUE(LoadStates::LOAD_OFF == physical[0]);
  TEST_ASSERT_TRUE(LoadStates::LOAD_ON == physical[1]);

  LoadPriorities::toPhysical< false >(prio, physical, false, overrideLoad1);
  TEST_ASSERT_TRUE(LoadStates::LOAD_ON == physical[1]);
  TEST_ASSERT_TRUE(LoadStates::LOAD_OFF == physical[2]);
}

void test_the_override_is_asked_for_the_physical_load(void)
{
  const uint8_t prio[3]{ 2, 0, 1 };
  LoadStates physical[3]{};
  // the override of load 2, the first in priority order
  const auto overrideLoad2 = [](const uint8_t iLoad)
  {
    return iLoad == 2;
  };

  LoadPriorities::toPhysical< false >(prio, physical, true, overrideLoad2);

  TEST_ASSERT_TRUE(LoadStates::LOAD_ON == physical[2]);
  TEST_ASSERT_TRUE(LoadStates::LOAD_OFF == physical[0]);
  TEST_ASSERT_TRUE(LoadStates::LOAD_OFF == physical[1]);
}

void test_calibration_mode_keeps_every_load_off(void)
{
  const uint8_t prio[3]{ 0 | ON, 1 | ON, 2 };
  LoadStates physical[3]{ LoadStates::LOAD_ON, LoadStates::LOAD_ON, LoadStates::LOAD_ON };
  const auto overrideAll = [](uint8_t /* iLoad */)
  {
    return true;
  };

  LoadPriorities::toPhysical< true >(prio, physical, true, overrideAll);

  TEST_ASSERT_TRUE(LoadStates::LOAD_OFF == physical[0]);
  TEST_ASSERT_TRUE(LoadStates::LOAD_OFF == physical[1]);
  TEST_ASSERT_TRUE(LoadStates::LOAD_OFF == physical[2]);
}

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_next_to_add_is_the_first_load_off);
  RUN_TEST(test_next_to_add_none_when_all_on);
  RUN_TEST(test_next_to_remove_is_the_last_load_on);
  RUN_TEST(test_next_to_remove_none_when_all_off);
  RUN_TEST(test_next_ignores_the_load_numbers);

  RUN_TEST(test_high_energy_switches_the_first_load_on);
  RUN_TEST(test_switching_on_follows_the_priorities_not_the_load_numbers);
  RUN_TEST(test_the_next_load_waits_for_the_end_of_the_post_transition_period);
  RUN_TEST(test_the_upper_threshold_follows_the_bucket_during_the_post_transition_period);
  RUN_TEST(test_the_upper_threshold_stays_within_the_bucket);
  RUN_TEST(test_low_energy_switches_the_last_load_off);
  RUN_TEST(test_the_previous_load_waits_for_the_end_of_the_post_transition_period);
  RUN_TEST(test_the_lower_threshold_stays_within_the_bucket);
  RUN_TEST(test_the_load_switched_last_may_switch_back_at_once);
  RUN_TEST(test_the_middle_of_the_bucket_counts_as_the_lower_half);
  RUN_TEST(test_each_half_resets_the_opposite_threshold);
  RUN_TEST(test_anti_flicker_thresholds_leave_a_dead_band);
  RUN_TEST(test_nothing_to_switch_starts_no_post_transition_period);
  RUN_TEST(test_a_surplus_brings_every_load_on_then_its_end_every_load_off);

  RUN_TEST(test_rotate_moves_the_last_load_first);
  RUN_TEST(test_rotate_keeps_each_load_state);
  RUN_TEST(test_rotate_n_times_is_back_to_the_start);
  RUN_TEST(test_rotate_a_single_load_changes_nothing);

  RUN_TEST(test_physical_states_follow_the_load_numbers);
  RUN_TEST(test_diversion_off_switches_every_load_off);
  RUN_TEST(test_an_override_forces_its_load_on);
  RUN_TEST(test_the_override_is_asked_for_the_physical_load);
  RUN_TEST(test_calibration_mode_keeps_every_load_off);

  return UNITY_END();
}
