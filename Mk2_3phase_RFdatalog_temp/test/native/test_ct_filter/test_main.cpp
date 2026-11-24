// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file test_main.cpp
 * @brief Native tests for ct_filter.h: the CT filter, in fixed point
 *
 * @details The current sample reaches the filter DC-removed, left-aligned (x64 scale, int16).
 *          The reference is Robin's filter (Mk2_fasterControl_Full), verbatim, in float at his
 *          x256 resolution: the fixed-point filter must track it within TOLERANCE of its output
 *          on a step, a sine and noise, for the settings of the old boards (alpha 0.0011, gain 9),
 *          the shipped alpha (0.002) and a few gains.
 */

#include <unity.h>

#include <math.h>
#include <stdint.h>

#include "ct_filter.h"  // Real header - pure C++

void setUp(void) {}
void tearDown(void) {}

/** @brief Output tolerance against Robin's float, in x64 units: one ADC count (see the README). */
inline constexpr int32_t TOLERANCE{ 64 };

/** @brief Robin's filter, verbatim from Mk2_fasterControl_Full/processing.cpp (x256 scale). */
struct Robin
{
  int32_t lpf_long{};

  int32_t apply(const int16_t countsMinusDC, const float lpf_gain, const float alpha)
  {
    int32_t sampleIminusDC_grid = ((int32_t)countsMinusDC) << 8;
    const int32_t last_lpf_long = lpf_long;
    lpf_long += alpha * (sampleIminusDC_grid - last_lpf_long);
    sampleIminusDC_grid += (lpf_gain * lpf_long);
    return sampleIminusDC_grid;
  }
};

/** @brief A DC-removed sample in ADC counts, at the x64 scale of the firmware. */
static int16_t x64(const int16_t counts)
{
  return static_cast< int16_t >(counts * 64);
}

/** @brief The low-pass state of the filter, in ADC counts. */
static float lowPassCounts(const CtFilter::LowPassBoost &filter)
{
  return static_cast< float >(static_cast< double >(filter.lowPass()) / (64.0 * (1L << CtFilter::LPF_FRACTION_BITS)));
}

/** @brief The shape of an input, in ADC counts. */
enum class Shape : uint8_t
{
  STEP,
  SINE,
  NOISE
};

/** @brief A 50 Hz sine of 64 samples per cycle (the single-phase rate, twice the 3-phase one), quarter wave table, 100 peak. */
static int16_t sineCounts(const uint16_t i, const int16_t dcResidual, const int16_t amplitudeFactor)
{
  static constexpr int16_t sine64[16]{ 0, 20, 39, 56, 71, 83, 92, 98, 100, 98, 92, 83, 71, 56, 39, 20 };
  const uint8_t k{ static_cast< uint8_t >(i % 64) };
  const int16_t quarter{ sine64[k % 16] };
  return static_cast< int16_t >(dcResidual + amplitudeFactor * ((k < 32) ? quarter : -quarter));
}

/** @brief Input sample i, in ADC counts; noise is a fixed linear congruential sequence. The step
 *         is sized so that the filter output stays within int16 (saturation has its own test). */
static int16_t inputCounts(const Shape shape, const uint16_t i, uint32_t &seed, const float gain)
{
  switch (shape)
  {
    case Shape::STEP:
      return (i < 100) ? 0 : static_cast< int16_t >(30000.0F / (64.0F * (1.0F + gain)));  // output within int16
    case Shape::SINE:
      return sineCounts(i, 5, 2);  // 200 counts peak, 5 counts of DC residual
    default:
      seed = seed * 1664525UL + 1013904223UL;
      return static_cast< int16_t >(static_cast< int16_t >((seed >> 16) % 301U) - 150 + 3);  // -147..153
  }
}

