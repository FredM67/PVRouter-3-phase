/**
 * @file datalog_math.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Integer arithmetic of the datalog: power and RMS voltage per phase
 * @version 0.1
 * @date 2026-11-13
 *
 * @copyright Copyright (c) 2026-2026
 *
 * @details At the end of each datalog period, the main loop turns the sums of the ISR into
 *          watts and volts. This used to be float arithmetic (a multiplication by f_powerCal,
 *          sqrt() and a multiplication by f_voltageCal), which linked about 750 bytes of
 *          libgcc/libm float code and kept the calibration arrays in RAM (#120).
 *
 *          The calibration values stay floats in calibration.h. They are turned into 16-bit
 *          fixed-point constants at compile time (Energy::toFixed()), and the square root is
 *          an integer one, computed on a normalised value so that it keeps 15 to 16
 *          significant bits.
 *
 * @note This header has no Arduino dependency, so it can be exercised by the native
 *       test suite.
 */

#ifndef DATALOG_MATH_H
#define DATALOG_MATH_H

#include <stdint.h>

#include "energy_bucket.h"
#include "mult_asm.h"

namespace Datalog
{
/**
 * @brief The average power of a phase, in watts.
 *
 * @param averagePower The phase's average power over the datalog period, in V_ADC x I_ADC
 *                     units (sumP / sample sets, at most +-2^18).
 * @param calFixed The phase's power calibration, from Energy::toFixed().
 * @param shift The shift returned by Energy::calibrationShift() (>= 13 for any valid
 *              calibration, see Energy::isValidCalibration()).
 * @return averagePower x f_powerCal, rounded half away from zero, saturated to +-32767 W.
 */
inline int16_t watts(const int32_t averagePower, const uint16_t calFixed, const uint8_t shift)
{
  const bool negative{ averagePower < 0 };
  const uint32_t magnitude{ static_cast< uint32_t >(negative ? -averagePower : averagePower) };

  uint32_t product;  // (magnitude x calFixed) >> 8, at most 2^26
  multU24x16_to32_hi8(product, magnitude, calFixed);

  const uint8_t remaining{ static_cast< uint8_t >(shift - 8) };
  uint32_t rounded{ (product + (static_cast< uint32_t >(1) << (remaining - 1))) >> remaining };
  if (rounded > INT16_MAX)
  {
    rounded = INT16_MAX;
  }

  const auto result{ static_cast< int16_t >(rounded) };
  return negative ? static_cast< int16_t >(-result) : result;
}

/**
 * @brief Integer square root, rounded to the nearest integer.
 *
 * @param value The radicand.
 * @return round(sqrt(value)), at most 65535.
 */
inline uint16_t isqrt(uint32_t value)
{
  uint32_t root{ 0 };
  uint32_t bit{ 1UL << 30 };  // the highest power of 4 within 32 bits

  while (bit > value)
  {
    bit >>= 2;
  }
  while (bit)
  {
    if (value >= root + bit)
    {
      value -= root + bit;
      root = (root >> 1) + bit;
    }
    else
    {
      root >>= 1;
    }
    bit >>= 2;
  }
  // value is now the remainder, x - root^2: round up when x >= (root + 0.5)^2 = root^2 + root + 0.25
  if ((value > root) && (root < UINT16_MAX))
  {
    ++root;
  }
  return static_cast< uint16_t >(root);
}

/**
 * @brief The square root of a mean square, with as many fractional bits as fit.
 *
 * @param meanSquare The mean of V_ADC^2 (in the scaling of the ISR).
 * @param[out] fractionBits The number of fractional bits of the result.
 * @return sqrt(meanSquare) x 2^fractionBits, rounded; >= 2^15 unless meanSquare is 0.
 */
inline uint16_t sqrtScaled(uint32_t meanSquare, uint8_t& fractionBits)
{
  fractionBits = 0;
  if (meanSquare)
  {
    while (meanSquare < (1UL << 30))  // each step of 2 bits adds one bit to the root
    {
      meanSquare <<= 2;
      ++fractionBits;
    }
  }
  return isqrt(meanSquare);
}

/**
 * @brief Largest shift that keeps every (factor x calibration value) within 16 bits.
 *
 * @param factor The scaling of the result, e.g. 100 for hundredths of a volt.
 * @param cal The voltage calibration values, one per phase.
 */
template< uint8_t N >
constexpr uint8_t voltageShift(const float factor, const float (&cal)[N])
{
  return Energy::shiftFor(factor * Energy::largestOf(cal));
}

/**
 * @brief Check that the voltage calibration values can be represented.
 *
 * @details Every value must be positive, and the shift small enough for vrms(): with up to
 *          15 fractional bits from sqrtScaled(), the total shift must stay below 32.
 */
template< uint8_t N >
constexpr bool isValidVoltageCalibration(const float factor, const float (&cal)[N])
{
  for (const auto value : cal)
  {
    if (!(value > 0.0F))
    {
      return false;
    }
  }
  return voltageShift(factor, cal) <= 16;
}

/**
 * @brief The RMS voltage of a phase.
 *
 * @param meanSquare The mean of V_ADC^2 over the datalog period (sum_Vsquared / sample sets).
 * @param calFixed The phase's factor x f_voltageCal, from Energy::toFixed().
 * @param shift The shift returned by voltageShift() (<= 16).
 * @return factor x f_voltageCal x sqrt(meanSquare), rounded, saturated to 65535.
 */
inline uint16_t vrms(const uint32_t meanSquare, const uint16_t calFixed, const uint8_t shift)
{
  uint8_t fractionBits;
  const uint16_t root{ sqrtScaled(meanSquare, fractionBits) };

  uint32_t product;
  multU16x16_to32(product, root, calFixed);

  // rounded shift by (fractionBits + shift), without overflowing the 32 bits of product
  const uint8_t total{ static_cast< uint8_t >(fractionBits + shift) };
  if (total)
  {
    product = ((product >> (total - 1)) + 1) >> 1;
  }
  return product > UINT16_MAX ? UINT16_MAX : static_cast< uint16_t >(product);
}

/**
 * @brief Calibration values of all phases in 16-bit fixed point, with their shared shift.
 */
template< uint8_t N >
struct FixedCalibration
{
  uint16_t value[N]; /**< round(cal x 2^shift), one per phase */
  uint8_t shift;     /**< shared by all phases */
};

/**
 * @brief The power calibration in fixed point, for watts(), built at compile time.
 *
 * @param cal f_powerCal, one value per phase.
 */
template< uint8_t N >
constexpr FixedCalibration< N > powerCalibration(const float (&cal)[N])
{
  FixedCalibration< N > fixed{};
  fixed.shift = Energy::calibrationShift(cal);
  for (uint8_t phase = 0; phase < N; ++phase)
  {
    fixed.value[phase] = Energy::toFixed(cal[phase], fixed.shift);
  }
  return fixed;
}

/**
 * @brief The voltage calibration in fixed point, for vrms(), built at compile time.
 *
 * @param factor The scaling of the result (100 for hundredths of a volt, x4 when the ISR
 *               scales V^2 down by 16).
 * @param cal f_voltageCal, one value per phase.
 */
template< uint8_t N >
constexpr FixedCalibration< N > voltageCalibration(const float factor, const float (&cal)[N])
{
  FixedCalibration< N > fixed{};
  fixed.shift = voltageShift(factor, cal);
  for (uint8_t phase = 0; phase < N; ++phase)
  {
    fixed.value[phase] = Energy::toFixed(factor * cal[phase], fixed.shift);
  }
  return fixed;
}
}  // namespace Datalog

#endif /* DATALOG_MATH_H */
