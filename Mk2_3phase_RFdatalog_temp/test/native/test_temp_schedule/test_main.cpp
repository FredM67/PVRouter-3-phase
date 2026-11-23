// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file test_main.cpp
 * @brief Native unit tests for temp_schedule.h: which 1-Wire transaction runs on which mains cycle
 *
 * @details The main loop asks the schedule once per mains cycle, when it sees the new cycle, and
 *          runs the transaction it gets: the conversion of every sensor, then, once the conversion
 *          time has passed, the three transactions of each sensor's read. One transaction per
 *          cycle at most, only on a cycle seen on time (the loop idle when it began), and the
 *          round over well before the end of the datalog period, whose values it gives.
 */

#include <unity.h>

#include <stdint.h>

#include "temp_schedule.h"  // Real header - pure C++

using TempSchedule::Action;
using TempSchedule::Step;

void setUp(void) {}
void tearDown(void) {}

namespace
{
constexpr uint16_t C50{ TempSchedule::conversionCycles(50) };
constexpr uint16_t C60{ TempSchedule::conversionCycles(60) };

/** The transaction expected after the conversion, k-th from 0: three per sensor. */
Action readStep(uint16_t k)
{
  constexpr Step kinds[]{ Step::ADDRESS, Step::READ, Step::CLOSE };
  return Action{ kinds[k % 3], static_cast< uint8_t >(k / 3) };
}

void assertAction(const Action expected, const Action actual, const char* msg)
{
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(static_cast< uint8_t >(expected.step), static_cast< uint8_t >(actual.step), msg);
  if (expected.step != Step::NONE)
  {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected.sensor, actual.sensor, msg);
  }
}
}  // namespace

// ============================================================================
// Conversion time
// ============================================================================

void test_conversion_cycles_cover_750_ms()
{
  // 750 ms rounded up to whole cycles, plus one: the loop may start a transaction up to a little
  // after its cycle began
  TEST_ASSERT_EQUAL_UINT16(39, C50);  // 780 ms
  TEST_ASSERT_EQUAL_UINT16(46, C60);  // 767 ms
  TEST_ASSERT_TRUE(C50 * 1000UL / 50 >= 760);
  TEST_ASSERT_TRUE(C60 * 1000UL / 60 >= 760);
}

// ============================================================================
// The round
// ============================================================================

void test_round_of_two_sensors()
{
  TempSchedule::Scheduler< 2, C50 > schedule;

  assertAction({ Step::CONVERT, 0 }, schedule.next(true), "first cycle: the conversion");
  for (uint16_t i = 1; i != C50; ++i)
  {
    assertAction({ Step::NONE, 0 }, schedule.next(true), "during the conversion: nothing");
  }
  for (uint16_t k = 0; k != 6; ++k)
  {
    assertAction(readStep(k), schedule.next(true), "the reads, one transaction per cycle");
  }
  TEST_ASSERT_TRUE(schedule.done());
  for (uint16_t i = 0; i != 300; ++i)
  {
    assertAction({ Step::NONE, 0 }, schedule.next(true), "round over: nothing until the next period");
  }
}

void test_first_read_conversion_cycles_after_the_conversion()
{
  TempSchedule::Scheduler< 1, C60 > schedule;
  uint16_t cycle{ 0 }, convertAt{ 0 }, addressAt{ 0 };
  while (!schedule.done() && cycle < 1000)
  {
    const Action a{ schedule.next(true) };
    if (a.step == Step::CONVERT)
    {
      convertAt = cycle;
    }
    if (a.step == Step::ADDRESS)
    {
      addressAt = cycle;
    }
    ++cycle;
  }
  TEST_ASSERT_EQUAL_UINT16(C60, addressAt - convertAt);
}

void test_late_cycles_postpone_without_losing_a_step()
{
  TempSchedule::Scheduler< 1, C50 > schedule;

  assertAction({ Step::NONE, 0 }, schedule.next(false), "late: no conversion");
  assertAction({ Step::CONVERT, 0 }, schedule.next(true), "on time: the conversion");
  for (uint16_t i = 1; i != C50; ++i)
  {
    schedule.next(true);
  }
  assertAction({ Step::NONE, 0 }, schedule.next(false), "late: the read waits");
  assertAction({ Step::ADDRESS, 0 }, schedule.next(true), "");
  assertAction({ Step::NONE, 0 }, schedule.next(false), "late: the read waits");
  assertAction({ Step::READ, 0 }, schedule.next(true), "");
  assertAction({ Step::CLOSE, 0 }, schedule.next(true), "");
  TEST_ASSERT_TRUE(schedule.done());
}

