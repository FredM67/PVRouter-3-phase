/**
 * @file test_main.cpp
 * @brief Native unit tests for energy_bucket.h
 *
 * The integer energy bucket must reproduce the float arithmetic it replaces,
 *   bucket += (l_sumP / n) * f_powerCal[phase]
 * over the full input range, without overflow and without drifting over time.
 * The float/double expression is the reference. No config.h, no stubs.
 */

#include <unity.h>

#include <math.h>

#include "energy_bucket.h"  // Real header - pure C++

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Helpers
// ============================================================================

static constexpr int32_t FULL_SCALE{ 1L << 18 };  // |V_ADC x I_ADC| never exceeds this

/** @brief The float arithmetic being replaced, in bucket units. */
static double reference(int32_t averagePower, float cal)
{
  return static_cast< double >(averagePower) * cal * (1 << Energy::FRACTION_BITS);
}

/** @brief Relative error of a quantised calibration value. */
static double quantisationError(float cal, uint8_t shift)
{
  const double exact{ static_cast< double >(cal) * ldexp(1.0, shift) };
  return fabs(Energy::toFixed(cal, shift) - exact) / exact;
}

// A real-world calibration set, and two extremes around it
static constexpr float typicalCal[]{ 0.042346F, 0.043453F, 0.042969F };
static constexpr float largeCal[]{ 0.1F, 0.1F, 0.1F };
static constexpr float smallCal[]{ 0.02F, 0.02F, 0.02F };

// ============================================================================
// Calibration quantisation
// ============================================================================

void test_shift_for_a_typical_calibration(void)
{
  constexpr uint8_t shift{ Energy::calibrationShift(typicalCal) };  // must be constexpr

  TEST_ASSERT_EQUAL_UINT8(20, shift);
}

void test_shift_for_large_and_small_values(void)
{
  TEST_ASSERT_EQUAL_UINT8(19, Energy::calibrationShift(largeCal));
  TEST_ASSERT_EQUAL_UINT8(21, Energy::calibrationShift(smallCal));
}

void test_shift_follows_the_largest_value(void)
{
  // phases share one shift, so their contributions add up in the same unit
  static constexpr float mixed[]{ 0.02F, 0.1F, 0.05F };

  TEST_ASSERT_EQUAL_UINT8(19, Energy::calibrationShift(mixed));
}

void test_largest_value_fills_16_bits(void)
{
  constexpr uint8_t shift{ Energy::calibrationShift(typicalCal) };

  // at least 2^15, at most 2^16 - 1: no bit wasted, none lost
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(32768, Energy::toFixed(0.043453F, shift));
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(65535, Energy::toFixed(0.043453F, shift));
}

void test_toFixed_rounds_to_nearest(void)
{
  constexpr uint16_t fixed{ Energy::toFixed(0.042346F, 20) };  // must be constexpr

  TEST_ASSERT_EQUAL_UINT16(44403, fixed);                       // 0.042346 x 2^20 = 44403.0
  TEST_ASSERT_EQUAL_UINT16(52429, Energy::toFixed(0.05F, 20));  // 52428.8 -> 52429
}

void test_quantisation_error_is_negligible(void)
{
  // sweep single-phase calibrations from 0.005 to 0.5: the normalised shift keeps the
  // error below 0.002%, far below what a calibration against a meter can achieve
  for (float cal = 0.005F; cal < 0.5F; cal *= 1.07F)
  {
    const float set[]{ cal };
    TEST_ASSERT_TRUE_MESSAGE(quantisationError(cal, Energy::calibrationShift(set)) < 2e-5, "quantisation error too large");
  }
}

void test_typical_set_quantisation_error(void)
{
  constexpr uint8_t shift{ Energy::calibrationShift(typicalCal) };

  for (const float cal : typicalCal)
  {
    TEST_ASSERT_TRUE(quantisationError(cal, shift) < 1e-5);
  }
}

