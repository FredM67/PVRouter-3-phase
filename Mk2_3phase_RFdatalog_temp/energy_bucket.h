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
 * @brief Largest shift that keeps every calibration value within 16 bits.
 *
 * @param cal The power calibration values, one per phase.
 * @return The shift, i.e. the number of fractional bits of the fixed-point values.
 */
template< uint8_t N >
constexpr uint8_t calibrationShift(const float (&cal)[N])
{
  float largest{ 0.0F };
  for (const auto value : cal)
  {
    if (value > largest)
    {
      largest = value;
    }
  }

  uint8_t shift{ 0 };
  while ((shift < 31) && (largest * static_cast< float >(1UL << (shift + 1)) <= 65535.0F))
  {
    ++shift;
  }
  return shift;
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
