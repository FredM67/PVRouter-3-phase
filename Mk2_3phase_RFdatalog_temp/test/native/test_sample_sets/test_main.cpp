// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file test_main.cpp
 * @brief Native tests for sample_sets.h: the counter of the sample sets of a datalog period
 *
 * @details The counter is 16 bits while the largest count of a datalog period fits in it, 32 bits
 *          above: the default 5 s period keeps its 16-bit counter, periods up to 40 s are safe.
 *          Largest count = mains cycles of the period x N_MAX, the most sample sets in a cycle:
 *          36 at 50 Hz (32 + 4), 30 at 60 Hz (26 + 4), with 6 conversions of 104 us per set.
 */

#include <unity.h>

#include <stdint.h>

#include "sample_sets.h"  // Real header - pure C++

void setUp(void) {}
void tearDown(void) {}

inline constexpr uint8_t N_MAX_50HZ{ 36 };
inline constexpr uint8_t N_MAX_60HZ{ 30 };

template< typename A, typename B >
struct same
{
  static constexpr bool value{ false };
};
template< typename A >
struct same< A, A >
{
  static constexpr bool value{ true };
};

/** @brief The counter of a period holds its largest count. */
template< uint32_t MaxCount >
static void assertHolds()
{
  using Counter = SampleSets::Counter< MaxCount >;
  TEST_ASSERT_EQUAL_UINT32(MaxCount, static_cast< uint32_t >(static_cast< Counter >(MaxCount)));
}

void test_largest_count_of_a_period(void)
{
  static_assert(SampleSets::maxPerDatalog(250, N_MAX_50HZ) == 9000UL, "must be constexpr");
  TEST_ASSERT_EQUAL_UINT32(64800UL, SampleSets::maxPerDatalog(36 * 50, N_MAX_50HZ));
  TEST_ASSERT_EQUAL_UINT32(64800UL, SampleSets::maxPerDatalog(36 * 60, N_MAX_60HZ));
  TEST_ASSERT_EQUAL_UINT32(72000UL, SampleSets::maxPerDatalog(40 * 50, N_MAX_50HZ));
  TEST_ASSERT_EQUAL_UINT32(72000UL, SampleSets::maxPerDatalog(40 * 60, N_MAX_60HZ));
}

void test_16_bits_up_to_36_s(void)
{
  constexpr uint32_t at5s{ SampleSets::maxPerDatalog(5 * 50, N_MAX_50HZ) };
  constexpr uint32_t at36s50Hz{ SampleSets::maxPerDatalog(36 * 50, N_MAX_50HZ) };
  constexpr uint32_t at36s60Hz{ SampleSets::maxPerDatalog(36 * 60, N_MAX_60HZ) };

  TEST_ASSERT_TRUE((same< SampleSets::Counter< at5s >, uint16_t >::value));
  TEST_ASSERT_TRUE((same< SampleSets::Counter< at36s50Hz >, uint16_t >::value));
  TEST_ASSERT_TRUE((same< SampleSets::Counter< at36s60Hz >, uint16_t >::value));
  TEST_ASSERT_TRUE((same< SampleSets::Counter< UINT16_MAX >, uint16_t >::value));

  assertHolds< at36s50Hz >();
  assertHolds< at36s60Hz >();
  assertHolds< UINT16_MAX >();
}

void test_32_bits_from_37_s_to_40_s(void)
{
  constexpr uint32_t at37s50Hz{ SampleSets::maxPerDatalog(37 * 50, N_MAX_50HZ) };
  constexpr uint32_t at37s60Hz{ SampleSets::maxPerDatalog(37 * 60, N_MAX_60HZ) };
  constexpr uint32_t at40s50Hz{ SampleSets::maxPerDatalog(40 * 50, N_MAX_50HZ) };
  constexpr uint32_t at40s60Hz{ SampleSets::maxPerDatalog(40 * 60, N_MAX_60HZ) };

  TEST_ASSERT_TRUE((same< SampleSets::Counter< UINT16_MAX + 1UL >, uint32_t >::value));
  TEST_ASSERT_TRUE((same< SampleSets::Counter< at37s50Hz >, uint32_t >::value));
  TEST_ASSERT_TRUE((same< SampleSets::Counter< at37s60Hz >, uint32_t >::value));
  TEST_ASSERT_TRUE((same< SampleSets::Counter< at40s50Hz >, uint32_t >::value));
  TEST_ASSERT_TRUE((same< SampleSets::Counter< at40s60Hz >, uint32_t >::value));

  assertHolds< at37s50Hz >();
  assertHolds< at40s50Hz >();
  assertHolds< at40s60Hz >();
}

void test_datalog_sums_hold_40_s(void)
{
  // each sample adds at most 2^18 (|V_ADC x I_ADC| or V_ADC^2): the power sum is 64 bits,
  // the V^2 sum 32 bits, scaled down by 16 for long periods (Datalog::isVSquaredScaled())
  constexpr uint64_t fullScale{ 1ULL << 18 };
  constexpr uint64_t at40s{ SampleSets::maxPerDatalog(40 * 60, N_MAX_60HZ) };  // the larger of 50/60 Hz

  TEST_ASSERT_TRUE(at40s * (fullScale >> 4) <= UINT32_MAX);
  TEST_ASSERT_TRUE(at40s * fullScale <= static_cast< uint64_t >(INT64_MAX));
}

int main(int /* argc */, char** /* argv */)
{
  UNITY_BEGIN();

  RUN_TEST(test_largest_count_of_a_period);
  RUN_TEST(test_16_bits_up_to_36_s);
  RUN_TEST(test_32_bits_from_37_s_to_40_s);
  RUN_TEST(test_datalog_sums_hold_40_s);

  return UNITY_END();
}