void test_fixed_table_for_a_typical_calibration(void)
{
  // the whole table is built at compile time: the ISR indexes it by phase
  constexpr auto table{ Energy::toFixed(typicalCal) };

  TEST_ASSERT_EQUAL_UINT8(20, table.shift);
  TEST_ASSERT_EQUAL_UINT16(44403, table.value[0]);
  TEST_ASSERT_EQUAL_UINT16(45564, table.value[1]);
  TEST_ASSERT_EQUAL_UINT16(45056, table.value[2]);
}

void test_valid_calibration_accepts_realistic_values(void)
{
  constexpr bool valid{ Energy::isValidCalibration(typicalCal) };  // must be constexpr

  TEST_ASSERT_TRUE(valid);
  TEST_ASSERT_TRUE(Energy::isValidCalibration(largeCal));
  TEST_ASSERT_TRUE(Energy::isValidCalibration(smallCal));
}

void test_valid_calibration_rejects_zero_negative_and_huge_values(void)
{
  static constexpr float withZero[]{ 0.05F, 0.0F, 0.05F };
  static constexpr float withNegative[]{ 0.05F, -0.05F, 0.05F };
  static constexpr float huge[]{ 10.0F, 10.0F, 10.0F };  // leaves too few fractional bits

  TEST_ASSERT_FALSE(Energy::isValidCalibration(withZero));
  TEST_ASSERT_FALSE(Energy::isValidCalibration(withNegative));
  TEST_ASSERT_FALSE(Energy::isValidCalibration(huge));
}

// ============================================================================
// Contribution of one cycle, against the float reference
// ============================================================================

/** @brief Every average power from -FULL_SCALE to +FULL_SCALE, in uneven steps. */
static void checkAgainstReference(const float (&cal)[3])
{
  const uint8_t shift{ Energy::calibrationShift(cal) };

  for (uint8_t phase = 0; phase < 3; ++phase)
  {
    const uint16_t calFixed{ Energy::toFixed(cal[phase], shift) };

    for (int32_t power = -FULL_SCALE; power <= FULL_SCALE; power += 997)
    {
      const double expected{ reference(power, cal[phase]) };
      const double actual{ static_cast< double >(Energy::contribution(power, calFixed, shift)) };

      // one unit of rounding, plus the calibration quantisation
      TEST_ASSERT_TRUE_MESSAGE(fabs(actual - expected) <= 1.0 + fabs(expected) * 2e-5, "contribution differs from the float reference");
    }
  }
}

void test_contribution_matches_the_reference_for_a_typical_calibration(void)
{
  checkAgainstReference(typicalCal);
}

void test_contribution_matches_the_reference_for_extreme_calibrations(void)
{
  checkAgainstReference(largeCal);
  checkAgainstReference(smallCal);
}

void test_contribution_of_zero_is_zero(void)
{
  const uint8_t shift{ Energy::calibrationShift(typicalCal) };

  TEST_ASSERT_EQUAL_INT32(0, Energy::contribution(0, Energy::toFixed(typicalCal[0], shift), shift));
}

void test_contribution_at_full_scale_does_not_overflow(void)
{
  // the product needs ~35 bits: a 32-bit intermediate would wrap here
  const uint8_t shift{ Energy::calibrationShift(largeCal) };
  const uint16_t calFixed{ Energy::toFixed(largeCal[0], shift) };

  TEST_ASSERT_INT32_WITHIN(2, lround(reference(FULL_SCALE, largeCal[0])), Energy::contribution(FULL_SCALE, calFixed, shift));
  TEST_ASSERT_INT32_WITHIN(2, lround(reference(-FULL_SCALE, largeCal[0])), Energy::contribution(-FULL_SCALE, calFixed, shift));
}

void test_contribution_is_sign_symmetric(void)
{
  // import and export must weigh the same, or the bucket drifts one way
  const uint8_t shift{ Energy::calibrationShift(typicalCal) };
  const uint16_t calFixed{ Energy::toFixed(typicalCal[1], shift) };

  for (int32_t power = 1; power <= FULL_SCALE; power += 1009)
  {
    TEST_ASSERT_EQUAL_INT32(-Energy::contribution(power, calFixed, shift), Energy::contribution(-power, calFixed, shift));
  }
}

