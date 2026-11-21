/**
 * @file test_main.cpp
 * @brief Native unit tests for utils_dualtariff.h: the scheduled boosts of the dual tariff
 *
 * @details Each boost has a window (FIRST, LAST, FROM_START/BEFORE_END with or without FOR,
 *          in HOURS() or MINUTES()) and its targets. The firmware resolves each window at
 *          compile time, in ms from the start of the off-peak period, and forces the targets
 *          while millis() - start is in it. No config.h, no stubs: the real code.
 */

#include <unity.h>

#include "utils_dualtariff.h"
#include "utils_override.h"  // TargetMap  // Real header - pure C++

namespace
{
constexpr uint32_t MINUTE{ 60UL * 1000UL };
constexpr uint32_t HOUR{ 60UL * MINUTE };
}  // namespace

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Scheduled boosts: BoostWindow
// ============================================================================

void test_hours_and_minutes(void)
{
  TEST_ASSERT_EQUAL_UINT16(120, HOURS(2).value);
  TEST_ASSERT_EQUAL_UINT16(720, HOURS(12).value);
  TEST_ASSERT_EQUAL_UINT16(90, MINUTES(90).value);
}

void test_first_starts_at_the_start_and_lasts_its_duration(void)
{
  TEST_ASSERT_EQUAL_UINT32(0, DualTariff::windowStart(FIRST(HOURS(2)), 8));
  TEST_ASSERT_EQUAL_UINT32(7200000UL, DualTariff::windowEnd(FIRST(HOURS(2)), 8));
}

void test_last_starts_back_from_the_end_and_has_no_end(void)
{
  TEST_ASSERT_EQUAL_UINT32(21600000UL, DualTariff::windowStart(LAST(HOURS(2)), 8));
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, DualTariff::windowEnd(LAST(HOURS(2)), 8));
}

void test_from_start_for(void)
{
  constexpr BoostWindow w{ FROM_START(HOURS(2)), FOR(HOURS(2)) };
  TEST_ASSERT_EQUAL_UINT32(7200000UL, DualTariff::windowStart(w, 8));
  TEST_ASSERT_EQUAL_UINT32(14400000UL, DualTariff::windowEnd(w, 8));
}

void test_before_end_for(void)
{
  constexpr BoostWindow w{ BEFORE_END(HOURS(3)), FOR(MINUTES(90)) };
  TEST_ASSERT_EQUAL_UINT32(18000000UL, DualTariff::windowStart(w, 8));
  TEST_ASSERT_EQUAL_UINT32(23400000UL, DualTariff::windowEnd(w, 8));
}

void test_until_the_end_has_no_end(void)
{
  constexpr BoostWindow w{ FROM_START(HOURS(1)), BoostLength{ UNTIL_THE_END } };
  TEST_ASSERT_EQUAL_UINT32(3600000UL, DualTariff::windowStart(w, 8));
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, DualTariff::windowEnd(w, 8));
}

void test_twelve_hours(void)
{
  TEST_ASSERT_EQUAL_UINT32(0, DualTariff::windowStart(LAST(HOURS(12)), 12));
  TEST_ASSERT_EQUAL_UINT32(43140000UL, DualTariff::windowStart(LAST(MINUTES(1)), 12));
  TEST_ASSERT_EQUAL_UINT32(43200000UL, DualTariff::windowEnd(FIRST(HOURS(12)), 12));
}

