/**
 * @file test_main.cpp
 * @brief Native unit tests for datalog_math.h
 *
 * The integer datalog arithmetic must reproduce the float arithmetic it replaces (#120),
 *   power_L     = -(sumP / n) * f_powerCal[phase]
 *   Vrms_L_x100 = factor * f_voltageCal[phase] * sqrt(sum_Vsquared / n)
 * over the full input range, without overflow. The float expressions, evaluated in double
 * precision, are the reference. No config.h, no stubs.
 */

#include <unity.h>

#include <math.h>

#include "datalog_math.h"  // Real header - pure C++

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Helpers
// ============================================================================

static constexpr int32_t FULL_SCALE{ 1L << 18 };  // |V_ADC x I_ADC| never exceeds this

// Power calibrations: old board, new board with 50 A and 20 A CTs, and two extremes
static constexpr float oldBoardCal[]{ 0.05F, 0.05F, 0.05F };
static constexpr float newBoard50Cal[]{ 0.1432025F, 0.1432025F, 0.1432025F };
static constexpr float newBoard20Cal[]{ 0.057281F, 0.057281F, 0.057281F };
static constexpr float smallCal[]{ 0.02F, 0.02F, 0.02F };
static constexpr float largeCal[]{ 0.5F, 0.5F, 0.5F };

// Voltage calibrations: old board (per phase) and new board
static constexpr float oldBoardVCal[]{ 0.8151F, 0.8184F, 0.8195F };
static constexpr float newBoardVCal[]{ 0.888065256F, 0.888065256F, 0.888065256F };

/** @brief The float arithmetic being replaced, in watts (import = +ve). */
static double referenceWatts(int32_t averagePower, float cal)
{
  return -static_cast< double >(averagePower) * cal;
}

/** @brief The float arithmetic being replaced, in hundredths of a volt (x4 for long periods). */
static double referenceVrms(uint32_t meanSquare, uint16_t factor, float cal)
{
  return static_cast< double >(factor) * cal * sqrt(static_cast< double >(meanSquare));
}

/**
 * @brief Largest error of watts() against the reference, over the full input range, beyond
 *        what the 16-bit calibration constant explains.
 *
 * @details The calibration constant has at least 16 significant bits (relative error <= 2^-16,
 *          the same constant as in the ISR), and the result is rounded (<= 0.5 W): the returned
 *          value must stay <= 0.5 W.
 */
template< uint8_t N >
static double worstWattsError(const float (&cal)[N])
{
  const auto fixed{ Datalog::powerCalibration(cal) };
  double worst{ 0.0 };
  for (int32_t average = -FULL_SCALE; average <= FULL_SCALE; average += 97)
  {
    const double expected{ referenceWatts(average, cal[0]) };
    if (fabs(expected) > INT16_MAX)
    {
      continue;  // saturated, tested separately
    }
    const double error{ fabs(Datalog::watts(-average, fixed.value[0], fixed.shift) - expected) - fabs(expected) * ldexp(1.0, -16) };
    if (error > worst)
    {
      worst = error;
    }
  }
  return worst;
}

/**
 * @brief Largest error of vrms() against the reference, from 0 to the largest ADC voltage,
 *        beyond what the 16-bit square root and calibration constant explain.
 *
 * @details Both have at least 16 significant bits (relative error <= 2^-16 each, 2^-14 with a
 *          margin), and the result is rounded (<= 0.5): the returned value must stay <= 0.5,
 *          i.e. 0.012 V at most at 230 V.
 */
template< uint8_t N >
static double worstVrmsError(uint16_t factor, const float (&cal)[N], uint32_t maxMeanSquare)
{
  const auto fixed{ Datalog::voltageCalibration(factor, cal) };
  double worst{ 0.0 };
  for (uint8_t phase = 0; phase < N; ++phase)
  {
    for (uint32_t meanSquare = 0; meanSquare <= maxMeanSquare; meanSquare += 13)
    {
      const double expected{ referenceVrms(meanSquare, factor, cal[phase]) };
      const double error{ fabs(Datalog::vrms(meanSquare, fixed.value[phase], fixed.shift) - expected) - expected * ldexp(1.0, -14) };
      if (error > worst)
      {
        worst = error;
      }
    }
  }
  return worst;
}