void test_no_drift_over_one_minute(void)
{
  // 3000 cycles (1 min at 50 Hz) of a slowly varying load: rounding must not accumulate
  const uint8_t shift{ Energy::calibrationShift(typicalCal) };
  const uint16_t calFixed{ Energy::toFixed(typicalCal[2], shift) };

  int64_t bucket{ 0 };
  double expected{ 0.0 };

  for (int32_t cycle = 0; cycle < 3000; ++cycle)
  {
    const int32_t power{ static_cast< int32_t >(20000.0 + 15000.0 * sin(cycle * 0.01)) + (cycle % 7) };
    bucket += Energy::contribution(power, calFixed, shift);
    expected += reference(power, typicalCal[2]);
  }

  TEST_ASSERT_TRUE_MESSAGE(fabs(static_cast< double >(bucket) - expected) <= fabs(expected) * 2e-5 + 64.0, "the integer bucket drifts from the float one");
}

// ============================================================================
// Per-sample calibration: cal / n in one table, no division in the ISR
// ============================================================================

// sample sets per mains cycle: ~32 at 50 Hz, ~26.7 at 60 Hz, with margin
static constexpr uint8_t N_MIN_50HZ{ 26 };
static constexpr uint8_t N_MAX_50HZ{ 38 };

/** @brief The exact energy of one cycle: no truncating division, in bucket units. */
static double exactReference(int32_t sumP, uint8_t n, float cal)
{
  return static_cast< double >(sumP) / n * cal * (1 << Energy::FRACTION_BITS);
}

void test_per_sample_shift_and_values(void)
{
  constexpr auto table{ Energy::toFixedPerSample< N_MIN_50HZ, N_MAX_50HZ >(typicalCal) };  // must be constexpr

  TEST_ASSERT_EQUAL_UINT8(25, table.shift);
  // at n = 32, cal / 32 x 2^25 == cal x 2^20: the step-1 constants again
  TEST_ASSERT_EQUAL_UINT16(44403, table.value[0][32 - N_MIN_50HZ]);
  TEST_ASSERT_EQUAL_UINT16(45564, table.value[1][32 - N_MIN_50HZ]);
  TEST_ASSERT_EQUAL_UINT16(45056, table.value[2][32 - N_MIN_50HZ]);
}

void test_per_sample_shift_for_60Hz(void)
{
  constexpr auto table{ Energy::toFixedPerSample< 20, 32 >(typicalCal) };

  TEST_ASSERT_EQUAL_UINT8(24, table.shift);
  TEST_ASSERT_EQUAL_UINT16(22202, table.value[0][32 - 20]);
}

void test_per_sample_largest_value_fills_16_bits(void)
{
  constexpr auto table{ Energy::toFixedPerSample< N_MIN_50HZ, N_MAX_50HZ >(typicalCal) };

  // the largest entry is the largest calibration over the smallest n
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(32768, table.value[1][0]);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(65535, table.value[1][0]);
}

void test_per_sample_contribution_matches_the_exact_reference(void)
{
  constexpr auto table{ Energy::toFixedPerSample< N_MIN_50HZ, N_MAX_50HZ >(typicalCal) };

  for (uint8_t phase = 0; phase < 3; ++phase)
  {
    for (uint8_t n = N_MIN_50HZ; n <= N_MAX_50HZ; ++n)
    {
      const int32_t fullScale{ static_cast< int32_t >(n) * FULL_SCALE };

      for (int32_t sumP = -fullScale; sumP <= fullScale; sumP += 49999)
      {
        const double expected{ exactReference(sumP, n, typicalCal[phase]) };
        const double actual{ static_cast< double >(Energy::contribution(sumP, table.value[phase][n - N_MIN_50HZ], table.shift)) };

        // one unit of rounding, plus the quantisation of cal / n (coarsest at the largest n)
        TEST_ASSERT_TRUE_MESSAGE(fabs(actual - expected) <= 1.0 + fabs(expected) * 4e-5, "per-sample contribution differs from the exact reference");
      }
    }
  }
}

