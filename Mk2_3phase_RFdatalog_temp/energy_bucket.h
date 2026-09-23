/**
 * @file energy_bucket.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Integer arithmetic of the energy bucket
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026-2026
 *
 * @details The energy bucket used to be a float, which put libgcc floating-point calls
 *          in the ADC ISR. It is now an int32_t in fixed-point units:
 *          1 unit = 1 / 2^FRACTION_BITS W x mains cycle.
 *
 *          The power calibration values stay floats in calibration.h. They are turned
 *          into 16-bit fixed-point constants at compile time, with a shift chosen so
 *          that the largest of them fills the 16 bits - the quantisation error then
 *          stays far below the accuracy of the calibration itself.
 *
 * @note This header has no Arduino dependency, so it can be exercised by the native
 *       test suite.
 */

#ifndef ENERGY_BUCKET_H
#define ENERGY_BUCKET_H

#include <stdint.h>

#include "mult_asm.h"

namespace Energy
{
inline constexpr uint8_t FRACTION_BITS{ 4 }; /**< fractional bits of a bucket unit */

/**
 * @brief Largest shift that keeps a value within 16 bits once scaled by 2^shift.
 */
constexpr uint8_t shiftFor(const float largest)
{
  uint8_t shift{ 0 };
  while ((shift < 31) && (largest * static_cast< float >(1UL << (shift + 1)) <= 65535.0F))
  {
    ++shift;
  }
  return shift;
}

/**
 * @brief Largest of the calibration values.
 */
template< uint8_t N >
constexpr float largestOf(const float (&cal)[N])
{
  float largest{ 0.0F };
  for (const auto value : cal)
  {
    if (value > largest)
    {
      largest = value;
    }
  }
  return largest;
}

/**
 * @brief Largest shift that keeps every calibration value within 16 bits.
 *
 * @param cal The power calibration values, one per phase.
 * @return The shift, i.e. the number of fractional bits of the fixed-point values.
 */
template< uint8_t N >
constexpr uint8_t calibrationShift(const float (&cal)[N])
{
  return shiftFor(largestOf(cal));
}

/**
 * @brief A calibration value as a 16-bit fixed-point constant.
 *
 * @param cal The calibration value.
 * @param shift The shift returned by calibrationShift().
 * @return round(cal x 2^shift).
 */
constexpr uint16_t toFixed(const float cal, const uint8_t shift)
{
  // scaling by a power of two is exact in float, so only the final rounding matters
  return static_cast< uint16_t >(cal * static_cast< float >(1UL << shift) + 0.5F);
}

/**
 * @brief cal / n for every phase and every expected sample count n, in fixed point.
 *
 * @details One cycle's contribution is (sumP / n) x cal = sumP x (cal / n). With this
 *          table the ISR multiplies sumP directly, with no division - and without the
 *          truncation of that division either.
 *
 * @tparam NMin Smallest expected number of sample sets per mains cycle.
 * @tparam NMax Largest expected number of sample sets per mains cycle (<= 64, so that
 *              sumP stays within the 24 bits contribution() expects).
 * @tparam N Number of phases.
 */
template< uint8_t NMin, uint8_t NMax, uint8_t N >
struct PerSampleCalibration
{
  uint16_t value[N][NMax - NMin + 1]; /**< round(cal / n x 2^shift), indexed [phase][n - NMin] */
  uint8_t shift;                      /**< shared by the whole table */
};

/**
 * @brief Build the per-sample calibration table at compile time.
 *
 * @param cal The power calibration values, one per phase.
 */
template< uint8_t NMin, uint8_t NMax, uint8_t N >
constexpr PerSampleCalibration< NMin, NMax, N > toFixedPerSample(const float (&cal)[N])
{
  static_assert((NMin >= 1) && (NMin <= NMax), "empty range of sample counts");
  static_assert(NMax <= 64, "sumP must stay within 24 bits: at most 64 sample sets per cycle");

  PerSampleCalibration< NMin, NMax, N > table{};
  table.shift = shiftFor(largestOf(cal) / NMin);  // the largest entry: largest cal, smallest n
  for (uint8_t phase = 0; phase < N; ++phase)
  {
    for (uint8_t n = NMin; n <= NMax; ++n)
    {
      table.value[phase][n - NMin] = toFixed(cal[phase] / n, table.shift);
    }
  }
  return table;
}

/**
 * @brief Check that the calibration values can be represented.
 *
 * @details Every value must be positive, and the largest must leave enough fractional
 *          bits for contribution(): shift >= FRACTION_BITS + 9, i.e. values below ~8.
 *
 * @param cal The power calibration values, one per phase.
 */
template< uint8_t N >
constexpr bool isValidCalibration(const float (&cal)[N])
{
  for (const auto value : cal)
  {
    if (!(value > 0.0F))
    {
      return false;
    }
  }
  return calibrationShift(cal) >= FRACTION_BITS + 9;
}

/**
 * @brief Contribution of one mains cycle of one phase to the bucket.
 *
 * @param averagePower The phase's average power over the cycle, in V_ADC x I_ADC units
 *                     (at most +-2^18).
 * @param calFixed The phase's calibration value, from toFixed().
 * @param shift The shift returned by calibrationShift().
 * @return averagePower x cal, in bucket units, rounded to the nearest unit.
 */
inline int32_t contribution(const int32_t averagePower, const uint16_t calFixed, const uint8_t shift)
{
  // Work on the magnitude and apply the sign last: rounding is then half away from
  // zero, and import and export weigh exactly the same.
  const bool negative{ averagePower < 0 };
  const uint32_t magnitude{ static_cast< uint32_t >(negative ? -averagePower : averagePower) };

  uint32_t product;  // (magnitude x calFixed) >> 8
  multU24x16_to32_hi8(product, magnitude, calFixed);

  // remaining shift down to bucket units; >= 1 for any calibration value below 8
  const uint8_t remaining{ static_cast< uint8_t >(shift - FRACTION_BITS - 8) };
  const auto rounded{ static_cast< int32_t >((product + (1UL << (remaining - 1))) >> remaining) };

  return negative ? -rounded : rounded;
}

/**
 * @brief A power in watts, in bucket units per mains cycle.
 */
constexpr int32_t fromWatts(const int32_t watts)
{
  return watts * (1L << FRACTION_BITS);
}
}  // namespace Energy

#endif  // ENERGY_BUCKET_H