// ============================================================================
// Integer square root
// ============================================================================

/**
 * @brief Every rounding boundary of every root: round(sqrt(x)) changes from r to r + 1 between
 *        r^2 + r and r^2 + r + 1, since (r + 0.5)^2 = r^2 + r + 0.25.
 */
void test_isqrt_at_every_boundary(void)
{
  for (uint32_t root = 0; root <= UINT16_MAX; ++root)
  {
    const uint32_t square{ root * root };
    if (root > 1)  // r^2 - 1 >= (r - 0.5)^2 from r = 2 on (sqrt(0) = 0)
    {
      TEST_ASSERT_EQUAL_UINT16(root, Datalog::isqrt(square - 1));
    }
    TEST_ASSERT_EQUAL_UINT16(root, Datalog::isqrt(square));
    TEST_ASSERT_EQUAL_UINT16(root, Datalog::isqrt(square + root));  // up to UINT32_MAX
    if (root < UINT16_MAX)                                          // 65535^2 + 65536 = 2^32 wraps
    {
      TEST_ASSERT_EQUAL_UINT16(root + 1, Datalog::isqrt(square + root + 1));
    }
  }
}

/** @brief 2 million values over the whole 32-bit range, against sqrt() in double precision (exact here). */
void test_isqrt_matches_the_reference(void)
{
  constexpr uint32_t STRIDE{ 2147 };  // odd, and not a multiple of small primes: hits all residues
  uint32_t value{ 0 };
  for (uint32_t i = 0; i < (UINT32_MAX / STRIDE); ++i, value += STRIDE)
  {
    const auto expected{ static_cast< uint32_t >(lround(sqrt(static_cast< double >(value)))) };
    if (Datalog::isqrt(value) != (expected > UINT16_MAX ? UINT16_MAX : expected))
    {
      TEST_FAIL_MESSAGE("isqrt() differs from round(sqrt())");
    }
  }
}

void test_isqrt_edge_cases(void)
{
  TEST_ASSERT_EQUAL_UINT16(0, Datalog::isqrt(0));
  TEST_ASSERT_EQUAL_UINT16(1, Datalog::isqrt(1));
  TEST_ASSERT_EQUAL_UINT16(1, Datalog::isqrt(2));                 // 1.414
  TEST_ASSERT_EQUAL_UINT16(2, Datalog::isqrt(3));                 // 1.732
  TEST_ASSERT_EQUAL_UINT16(46341, Datalog::isqrt(1UL << 31));     // 46340.95
  TEST_ASSERT_EQUAL_UINT16(32768, Datalog::isqrt(1UL << 30));     // smallest normalised value
  TEST_ASSERT_EQUAL_UINT16(32768, Datalog::isqrt(0x40008000UL));  // 32768.4999962: rounds down
  TEST_ASSERT_EQUAL_UINT16(32769, Datalog::isqrt(0x40008001UL));  // 32768.5000114: rounds up
}

void test_isqrt_of_the_largest_value_saturates(void)
{
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, Datalog::isqrt(UINT32_MAX));  // 65535.99999 rounds to 65536
}

void test_sqrtScaled_keeps_15_significant_bits(void)
{
  uint8_t fractionBits;
  for (uint32_t value = 1; value < (1UL << 30); value = value * 3 + 1)
  {
    const uint16_t root{ Datalog::sqrtScaled(value, fractionBits) };
    TEST_ASSERT_GREATER_OR_EQUAL_UINT16(1U << 15, root);
    TEST_ASSERT_FLOAT_WITHIN(0.5, sqrt(static_cast< double >(value)) * ldexp(1.0, fractionBits), root);
  }
}

void test_sqrtScaled_of_zero(void)
{
  uint8_t fractionBits{ 99 };
  TEST_ASSERT_EQUAL_UINT16(0, Datalog::sqrtScaled(0, fractionBits));
  TEST_ASSERT_EQUAL_UINT8(0, fractionBits);
}

// ============================================================================
// Power
// ============================================================================

void test_power_calibration_is_constexpr(void)
{
  constexpr auto fixed{ Datalog::powerCalibration(oldBoardCal) };  // must be constexpr

  TEST_ASSERT_EQUAL_UINT8(Energy::calibrationShift(oldBoardCal), fixed.shift);
  TEST_ASSERT_EQUAL_UINT16(Energy::toFixed(0.05F, fixed.shift), fixed.value[0]);
}