// 3 h before the end, for 2 h, and 1 h 30 before the end, for 1 h, whatever the period: no
// 16-bit overflow from 10 h on (#214)
void test_before_end_from_3_to_12_hours(void)
{
  for (uint8_t hours = 3; hours <= 12; ++hours)
  {
    const BoostWindow a{ BEFORE_END(HOURS(3)), FOR(HOURS(2)) };
    TEST_ASSERT_EQUAL_UINT32((hours - 3) * HOUR, DualTariff::windowStart(a, hours));
    TEST_ASSERT_EQUAL_UINT32((hours - 1) * HOUR, DualTariff::windowEnd(a, hours));
    TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak(a, hours));
    TEST_ASSERT_TRUE(DualTariff::endsWithinOffPeak(a, hours));

    const BoostWindow b{ BEFORE_END(MINUTES(90)), FOR(MINUTES(60)) };
    TEST_ASSERT_EQUAL_UINT32(hours * HOUR - 90 * MINUTE, DualTariff::windowStart(b, hours));
    TEST_ASSERT_EQUAL_UINT32(hours * HOUR - 30 * MINUTE, DualTariff::windowEnd(b, hours));
  }
}

// FOR(MINUTES(65535)) is a (wrong) length, not 'until the end': it ends after any period
void test_for_the_largest_minutes_is_not_until_the_end(void)
{
  constexpr BoostWindow w{ FROM_START(MINUTES(0)), FOR(MINUTES(65535)) };
  TEST_ASSERT_NOT_EQUAL(UNTIL_THE_END, w.length.minutes);
  TEST_ASSERT_FALSE(DualTariff::endsWithinOffPeak(w, 12));
}

void test_starts_within_off_peak(void)
{
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak(FIRST(HOURS(10)), 8));     // starts within, but ends after: see below
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak(LAST(HOURS(8)), 8));       // starts at 0
  TEST_ASSERT_FALSE(DualTariff::startsWithinOffPeak(LAST(MINUTES(481)), 8));  // 1 min before the start
  TEST_ASSERT_FALSE(DualTariff::startsWithinOffPeak(LAST(MINUTES(0)), 8));    // at the end
  TEST_ASSERT_FALSE(DualTariff::startsWithinOffPeak(BoostWindow{ FROM_START(HOURS(8)), FOR(HOURS(1)) }, 8));
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak(BoostWindow{ FROM_START(MINUTES(479)), FOR(HOURS(1)) }, 8));
}

void test_ends_within_off_peak(void)
{
  TEST_ASSERT_FALSE(DualTariff::endsWithinOffPeak(FIRST(HOURS(10)), 8));
  TEST_ASSERT_TRUE(DualTariff::endsWithinOffPeak(FIRST(HOURS(8)), 8));  // ends at the end
  TEST_ASSERT_FALSE(DualTariff::endsWithinOffPeak(BoostWindow{ BEFORE_END(HOURS(1)), FOR(HOURS(2)) }, 8));
  TEST_ASSERT_TRUE(DualTariff::endsWithinOffPeak(BoostWindow{ BEFORE_END(HOURS(3)), FOR(HOURS(2)) }, 8));
  TEST_ASSERT_TRUE(DualTariff::endsWithinOffPeak(BoostWindow{ FROM_START(HOURS(7)), FOR(HOURS(1)) }, 8));
  TEST_ASSERT_FALSE(DualTariff::endsWithinOffPeak(BoostWindow{ FROM_START(HOURS(7)), FOR(MINUTES(61)) }, 8));
  TEST_ASSERT_TRUE(DualTariff::endsWithinOffPeak(LAST(HOURS(8)), 8));  // until the end
  TEST_ASSERT_TRUE(DualTariff::endsWithinOffPeak(BoostWindow{ FROM_START(HOURS(1)), BoostLength{ UNTIL_THE_END } }, 8));
  TEST_ASSERT_TRUE(DualTariff::endsWithinOffPeak(FIRST(HOURS(12)), 12));
}

void test_a_boost_of_zero_minutes_has_no_length(void)
{
  TEST_ASSERT_FALSE(DualTariff::hasLength(FIRST(MINUTES(0))));
  TEST_ASSERT_FALSE(DualTariff::hasLength(BoostWindow{ BEFORE_END(HOURS(2)), FOR(MINUTES(0)) }));
  TEST_ASSERT_TRUE(DualTariff::hasLength(LAST(HOURS(2))));
}

