/**
 * @file test_main.cpp
 * @brief Native unit tests for utils_dualtariff.h: the forcing windows of the dual tariff
 *
 * @details Each load has a pair { offset, duration }: hours from -24 to 24, minutes beyond; a
 *          negative offset counts back from the end of the off-peak period; a duration of
 *          UINT16_MAX lasts until the end. The firmware computes each window at compile time,
 *          in ms from the start of the off-peak period, and forces the load while
 *          millis() - start is in it. No config.h, no stubs: the real code.
 */

#include <unity.h>

#include "utils_dualtariff.h"  // Real header - pure C++

namespace
{
constexpr uint32_t MINUTE{ 60UL * 1000UL };
constexpr uint32_t HOUR{ 60UL * MINUTE };

uint32_t startOf(const pairForceLoad& force, const uint8_t hours)
{
  return DualTariff::windowStart(force, hours);
}

uint32_t endOf(const pairForceLoad& force, const uint8_t hours)
{
  return DualTariff::windowEnd(force, hours);
}

/** Whether the load is forced, `elapsed` ms after the start of the off-peak period. */
bool forcedAt(const pairForceLoad& force, const uint8_t hours, const uint32_t elapsed)
{
  return DualTariff::inWindow(elapsed, startOf(force, hours), endOf(force, hours));
}
}  // namespace

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Windows in hours
// ============================================================================

void test_positive_offset_counts_from_the_start(void)
{
  // { 2, 3 }, 8 h: from 2 h to 5 h after the start
  TEST_ASSERT_EQUAL_UINT32(2 * HOUR, startOf({ 2, 3 }, 8));
  TEST_ASSERT_EQUAL_UINT32(5 * HOUR, endOf({ 2, 3 }, 8));
}

void test_negative_offset_counts_back_from_the_end(void)
{
  // { -3, 2 }, 8 h (config.h): from 5 h to 7 h, that is 3 h before the end, for 2 h
  TEST_ASSERT_EQUAL_UINT32(5 * HOUR, startOf({ -3, 2 }, 8));
  TEST_ASSERT_EQUAL_UINT32(7 * HOUR, endOf({ -3, 2 }, 8));
}

void test_zero_offset_starts_with_the_period(void)
{
  TEST_ASSERT_EQUAL_UINT32(0, startOf({ 0, 1 }, 8));
  TEST_ASSERT_EQUAL_UINT32(1 * HOUR, endOf({ 0, 1 }, 8));
}

// ============================================================================
// Minutes: beyond +-24
// ============================================================================

void test_offset_and_duration_beyond_24_are_minutes(void)
{
  // { 30, 90 }: from 30 min to 2 h
  TEST_ASSERT_EQUAL_UINT32(30 * MINUTE, startOf({ 30, 90 }, 8));
  TEST_ASSERT_EQUAL_UINT32(2 * HOUR, endOf({ 30, 90 }, 8));
}

void test_negative_offset_in_minutes(void)
{
  // { -90, 60 }, 8 h: from 6 h 30 to 7 h 30
  TEST_ASSERT_EQUAL_UINT32(6 * HOUR + 30 * MINUTE, startOf({ -90, 60 }, 8));
  TEST_ASSERT_EQUAL_UINT32(7 * HOUR + 30 * MINUTE, endOf({ -90, 60 }, 8));
}

void test_24_is_hours_25_is_minutes(void)
{
  // the documented trap: a duration of 24 is 24 hours, 25 is 25 minutes
  TEST_ASSERT_EQUAL_UINT32(24 * HOUR, endOf({ 0, 24 }, 8));
  TEST_ASSERT_EQUAL_UINT32(25 * MINUTE, endOf({ 0, 25 }, 8));

  // the same for the offset: 24 is hours, 25 is minutes
  TEST_ASSERT_EQUAL_UINT32(24 * HOUR, startOf({ 24, 1 }, 8));
  TEST_ASSERT_EQUAL_UINT32(25 * MINUTE, startOf({ 25, 1 }, 8));
  TEST_ASSERT_EQUAL_UINT32(12 * HOUR - 25 * MINUTE, startOf({ -25, 1 }, 12));
}

void test_the_units_of_offset_and_duration_are_independent(void)
{
  // offset in minutes, duration in hours, and the other way round
  TEST_ASSERT_EQUAL_UINT32(30 * MINUTE + 2 * HOUR, endOf({ 30, 2 }, 8));
  TEST_ASSERT_EQUAL_UINT32(2 * HOUR + 45 * MINUTE, endOf({ 2, 45 }, 8));
}