void test_watts_matches_the_reference_old_board(void)
{
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.5F, worstWattsError(oldBoardCal));
}

void test_watts_matches_the_reference_new_board(void)
{
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.5F, worstWattsError(newBoard50Cal));
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.5F, worstWattsError(newBoard20Cal));
}

void test_watts_matches_the_reference_extremes(void)
{
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.5F, worstWattsError(smallCal));
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.5F, worstWattsError(largeCal));
}

void test_watts_is_sign_symmetric(void)
{
  const auto fixed{ Datalog::powerCalibration(newBoard50Cal) };
  for (int32_t average = 0; average <= 200000; average += 101)
  {
    TEST_ASSERT_EQUAL_INT16(-Datalog::watts(average, fixed.value[0], fixed.shift), Datalog::watts(-average, fixed.value[0], fixed.shift));
  }
}

void test_watts_saturates_beyond_int16(void)
{
  const auto fixed{ Datalog::powerCalibration(largeCal) };  // 2^18 x 0.5 = 131072 W
  TEST_ASSERT_EQUAL_INT16(INT16_MAX, Datalog::watts(FULL_SCALE, fixed.value[0], fixed.shift));
  TEST_ASSERT_EQUAL_INT16(-INT16_MAX, Datalog::watts(-FULL_SCALE, fixed.value[0], fixed.shift));
}

void test_watts_differs_from_the_float_code_by_at_most_1_W(void)
{
  // the float code truncated, the integer code rounds
  const auto fixed{ Datalog::powerCalibration(oldBoardCal) };
  for (int32_t sumP = -FULL_SCALE; sumP <= FULL_SCALE; sumP += 89)
  {
    auto truncated{ static_cast< int16_t >(sumP * oldBoardCal[0]) };
    truncated *= -1;
    TEST_ASSERT_INT_WITHIN(1, truncated, Datalog::watts(-sumP, fixed.value[0], fixed.shift));
  }
}

// ============================================================================
// RMS voltage
// ============================================================================

void test_power_shift_follows_the_sample_count(void)
{
  // full-span sines average 2^17 per sample: up to 16383 sample sets fit in 31 bits unshifted
  TEST_ASSERT_EQUAL_UINT8(0, Datalog::powerShift(16383));
  TEST_ASSERT_EQUAL_UINT8(4, Datalog::powerShift(16384));
  // 3-phase, 36 sets per cycle at most at 50 Hz: 5 s and 9 s unshifted, 10 s and 30 s shifted
  TEST_ASSERT_EQUAL_UINT8(0, Datalog::powerShift(5UL * 50 * 36));
  TEST_ASSERT_EQUAL_UINT8(0, Datalog::powerShift(9UL * 50 * 36));
  TEST_ASSERT_EQUAL_UINT8(4, Datalog::powerShift(10UL * 50 * 36));
  TEST_ASSERT_EQUAL_UINT8(4, Datalog::powerShift(30UL * 50 * 36));
}

void test_power_shift_keeps_30_s_of_any_signal_in_32_bits(void)
{
  // shifted by 4, a sample adds at most 2^14: the 54000 sets of 30 s stay below 2^31
  const uint32_t sets{ 30UL * 50 * 36 };
  TEST_ASSERT_TRUE(static_cast< uint64_t >(sets) * ((1UL << 18) >> Datalog::powerShift(sets)) <= INT32_MAX);
}

void test_average_power_unshifted_is_the_quotient(void)
{
  TEST_ASSERT_EQUAL_INT32(-142, Datalog::averagePower< 0 >(-1000, 7));
  TEST_ASSERT_EQUAL_INT32(142, Datalog::averagePower< 0 >(1000, 7));
  TEST_ASSERT_EQUAL_INT32(0, Datalog::averagePower< 0 >(0, 9000));
}