void test_boost_windows_are_computed_at_compile_time(void)
{
  static_assert(DualTariff::windowEnd(FIRST(HOURS(2)), 8) == 7200000UL, "computed at compile time");
  TEST_PASS();
}

void test_boost_window_includes_its_start_and_excludes_its_end(void)
{
  constexpr BoostWindow w{ FROM_START(HOURS(2)), FOR(HOURS(3)) };  // 2 h to 5 h
  const uint32_t start{ DualTariff::windowStart(w, 8) };
  const uint32_t end{ DualTariff::windowEnd(w, 8) };

  TEST_ASSERT_FALSE(DualTariff::inWindow(2 * HOUR - 1, start, end));
  TEST_ASSERT_TRUE(DualTariff::inWindow(2 * HOUR, start, end));
  TEST_ASSERT_TRUE(DualTariff::inWindow(5 * HOUR - 1, start, end));
  TEST_ASSERT_FALSE(DualTariff::inWindow(5 * HOUR, start, end));
  TEST_ASSERT_TRUE(DualTariff::inWindow(UINT32_MAX - 1, DualTariff::windowStart(LAST(HOURS(2)), 8), DualTariff::windowEnd(LAST(HOURS(2)), 8)));
}

void test_boost_window_across_the_millis_wraparound(void)
{
  const uint32_t offPeakStart{ UINT32_MAX - MINUTE + 1 };              // 1 min before millis() wraps
  const uint32_t start{ DualTariff::windowStart(LAST(HOURS(3)), 8) };  // 5 h
  const uint32_t end{ DualTariff::windowEnd(LAST(HOURS(3)), 8) };

  const uint32_t at6h{ offPeakStart + 6 * HOUR };  // wrapped around
  const uint32_t at4h{ offPeakStart + 4 * HOUR };

  TEST_ASSERT_TRUE(at6h < offPeakStart);
  TEST_ASSERT_TRUE(DualTariff::inWindow(at6h - offPeakStart, start, end));
  TEST_ASSERT_FALSE(DualTariff::inWindow(at4h - offPeakStart, start, end));
}

namespace
{
// pins written raw: LOAD() needs a load map (test_boosts_from_the_settings)
constexpr BoostSchedule boosts{ {
  { FIRST(HOURS(2)), { 5, 7 } },
  { LAST(HOURS(1)), { REMOTE_PIN_BASE + 1 } },
  { FROM_START(HOURS(1)), { 5 } },  // no FOR: until the end
  { BEFORE_END(HOURS(3)), FOR(MINUTES(90)), { 9 } },
} };
static_assert(boosts.size() == 4);
}  // namespace

void test_targets_become_masks(void)
{
  TEST_ASSERT_EQUAL_UINT16((1U << 5) | (1U << 7), boosts.getLocalBitmask(0));
  TEST_ASSERT_EQUAL_UINT8(0, boosts.getRemoteBitmask(0));
  TEST_ASSERT_EQUAL_UINT16(0, boosts.getLocalBitmask(1));
  TEST_ASSERT_EQUAL_UINT8(0b10, boosts.getRemoteBitmask(1));
}

void test_without_for_lasts_until_the_end(void)
{
  TEST_ASSERT_EQUAL_UINT32(3600000UL, DualTariff::resolve(boosts, 2, 8).start);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, DualTariff::resolve(boosts, 2, 8).end);
}

void test_resolve(void)
{
  constexpr auto b{ DualTariff::resolve(boosts, 3, 8) };
  static_assert(b.start == 18000000UL && b.end == 23400000UL && b.local == (1U << 9) && b.remote == 0);
  TEST_ASSERT_EQUAL_UINT32(18000000UL, b.start);
}

void test_a_target_may_be_in_several_boosts(void)
{
  TEST_ASSERT_TRUE(boosts.getLocalBitmask(0) & boosts.getLocalBitmask(2));  // pin 5 twice
}

void test_an_empty_list(void)
{
  constexpr BoostSchedule none{};
  static_assert(none.size() == 0);
  TEST_ASSERT_EQUAL_UINT8(0, none.size());
}