void test_per_sample_agrees_with_the_division_path(void)
{
  // the fallback (out-of-range n) divides first: both paths must agree within the
  // truncation of that division (< 1 raw unit, i.e. < 1 bucket unit), their two
  // roundings, and their two calibration quantisations (which may point opposite ways)
  constexpr auto table{ Energy::toFixedPerSample< N_MIN_50HZ, N_MAX_50HZ >(typicalCal) };
  constexpr auto fixed{ Energy::toFixed(typicalCal) };

  for (uint8_t n = N_MIN_50HZ; n <= N_MAX_50HZ; ++n)
  {
    for (int32_t sumP = -static_cast< int32_t >(n) * FULL_SCALE; sumP <= static_cast< int32_t >(n) * FULL_SCALE; sumP += 77777)
    {
      const int32_t direct{ Energy::contribution(sumP, table.value[0][n - N_MIN_50HZ], table.shift) };
      const int32_t divided{ Energy::contribution(sumP / n, fixed.value[0], fixed.shift) };

      TEST_ASSERT_TRUE_MESSAGE(fabs(static_cast< double >(direct - divided)) <= 2.0 + fabs(static_cast< double >(divided)) * 6e-5, "the per-sample and division paths disagree");
    }
  }
}

void test_per_sample_fallback_for_an_unexpected_cycle_length(void)
{
  // n outside the table (start-up, missing phase): the ISR rescales the sum to NMin
  // samples, (sumP / n) x NMin, and uses the NMin entry - same shift, no second path
  constexpr auto table{ Energy::toFixedPerSample< N_MIN_50HZ, N_MAX_50HZ >(typicalCal) };
  static constexpr uint8_t unexpected[]{ 1, 5, 20, 25, 39, 64, 100, 255 };

  for (const uint8_t n : unexpected)
  {
    for (int32_t average = -FULL_SCALE; average <= FULL_SCALE; average += 9973)
    {
      // with a remainder, so that the division truncates as it does in the ISR
      const int32_t sumP{ average * n + ((average < 0) ? -(n - 1) : (n - 1)) };
      const int32_t rescaled{ (sumP / n) * N_MIN_50HZ };
      const double expected{ exactReference(sumP, n, typicalCal[1]) };
      const double actual{ static_cast< double >(Energy::contribution(rescaled, table.value[1][0], table.shift)) };

      // one unit of rounding, < 1 unit of truncation, plus the quantisation
      TEST_ASSERT_TRUE_MESSAGE(fabs(actual - expected) <= 2.0 + fabs(expected) * 4e-5, "the fallback differs from the exact reference");
    }
  }
}

void test_per_sample_full_scale_does_not_overflow(void)
{
  // the largest sum the ISR can hand over: N_MAX samples at full scale
  constexpr auto table{ Energy::toFixedPerSample< N_MIN_50HZ, N_MAX_50HZ >(largeCal) };
  const int32_t sumP{ static_cast< int32_t >(N_MAX_50HZ) * FULL_SCALE };
  const uint16_t calOverN{ table.value[0][N_MAX_50HZ - N_MIN_50HZ] };

  // same contract as elsewhere (1 unit + quantisation); an overflow would be off by orders of magnitude
  const double expected{ exactReference(sumP, N_MAX_50HZ, largeCal[0]) };

  TEST_ASSERT_TRUE(fabs(Energy::contribution(sumP, calOverN, table.shift) - expected) <= 1.0 + expected * 4e-5);
  TEST_ASSERT_TRUE(fabs(Energy::contribution(-sumP, calOverN, table.shift) + expected) <= 1.0 + expected * 4e-5);
}