void test_average_power_shifted_is_the_exact_average(void)
{
  // sum x 16 / n truncated toward zero, over the whole range of a 30 s period
  const int32_t sums[]{ 0, 1, -1, 15, -15, 123456789, -123456789, 54000L * 16384 - 1, -(54000L * 16384 - 1), 3374, -3374 };
  const uint16_t counts[]{ 1, 7, 9000, 16383, 16384, 54000, UINT16_MAX };
  for (const int32_t sum : sums)
  {
    for (const uint16_t n : counts)
    {
      const auto exact{ static_cast< int32_t >(static_cast< int64_t >(sum) * 16 / n) };
      TEST_ASSERT_EQUAL_INT32(exact, Datalog::averagePower< 4 >(sum, n));
    }
  }
}

/** 30 s at 50 Hz, 32 sample sets per cycle, summed as the ISR does (rounded, / 16), against the
 *  exact average (64-bit sum, here only); noise: pseudo-random, uniform over +-noise */
static void checkShiftedAverage(const int32_t amplitude, const int32_t offset, const int32_t noise, const int32_t delta)
{
  const uint16_t n{ 30U * 50 * 32 };
  uint32_t lcg{ 12345 };
  int32_t scaled{ 0 };
  int64_t exact{ 0 };
  for (uint16_t i = 0; i != n; ++i)
  {
    lcg = lcg * 1664525UL + 1013904223UL;
    const int32_t dither{ noise ? static_cast< int32_t >(lcg >> 16) % (2 * noise + 1) - noise : 0 };
    const auto instP{ static_cast< int32_t >(lround(offset + amplitude * sin(2.0 * M_PI * i / 32.0 + 0.1))) + dither };
    scaled += (instP + 8) >> 4;
    exact += instP;
  }
  TEST_ASSERT_INT32_WITHIN(delta, static_cast< int32_t >(exact / n), Datalog::averagePower< 4 >(scaled, n));
}

void test_shifted_sum_is_within_half_the_shift_over_30_s(void)
{
  // any signal, even a constant or a sine repeating the same samples: within 8 step^2 (half of 16)
  const int32_t amplitudes[]{ 0, 1, 7, 100, 5000, 131072 };
  const int32_t offsets[]{ 0, 3, -3, 1000, -77777, 131071 };
  for (const int32_t amplitude : amplitudes)
  {
    for (const int32_t offset : offsets)
    {
      checkShiftedAverage(amplitude, offset, 0, 8);
    }
  }
}

void test_shifted_sum_keeps_the_average_of_a_noisy_signal_over_30_s(void)
{
  // with the noise of real samples (here +-64 step^2), the rounding averages out: within 1 step^2
  const int32_t amplitudes[]{ 0, 100, 5000, 131072 };
  const int32_t offsets[]{ 0, 3, -3, 1000, -77777 };
  for (const int32_t amplitude : amplitudes)
  {
    for (const int32_t offset : offsets)
    {
      checkShiftedAverage(amplitude, offset, 64, 1);
    }
  }
}

void test_voltage_calibration_is_constexpr(void)
{
  constexpr auto fixed{ Datalog::voltageCalibration(100, oldBoardVCal) };  // must be constexpr

  TEST_ASSERT_EQUAL_UINT8(9, fixed.shift);  // 81.95 x 2^9 = 41958 fills the 16 bits
  TEST_ASSERT_GREATER_OR_EQUAL_UINT16(1U << 15, fixed.value[2]);
}

void test_vrms_matches_the_reference_short_period(void)
{
  // 5 s datalog: V^2 unscaled, V_ADC up to 512 (old board: 230 V ~ 282 steps rms)
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.5F, worstVrmsError(100, oldBoardVCal, 512UL * 512UL));
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.5F, worstVrmsError(100, newBoardVCal, 512UL * 512UL));
}

void test_vrms_matches_the_reference_long_period(void)
{
  // datalog > 10 s: V^2 scaled down by 16, the result scaled up by 4
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.5F, worstVrmsError(400, oldBoardVCal, 512UL * 512UL / 16));
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.5F, worstVrmsError(400, newBoardVCal, 512UL * 512UL / 16));
}

void test_vrms_at_230_V(void)
{
  const auto fixed{ Datalog::voltageCalibration(100, newBoardVCal) };
  const double steps{ 230.0 / newBoardVCal[0] };  // rms ADC steps for 230 V
  const auto meanSquare{ static_cast< uint32_t >(steps * steps) };

  TEST_ASSERT_INT_WITHIN(1, lround(referenceVrms(meanSquare, 100, newBoardVCal[0])), Datalog::vrms(meanSquare, fixed.value[0], fixed.shift));
  TEST_ASSERT_INT_WITHIN(2, 23000, Datalog::vrms(meanSquare, fixed.value[0], fixed.shift));
}