// ============================================================================
// Until the end of the period, no boost
// ============================================================================

void test_uint16_max_lasts_until_the_end(void)
{
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, endOf({ -2, UINT16_MAX }, 8));
  TEST_ASSERT_EQUAL_UINT32(6 * HOUR, startOf({ -2, UINT16_MAX }, 8));

  // the one-value constructor: until the end
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, endOf(pairForceLoad{ -2 }, 8));
}

void test_a_load_left_out_of_rg_ForceLoad_is_forced_the_whole_period(void)
{
  // the loads after the last entry of rg_ForceLoad get a default pair: { 0, UINT16_MAX }
  const pairForceLoad leftOut{};

  TEST_ASSERT_EQUAL_UINT32(0, startOf(leftOut, 8));
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, endOf(leftOut, 8));
  TEST_ASSERT_TRUE(forcedAt(leftOut, 8, 0));
  TEST_ASSERT_TRUE(forcedAt(leftOut, 8, 8 * HOUR - 1));
}

void test_zero_zero_is_no_boost(void)
{
  const pairForceLoad none{ 0, 0 };

  TEST_ASSERT_FALSE(forcedAt(none, 8, 0));
  TEST_ASSERT_FALSE(forcedAt(none, 8, 1 * HOUR));
  TEST_ASSERT_FALSE(forcedAt(none, 8, 8 * HOUR - 1));
}

// ============================================================================
// Long off-peak periods: 10 to 12 hours used to overflow (bug 5, #214)
// ============================================================================

void test_negative_offsets_with_10_to_12_hours(void)
{
  // OffPeakDuration * 3600 in 16-bit int overflowed from 10 h (36000 > 32767)
  for (uint8_t hours = 3; hours <= 12; ++hours)
  {
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((hours - 3UL) * HOUR, startOf({ -3, 2 }, hours), "offset in hours");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(hours * HOUR - 90 * MINUTE, startOf({ -90, 60 }, hours), "offset in minutes");
  }
}

void test_the_period_needs_more_than_16_bits_beyond_18_hours(void)
{
  // validation.h stops at 12 hours; the computation itself holds beyond 65535 s
  TEST_ASSERT_EQUAL_UINT32(21 * HOUR, startOf({ -3, 2 }, 24));
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak({ -3, 2 }, 24));
}

void test_12_hours_whole_range(void)
{
  // the longest period: a window from the very start, one up to the very end
  TEST_ASSERT_EQUAL_UINT32(0, startOf({ -12, 1 }, 12));
  TEST_ASSERT_EQUAL_UINT32(1 * HOUR, endOf({ -12, 1 }, 12));
  TEST_ASSERT_EQUAL_UINT32(11 * HOUR, startOf({ -1, 1 }, 12));
  TEST_ASSERT_EQUAL_UINT32(12 * HOUR, endOf({ -1, 1 }, 12));
  TEST_ASSERT_EQUAL_UINT32(12 * HOUR - 25 * MINUTE, startOf({ -25, 25 }, 12));  // the shortest in minutes
  TEST_ASSERT_EQUAL_UINT32(12 * HOUR, endOf({ -25, 25 }, 12));
}

void test_windows_are_computed_at_compile_time(void)
{
  // as in dualtariff.h: rg_OffsetForce is constexpr
  static_assert(DualTariff::windowStart({ -3, 2 }, 12) == 9 * HOUR, "computed at compile time");
  static_assert(DualTariff::windowEnd({ -3, 2 }, 12) == 11 * HOUR, "computed at compile time");
  TEST_PASS();
}

// ============================================================================
// inWindow: start included, end excluded
// ============================================================================

void test_the_window_includes_its_start_and_excludes_its_end(void)
{
  const pairForceLoad force{ 2, 3 };  // 2 h to 5 h

  TEST_ASSERT_FALSE(forcedAt(force, 8, 2 * HOUR - 1));
  TEST_ASSERT_TRUE(forcedAt(force, 8, 2 * HOUR));
  TEST_ASSERT_TRUE(forcedAt(force, 8, 5 * HOUR - 1));
  TEST_ASSERT_FALSE(forcedAt(force, 8, 5 * HOUR));
}