/** @brief Largest |fixed - float| at the output (x64) over `samples` of the input. */
static int32_t maxOutputError(const Shape shape, const float gain, const float alpha, const uint16_t samples)
{
  const CtFilter::Coefficients coefficients{ CtFilter::coefficientsOf(gain, alpha) };
  CtFilter::LowPassBoost filter;
  Robin robin;
  uint32_t seed{ 12345 };
  int32_t worst{ 0 };

  for (uint16_t i = 0; i != samples; ++i)
  {
    const int16_t counts{ inputCounts(shape, i, seed, gain) };
    const int32_t expected{ robin.apply(counts, gain, alpha) >> 2 };  // back to x64
    const int32_t actual{ filter.apply(x64(counts), coefficients) };
    const int32_t error{ (actual > expected) ? actual - expected : expected - actual };
    if (error > worst)
    {
      worst = error;
    }
  }
  return worst;
}

static void checkTracksFloat(const Shape shape, const float gain, const float alpha, const uint16_t samples)
{
  TEST_ASSERT_INT32_WITHIN(TOLERANCE, 0, maxOutputError(shape, gain, alpha, samples));
}

void test_gain_zero_is_identity(void)
{
  const CtFilter::Coefficients noGain{ CtFilter::coefficientsOf(0.0F, 0.002F) };
  CtFilter::LowPassBoost filter;
  static constexpr int16_t samples[]{ 0, 1, -1, 63, -64, 1000, -1000, 32704, -32768, 32767, 12345, -23456 };

  for (uint8_t round = 0; round != 50; ++round)
  {
    for (const int16_t sample : samples)
    {
      TEST_ASSERT_EQUAL_INT16(sample, filter.apply(sample, noGain));
    }
  }
}

void test_saturates_at_the_int16_limits(void)
{
  const CtFilter::Coefficients coefficients{ CtFilter::coefficientsOf(9.0F, 0.0011F) };
  // a DC residual of 6 counts builds up the low-pass state...
  CtFilter::LowPassBoost positive;
  CtFilter::LowPassBoost negative;
  for (uint16_t i = 0; i != 20000; ++i)
  {
    positive.apply(x64(6), coefficients);
    negative.apply(x64(-6), coefficients);
  }
  TEST_ASSERT_TRUE(positive.lowPass() > 0);
  TEST_ASSERT_TRUE(negative.lowPass() < 0);

  // ... then a current peak at full scale: beyond int16, it saturates with its sign kept
  TEST_ASSERT_EQUAL_INT16(INT16_MAX, positive.apply(x64(511), coefficients));
  TEST_ASSERT_EQUAL_INT16(INT16_MIN, negative.apply(x64(-512), coefficients));
}

void test_a_small_steady_input_moves_the_state(void)
{
  // 4 ADC counts: alpha x 4 x 256 = 1.13 > 1, the state moves (at x64 it would not: 0.28)
  const CtFilter::Coefficients coefficients{ CtFilter::coefficientsOf(9.0F, 0.0011F) };
  CtFilter::LowPassBoost filter;
  filter.apply(x64(4), coefficients);
  TEST_ASSERT_EQUAL_INT32(1, filter.lowPass());

  CtFilter::LowPassBoost negativeFilter;
  negativeFilter.apply(x64(-4), coefficients);
  TEST_ASSERT_EQUAL_INT32(-1, negativeFilter.lowPass());
}

void test_time_constant_within_2_percent_of_robins(void)
{
  // After a step, the low-pass reaches 1 - 1/e of it in -1 / ln(1 - alpha) samples
  static constexpr float alphas[]{ 0.0011F, 0.002F };
  for (const float alpha : alphas)
  {
    const CtFilter::Coefficients coefficients{ CtFilter::coefficientsOf(9.0F, alpha) };
    const float expected{ static_cast< float >(-1.0 / log(1.0 - static_cast< double >(alpha))) };

    static constexpr int16_t STEP{ 200 };  // counts
    CtFilter::LowPassBoost filter;
    uint16_t samples{ 0 };
    while (lowPassCounts(filter) < static_cast< float >(STEP * (1.0 - exp(-1.0))))
    {
      filter.apply(x64(STEP), coefficients);
      ++samples;
      TEST_ASSERT_TRUE(samples < 10000);
    }
    TEST_ASSERT_FLOAT_WITHIN(expected * 0.02F, expected, static_cast< float >(samples));
  }
}