void test_late_cycles_still_count_for_the_conversion()
{
  // the conversion runs on the sensors' side whatever the loop does
  TempSchedule::Scheduler< 1, C50 > schedule;
  schedule.next(true);
  for (uint16_t i = 1; i != C50; ++i)
  {
    schedule.next(false);
  }
  assertAction({ Step::ADDRESS, 0 }, schedule.next(true), "conversion time over");
}

void test_a_sensor_that_does_not_answer_is_skipped()
{
  TempSchedule::Scheduler< 3, C50 > schedule;
  schedule.next(true);
  for (uint16_t i = 1; i != C50; ++i)
  {
    schedule.next(true);
  }
  assertAction({ Step::ADDRESS, 0 }, schedule.next(true), "");
  schedule.skipRestOf(0);  // no presence pulse
  assertAction({ Step::ADDRESS, 1 }, schedule.next(true), "the next sensor, on the next cycle");
  assertAction({ Step::READ, 1 }, schedule.next(true), "");
  schedule.skipRestOf(0);  // another sensor's: ignored
  assertAction({ Step::CLOSE, 1 }, schedule.next(true), "");
  assertAction({ Step::ADDRESS, 2 }, schedule.next(true), "");
  schedule.skipRestOf(2);
  TEST_ASSERT_TRUE(schedule.done());
  assertAction({ Step::NONE, 0 }, schedule.next(true), "the last sensor skipped: round over");
}

void test_new_period_restarts_a_finished_round()
{
  TempSchedule::Scheduler< 1, C50 > schedule;
  for (uint16_t i = 0; i != C50 + 3; ++i)
  {
    schedule.next(true);
  }
  TEST_ASSERT_TRUE(schedule.done());
  TEST_ASSERT_TRUE(schedule.newPeriod());  // restarted: the loop may convert at once
  TEST_ASSERT_FALSE(schedule.done());
  assertAction({ Step::CONVERT, 0 }, schedule.next(true), "a new round");
}

void test_conversion_in_the_cycle_that_ends_the_period()
{
  // The loop starts the next round as soon as it handles the end of the period, in the same mains
  // cycle, if it is on time: the round's cycle had no transaction (round over), so that cycle
  // still has one at most, and the bus is quiet again before the datalog output starts.
  TempSchedule::Scheduler< 1, C50 > schedule;
  for (uint16_t i = 0; i != C50 + 3; ++i)
  {
    schedule.next(true);
  }
  assertAction({ Step::NONE, 0 }, schedule.next(true), "the cycle that ends the period: nothing");
  TEST_ASSERT_TRUE(schedule.newPeriod());
  assertAction({ Step::CONVERT, 0 }, schedule.next(true), "the conversion, in that same cycle");
  for (uint16_t i = 1; i != C50; ++i)
  {
    assertAction({ Step::NONE, 0 }, schedule.next(true), "during the conversion: nothing");
  }
  assertAction({ Step::ADDRESS, 0 }, schedule.next(true), "");
}

void test_new_period_continues_an_unfinished_round()
{
  // a round longer than the period (only with many late cycles): the values of the sensors not
  // read yet are those of the period before; the conversion is never cut short
  TempSchedule::Scheduler< 2, C50 > schedule;
  for (uint16_t i = 0; i != C50 + 1; ++i)
  {
    schedule.next(true);  // the conversion, the wait, ADDRESS of sensor 0
  }
  TEST_ASSERT_FALSE(schedule.newPeriod());  // not restarted: no extra transaction in this cycle
  assertAction({ Step::READ, 0 }, schedule.next(true), "the round goes on");
}