void test_vrms_of_zero(void)
{
  const auto fixed{ Datalog::voltageCalibration(100, newBoardVCal) };
  TEST_ASSERT_EQUAL_UINT16(0, Datalog::vrms(0, fixed.value[0], fixed.shift));
}

void test_vrms_saturates(void)
{
  const auto fixed{ Datalog::voltageCalibration(400, newBoardVCal) };
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, Datalog::vrms(UINT32_MAX, fixed.value[0], fixed.shift));
}

void test_vrms_factor_follows_the_scaling_of_the_isr(void)
{
  static_assert(Datalog::vrmsFactor(5) == 100, "must be constexpr");
  TEST_ASSERT_EQUAL_UINT16(100, Datalog::vrmsFactor(1));
  TEST_ASSERT_EQUAL_UINT16(100, Datalog::vrmsFactor(10));  // V^2 unscaled up to 10 s
  TEST_ASSERT_EQUAL_UINT16(400, Datalog::vrmsFactor(11));  // V^2 / 16 above: root / 4
  TEST_ASSERT_EQUAL_UINT16(400, Datalog::vrmsFactor(40));
}

void test_valid_voltage_calibration(void)
{
  static constexpr float zeroCal[]{ 0.8F, 0.0F, 0.8F };
  static constexpr float negativeCal[]{ 0.8F, 0.8F, -0.8F };
  static constexpr float tinyCal[]{ 0.0001F, 0.0001F, 0.0001F };

  static_assert(Datalog::isValidVoltageCalibration(100, oldBoardVCal), "old board");
  static_assert(Datalog::isValidVoltageCalibration(400, newBoardVCal), "new board, long period");
  TEST_ASSERT_FALSE(Datalog::isValidVoltageCalibration(100, zeroCal));
  TEST_ASSERT_FALSE(Datalog::isValidVoltageCalibration(100, negativeCal));
  TEST_ASSERT_FALSE(Datalog::isValidVoltageCalibration(100, tinyCal));  // shift > 16
}

int main(int argc, char** argv)
{
  (void)argc;
  (void)argv;

  UNITY_BEGIN();

  RUN_TEST(test_isqrt_at_every_boundary);
  RUN_TEST(test_isqrt_matches_the_reference);
  RUN_TEST(test_isqrt_edge_cases);
  RUN_TEST(test_isqrt_of_the_largest_value_saturates);
  RUN_TEST(test_sqrtScaled_keeps_15_significant_bits);
  RUN_TEST(test_sqrtScaled_of_zero);

  RUN_TEST(test_power_calibration_is_constexpr);
  RUN_TEST(test_watts_matches_the_reference_old_board);
  RUN_TEST(test_watts_matches_the_reference_new_board);
  RUN_TEST(test_watts_matches_the_reference_extremes);
  RUN_TEST(test_watts_is_sign_symmetric);
  RUN_TEST(test_watts_saturates_beyond_int16);
  RUN_TEST(test_watts_differs_from_the_float_code_by_at_most_1_W);

  RUN_TEST(test_power_shift_follows_the_sample_count);
  RUN_TEST(test_power_shift_keeps_30_s_of_any_signal_in_32_bits);
  RUN_TEST(test_average_power_unshifted_is_the_quotient);
  RUN_TEST(test_average_power_shifted_is_the_exact_average);
  RUN_TEST(test_shifted_sum_is_within_half_the_shift_over_30_s);
  RUN_TEST(test_shifted_sum_keeps_the_average_of_a_noisy_signal_over_30_s);

  RUN_TEST(test_voltage_calibration_is_constexpr);
  RUN_TEST(test_vrms_matches_the_reference_short_period);
  RUN_TEST(test_vrms_matches_the_reference_long_period);
  RUN_TEST(test_vrms_at_230_V);
  RUN_TEST(test_vrms_of_zero);
  RUN_TEST(test_vrms_saturates);
  RUN_TEST(test_vrms_factor_follows_the_scaling_of_the_isr);
  RUN_TEST(test_valid_voltage_calibration);

  return UNITY_END();
}