void test_old_board_settings_track_robins_float(void)
{
  checkTracksFloat(Shape::STEP, 9.0F, 0.0011F, 6000);
  checkTracksFloat(Shape::SINE, 9.0F, 0.0011F, 64U * 100U);  // two seconds
  checkTracksFloat(Shape::NOISE, 9.0F, 0.0011F, 6000);
}

void test_shipped_alpha_tracks_robins_float(void)
{
  checkTracksFloat(Shape::STEP, 9.0F, 0.002F, 6000);
  checkTracksFloat(Shape::SINE, 9.0F, 0.002F, 64U * 100U);
  checkTracksFloat(Shape::NOISE, 9.0F, 0.002F, 6000);
}

void test_other_gains_track_robins_float(void)
{
  static constexpr float gains[]{ 0.5F, 1.0F, 4.5F, 20.0F };
  static constexpr float alphas[]{ 0.0011F, 0.002F };
  for (const float alpha : alphas)
  {
    for (const float gain : gains)
    {
      checkTracksFloat(Shape::STEP, gain, alpha, 6000);
      checkTracksFloat(Shape::SINE, gain, alpha, 64U * 100U);
      checkTracksFloat(Shape::NOISE, gain, alpha, 6000);
    }
  }
}

void test_state_settles_on_the_dc_residual_as_robins(void)
{
  const CtFilter::Coefficients coefficients{ CtFilter::coefficientsOf(9.0F, 0.0011F) };
  CtFilter::LowPassBoost filter;
  Robin robin;
  for (uint16_t i = 0; i != 64U * 50U; ++i)
  {
    const int16_t counts{ sineCounts(i, 5, 2) };
    robin.apply(counts, 9.0F, 0.0011F);
    filter.apply(x64(counts), coefficients);
  }
  TEST_ASSERT_FLOAT_WITHIN(1.0F, static_cast< float >(robin.lpf_long) / 256.0F, lowPassCounts(filter));
}

void test_valid_range_of_the_settings(void)
{
  TEST_ASSERT_TRUE(CtFilter::isAccurate(9.0F, 0.0011F));
  TEST_ASSERT_TRUE(CtFilter::isAccurate(9.0F, 0.002F));
  TEST_ASSERT_TRUE(CtFilter::isAccurate(63.0F, 0.06F));
  TEST_ASSERT_TRUE(CtFilter::isAccurate(0.0F, 0.002F));  // gain 0: nothing to filter, still a valid pair
  TEST_ASSERT_FALSE(CtFilter::isAccurate(9.0F, 0.0F));
  TEST_ASSERT_FALSE(CtFilter::isAccurate(9.0F, 0.0625F));
  TEST_ASSERT_FALSE(CtFilter::isAccurate(9.0F, 0.00001F));
  TEST_ASSERT_FALSE(CtFilter::isAccurate(64.0F, 0.002F));
  TEST_ASSERT_FALSE(CtFilter::isAccurate(-1.0F, 0.002F));
}

int main(int /* argc */, char ** /* argv */)
{
  UNITY_BEGIN();

  RUN_TEST(test_gain_zero_is_identity);
  RUN_TEST(test_saturates_at_the_int16_limits);
  RUN_TEST(test_a_small_steady_input_moves_the_state);
  RUN_TEST(test_time_constant_within_2_percent_of_robins);
  RUN_TEST(test_old_board_settings_track_robins_float);
  RUN_TEST(test_shipped_alpha_tracks_robins_float);
  RUN_TEST(test_other_gains_track_robins_float);
  RUN_TEST(test_state_settles_on_the_dc_residual_as_robins);
  RUN_TEST(test_valid_range_of_the_settings);

  return UNITY_END();
}