void test_never_two_transactions_in_one_cycle_and_never_before_the_conversion_time()
{
  // a pseudo-random pattern of late cycles, over many periods
  TempSchedule::Scheduler< 4, C50 > schedule;
  uint32_t lfsr{ 0xACE1U };
  int32_t lastConvert{ -100000 };
  uint16_t rounds{ 0 };
  for (int32_t cycle = 0; cycle != 20000; ++cycle)
  {
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1U) & 0xB400U);
    const bool onTime{ (lfsr & 7U) != 0 };  // 1 cycle in 8 late
    if (cycle % 250 == 0)
    {
      schedule.newPeriod();
    }
    const Action a{ schedule.next(onTime) };  // one call per cycle: one transaction at most
    if (a.step != Step::NONE)
    {
      TEST_ASSERT_TRUE_MESSAGE(onTime, "a transaction on a late cycle");
    }
    if (a.step == Step::CONVERT)
    {
      lastConvert = cycle;
      ++rounds;
    }
    if (a.step == Step::ADDRESS || a.step == Step::READ)
    {
      TEST_ASSERT_TRUE_MESSAGE(cycle - lastConvert >= C50, "a read before the end of the conversion");
    }
  }
  TEST_ASSERT_EQUAL_UINT16(80, rounds);  // one round per period
}

// ============================================================================
// Fitting the period
// ============================================================================

/**
 * @brief A datalog period of the main loop: the round must be over before the cycle that ends the
 *        period, whose ISR copies run right after the new cycle is flagged.
 *
 * @details The cycle after each per-second tick is late (the per-second work and the display may
 *          run past the next tick).
 */
template< uint8_t N, uint8_t F >
static bool roundEndsInPeriod(const uint16_t periodCycles)
{
  constexpr uint16_t C{ TempSchedule::conversionCycles(F) };
  TempSchedule::Scheduler< N, C > schedule;
  schedule.newPeriod();
  for (uint16_t tick = 1; tick != periodCycles; ++tick)  // tick periodCycles: the period-end cycle
  {
    const bool onTime{ (tick % F) != 1 };
    schedule.next(onTime);
    if (schedule.done())
    {
      return true;
    }
  }
  return false;
}

void test_rounds_fit_their_period()
{
  // 5 s, the default
  static_assert(TempSchedule::fitsInPeriod(2, 50, 250), "");
  static_assert(TempSchedule::fitsInPeriod(8, 60, 300), "");
  TEST_ASSERT_TRUE((roundEndsInPeriod< 2, 50 >(250)));
  TEST_ASSERT_TRUE((roundEndsInPeriod< 8, 60 >(300)));
  // 1 s: the conversion takes most of it
  static_assert(TempSchedule::fitsInPeriod(3, 50, 50), "");
  static_assert(!TempSchedule::fitsInPeriod(4, 50, 50), "");
  static_assert(TempSchedule::fitsInPeriod(4, 60, 60), "");
  static_assert(!TempSchedule::fitsInPeriod(5, 60, 60), "");
  TEST_ASSERT_TRUE((roundEndsInPeriod< 3, 50 >(50)));
  TEST_ASSERT_TRUE((roundEndsInPeriod< 4, 60 >(60)));
  // what fitsInPeriod() refuses does not fit
  TEST_ASSERT_FALSE((roundEndsInPeriod< 5, 50 >(50)));
  // 2 s: the late cycle within the round counts
  static_assert(TempSchedule::fitsInPeriod(19, 50, 100), "");
  static_assert(!TempSchedule::fitsInPeriod(20, 50, 100), "");
  TEST_ASSERT_TRUE((roundEndsInPeriod< 19, 50 >(100)));
  TEST_ASSERT_FALSE((roundEndsInPeriod< 20, 50 >(100)));
}

int main(int argc, char** argv)
{
  (void)argc;
  (void)argv;

  UNITY_BEGIN();

  RUN_TEST(test_conversion_cycles_cover_750_ms);
  RUN_TEST(test_round_of_two_sensors);
  RUN_TEST(test_first_read_conversion_cycles_after_the_conversion);
  RUN_TEST(test_late_cycles_postpone_without_losing_a_step);
  RUN_TEST(test_late_cycles_still_count_for_the_conversion);
  RUN_TEST(test_a_sensor_that_does_not_answer_is_skipped);
  RUN_TEST(test_new_period_restarts_a_finished_round);
  RUN_TEST(test_conversion_in_the_cycle_that_ends_the_period);
  RUN_TEST(test_new_period_continues_an_unfinished_round);
  RUN_TEST(test_never_two_transactions_in_one_cycle_and_never_before_the_conversion_time);
  RUN_TEST(test_rounds_fit_their_period);

  return UNITY_END();
}