void test_until_the_end_never_ends_by_itself(void)
{
  // the end of the off-peak period stops it: the window itself never closes
  TEST_ASSERT_TRUE(forcedAt({ -2, UINT16_MAX }, 8, UINT32_MAX - 1));
}

void test_elapsed_time_across_the_millis_wraparound(void)
{
  // off-peak starting 1 min before millis() wraps around (after 49.7 days)
  const uint32_t offPeakStart{ UINT32_MAX - MINUTE + 1 };
  const pairForceLoad force{ -3, 2 };  // 5 h to 7 h of 8 h

  const uint32_t at6h{ offPeakStart + 6 * HOUR };  // wrapped around
  const uint32_t at4h{ offPeakStart + 4 * HOUR };

  TEST_ASSERT_TRUE(at6h < offPeakStart);
  TEST_ASSERT_TRUE(forcedAt(force, 8, at6h - offPeakStart));  // as main.cpp: millis() - ul_TimeOffPeak
  TEST_ASSERT_FALSE(forcedAt(force, 8, at4h - offPeakStart));
}

// ============================================================================
// startsWithinOffPeak: windows that would never open (validation.h)
// ============================================================================

void test_a_window_starting_after_the_period_never_opens(void)
{
  // { 10, 20 } with 8 h: starts 2 h after the end
  TEST_ASSERT_FALSE(DualTariff::startsWithinOffPeak({ 10, 20 }, 8));
  // starting at the very end: never opens either
  TEST_ASSERT_FALSE(DualTariff::startsWithinOffPeak({ 8, 1 }, 8));
  TEST_ASSERT_FALSE(DualTariff::startsWithinOffPeak({ 480, 1 }, 8));  // 480 min = 8 h
}

void test_a_window_starting_before_the_period_never_opens(void)
{
  // { -9, 1 } with 8 h: 1 h before the start, wraps around to a huge start
  TEST_ASSERT_FALSE(DualTariff::startsWithinOffPeak({ -9, 1 }, 8));
  TEST_ASSERT_TRUE(startOf({ -9, 1 }, 8) > 8 * HOUR);
  TEST_ASSERT_FALSE(forcedAt({ -9, 1 }, 8, 0));
  TEST_ASSERT_FALSE(forcedAt({ -9, 1 }, 8, 7 * HOUR));
}

void test_windows_within_the_period_are_accepted(void)
{
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak({ -3, 2 }, 8));  // config.h
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak({ 7, 1 }, 8));
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak({ -8, 1 }, 8));  // from the very start
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak({ 479, 1 }, 8));
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak({ -480, 30 }, 8));
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak({ -12, 1 }, 12));
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak(pairForceLoad{}, 8));  // a load left out
}

void test_no_boost_is_accepted_whatever_its_offset(void)
{
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak({ 0, 0 }, 8));
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak({ 10, 0 }, 8));
}

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_positive_offset_counts_from_the_start);
  RUN_TEST(test_negative_offset_counts_back_from_the_end);
  RUN_TEST(test_zero_offset_starts_with_the_period);

  RUN_TEST(test_offset_and_duration_beyond_24_are_minutes);
  RUN_TEST(test_negative_offset_in_minutes);
  RUN_TEST(test_24_is_hours_25_is_minutes);
  RUN_TEST(test_the_units_of_offset_and_duration_are_independent);

  RUN_TEST(test_uint16_max_lasts_until_the_end);
  RUN_TEST(test_a_load_left_out_of_rg_ForceLoad_is_forced_the_whole_period);
  RUN_TEST(test_zero_zero_is_no_boost);

  RUN_TEST(test_negative_offsets_with_10_to_12_hours);
  RUN_TEST(test_the_period_needs_more_than_16_bits_beyond_18_hours);
  RUN_TEST(test_12_hours_whole_range);
  RUN_TEST(test_windows_are_computed_at_compile_time);

  RUN_TEST(test_the_window_includes_its_start_and_excludes_its_end);
  RUN_TEST(test_until_the_end_never_ends_by_itself);
  RUN_TEST(test_elapsed_time_across_the_millis_wraparound);

  RUN_TEST(test_a_window_starting_after_the_period_never_opens);
  RUN_TEST(test_a_window_starting_before_the_period_never_opens);
  RUN_TEST(test_windows_within_the_period_are_accepted);
  RUN_TEST(test_no_boost_is_accepted_whatever_its_offset);

  return UNITY_END();
}