void test_boosts_from_the_settings(void)
{
  // as config_derived.h builds boostSchedule from the scheduledBoosts of config.h
  static constexpr uint8_t map[]{ Load::local(5), Load::remote(1) };
  static constexpr RelaySetting relaySettings[]{ { 8 } };
  static constexpr RelayEngine relays{ relaySettings };
  constexpr TargetMap targets{ map, relays, true };
  constexpr ScheduledBoosts settings{ { { FIRST(HOURS(2)), { LOAD(0), RELAY(0) } },
                                        { LAST(MINUTES(90)), { LOAD(1) } } } };
  constexpr BoostSchedule fromSettings{ settings, targets };

  static_assert(fromSettings.size() == 2);
  TEST_ASSERT_EQUAL_UINT16((1U << 5) | (1U << 8), fromSettings.getLocalBitmask(0));
  TEST_ASSERT_EQUAL_UINT8(0, fromSettings.getRemoteBitmask(0));
  TEST_ASSERT_EQUAL_UINT16(0, fromSettings.getLocalBitmask(1));
  TEST_ASSERT_EQUAL_UINT8(0b1, fromSettings.getRemoteBitmask(1));
  TEST_ASSERT_EQUAL_UINT16(90, fromSettings.getWindow(1).start.minutes);
  TEST_ASSERT_TRUE(fromSettings.getWindow(1).start.fromEnd);
}

void test_no_boost_in_the_settings(void)
{
  static constexpr uint8_t map[]{ Load::local(5) };
  static constexpr RelaySetting relaySettings[]{ { 8 } };
  static constexpr RelayEngine relays{ relaySettings };
  constexpr ScheduledBoosts settings{};
  constexpr BoostSchedule none{ settings, TargetMap{ map, relays, false } };
  static_assert(none.size() == 0);
  TEST_ASSERT_EQUAL_UINT8(0, none.size());
}

void test_a_start_before_the_period_wraps_to_a_large_value(void)
{
  // 1 h period, start 2 h before the end: -60 min, in uint32_t. On the host, this only pins
  // the value: the uint32_t cast in windowStart() matters on AVR, where an int is 16 bits.
  const BoostWindow w{ BEFORE_END(HOURS(2)), FOR(MINUTES(10)) };
  TEST_ASSERT_EQUAL_UINT32(static_cast< uint32_t >(-60) * 60000UL, DualTariff::windowStart(w, 1));
}

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_hours_and_minutes);
  RUN_TEST(test_first_starts_at_the_start_and_lasts_its_duration);
  RUN_TEST(test_last_starts_back_from_the_end_and_has_no_end);
  RUN_TEST(test_from_start_for);
  RUN_TEST(test_before_end_for);
  RUN_TEST(test_until_the_end_has_no_end);
  RUN_TEST(test_twelve_hours);
  RUN_TEST(test_before_end_from_3_to_12_hours);
  RUN_TEST(test_for_the_largest_minutes_is_not_until_the_end);
  RUN_TEST(test_starts_within_off_peak);
  RUN_TEST(test_ends_within_off_peak);
  RUN_TEST(test_a_boost_of_zero_minutes_has_no_length);
  RUN_TEST(test_boost_windows_are_computed_at_compile_time);
  RUN_TEST(test_boost_window_includes_its_start_and_excludes_its_end);
  RUN_TEST(test_boost_window_across_the_millis_wraparound);
  RUN_TEST(test_targets_become_masks);
  RUN_TEST(test_without_for_lasts_until_the_end);
  RUN_TEST(test_resolve);
  RUN_TEST(test_a_target_may_be_in_several_boosts);
  RUN_TEST(test_an_empty_list);
  RUN_TEST(test_boosts_from_the_settings);
  RUN_TEST(test_no_boost_in_the_settings);
  RUN_TEST(test_a_start_before_the_period_wraps_to_a_large_value);

  return UNITY_END();
}