void test_per_sample_no_drift_over_one_minute(void)
{
  // 3000 cycles with n wandering around 32, as it does between zero crossings
  constexpr auto table{ Energy::toFixedPerSample< N_MIN_50HZ, N_MAX_50HZ >(typicalCal) };

  int64_t bucket{ 0 };
  double expected{ 0.0 };

  for (int32_t cycle = 0; cycle < 3000; ++cycle)
  {
    const uint8_t n{ static_cast< uint8_t >(31 + cycle % 3) };
    const int32_t sumP{ n * static_cast< int32_t >(20000.0 + 15000.0 * sin(cycle * 0.01)) + (cycle % 11) };

    bucket += Energy::contribution(sumP, table.value[2][n - N_MIN_50HZ], table.shift);
    expected += exactReference(sumP, n, typicalCal[2]);
  }

  TEST_ASSERT_TRUE_MESSAGE(fabs(static_cast< double >(bucket) - expected) <= fabs(expected) * 4e-5 + 64.0, "the per-sample bucket drifts from the exact one");
}

// ============================================================================
// Units
// ============================================================================

void test_fromWatts_scales_by_the_fraction_bits(void)
{
  constexpr int32_t exportAdjustment{ Energy::fromWatts(20) };  // must be constexpr

  TEST_ASSERT_EQUAL_INT32(20 << Energy::FRACTION_BITS, exportAdjustment);
  TEST_ASSERT_EQUAL_INT32(-(50 << Energy::FRACTION_BITS), Energy::fromWatts(-50));
}

void test_bucket_capacity_fits_in_32_bits(void)
{
  // 1 Wh working zone at 60 Hz, the largest configuration: 3600 J x 60 cycles/s
  constexpr int32_t capacity{ Energy::fromWatts(3600L * 60) };

  TEST_ASSERT_EQUAL_INT32(3600L * 60 * (1L << Energy::FRACTION_BITS), capacity);
}

// ============================================================================

int main(int, char **)
{
  UNITY_BEGIN();

  RUN_TEST(test_shift_for_a_typical_calibration);
  RUN_TEST(test_shift_for_large_and_small_values);
  RUN_TEST(test_shift_follows_the_largest_value);
  RUN_TEST(test_largest_value_fills_16_bits);
  RUN_TEST(test_toFixed_rounds_to_nearest);
  RUN_TEST(test_quantisation_error_is_negligible);
  RUN_TEST(test_typical_set_quantisation_error);
  RUN_TEST(test_fixed_table_for_a_typical_calibration);
  RUN_TEST(test_valid_calibration_accepts_realistic_values);
  RUN_TEST(test_valid_calibration_rejects_zero_negative_and_huge_values);

  RUN_TEST(test_contribution_matches_the_reference_for_a_typical_calibration);
  RUN_TEST(test_contribution_matches_the_reference_for_extreme_calibrations);
  RUN_TEST(test_contribution_of_zero_is_zero);
  RUN_TEST(test_contribution_at_full_scale_does_not_overflow);
  RUN_TEST(test_contribution_is_sign_symmetric);
  RUN_TEST(test_no_drift_over_one_minute);

  RUN_TEST(test_per_sample_shift_and_values);
  RUN_TEST(test_per_sample_shift_for_60Hz);
  RUN_TEST(test_per_sample_largest_value_fills_16_bits);
  RUN_TEST(test_per_sample_contribution_matches_the_exact_reference);
  RUN_TEST(test_per_sample_agrees_with_the_division_path);
  RUN_TEST(test_per_sample_fallback_for_an_unexpected_cycle_length);
  RUN_TEST(test_per_sample_full_scale_does_not_overflow);
  RUN_TEST(test_per_sample_no_drift_over_one_minute);

  RUN_TEST(test_fromWatts_scales_by_the_fraction_bits);
  RUN_TEST(test_bucket_capacity_fits_in_32_bits);

  return UNITY_END();
}
