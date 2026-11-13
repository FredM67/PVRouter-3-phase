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

#ifdef __AVR__
using uint24_t = __uint24; /**< 3 bytes on AVR: the remainder and twice the root of the square root */
#else
using uint24_t = uint32_t;
#endif

#ifdef __AVR__
// The assembly of squareRoot(), as string literals to concatenate.

/** Shifts [val] (not 0) up to >= 2^30, counting the bits of the root gained in [shift]: whole
 *  bytes first (byte moves, 4 bits of the root each), then pairs of bits (1 bit each). */
#define DATALOG_SQRT_NORMALISE \
  "tst  %D[val]             \n\t" \
  "brne 6f                  \n\t" \
  "5:                       \n\t" \
  "mov  %D[val], %C[val]    \n\t" \
  "mov  %C[val], %B[val]    \n\t" \
  "mov  %B[val], %A[val]    \n\t" \
  "clr  %A[val]             \n\t" \
  "subi %[shift], -4        \n\t" \
  "tst  %D[val]             \n\t" \
  "breq 5b                  \n\t" \
  "6:                       \n\t" \
  "cpi  %D[val], 0x40       \n\t" \
  "brsh 7f                  \n\t" \
  "lsl  %A[val]             \n\t" \
  "rol  %B[val]             \n\t" \
  "rol  %C[val]             \n\t" \
  "rol  %D[val]             \n\t" \
  "lsl  %A[val]             \n\t" \
  "rol  %B[val]             \n\t" \
  "rol  %C[val]             \n\t" \
  "rol  %D[val]             \n\t" \
  "inc  %[shift]            \n\t" \
  "rjmp 6b                  \n\t" \
  "7:                       \n\t"

/** The 16 digits of the root: for each byte of [val], most significant first, for each pair of
 *  bits of the byte, bring the pair down into [rem], double the root, and try 2 x root + 1. */
#define DATALOG_SQRT_DIGITS \
  "clr  %A[rem]             \n\t" \
  "clr  %B[rem]             \n\t" \
  "clr  %C[rem]             \n\t" \
  "clr  %A[twice]           \n\t" \
  "clr  %B[twice]           \n\t" \
  "clr  %C[twice]           \n\t" \
  "ldi  %[bytes], 4         \n\t" \
  "1:                       \n\t" \
  "ldi  %[bits], 4          \n\t" \
  "2:                       \n\t" \
  "lsl  %D[val]             \n\t" \
  "rol  %A[rem]             \n\t" \
  "rol  %B[rem]             \n\t" \
  "rol  %C[rem]             \n\t" \
  "lsl  %D[val]             \n\t" \
  "rol  %A[rem]             \n\t" \
  "rol  %B[rem]             \n\t" \
  "rol  %C[rem]             \n\t" \
  "lsl  %A[twice]           \n\t" \
  "rol  %B[twice]           \n\t" \
  "rol  %C[twice]           \n\t" \
  "cp   %A[twice], %A[rem]  \n\t" \
  "cpc  %B[twice], %B[rem]  \n\t" \
  "cpc  %C[twice], %C[rem]  \n\t" \
  "brcc 3f                  \n\t" \
  "sec                      \n\t" \
  "sbc  %A[rem], %A[twice]  \n\t" \
  "sbc  %B[rem], %B[twice]  \n\t" \
  "sbc  %C[rem], %C[twice]  \n\t" \
  "ori  %A[twice], 2        \n\t" \
  "3:                       \n\t" \
  "dec  %[bits]             \n\t" \
  "brne 2b                  \n\t" \
  "mov  %D[val], %C[val]    \n\t" \
  "mov  %C[val], %B[val]    \n\t" \
  "mov  %B[val], %A[val]    \n\t" \
  "dec  %[bytes]            \n\t" \
  "brne 1b                  \n\t"

#define DATALOG_SQRT_OPERANDS \
  [rem] "=&r"(remainder), [twice] "=&d"(twiceRoot), [val] "+d"(value), [bytes] "=&d"(bytes), [bits] "=&d"(bits), [shift] "+d"(shift)
#endif

/**
 * @brief Square root, rounded to the nearest integer, of the value or of the value normalised.
 *
 * @details Digit by digit, two bits of the value per bit of the root, most significant first.
 *          The remainder (value - root^2 so far, < 2^19 before each trial) and twice the root
 *          (< 2^17) fit in 3 bytes; keeping twice the root makes each trial a single compare:
 *          remainder >= 2 x (2 x root) + 1  <=>  remainder > 2 x (2 x root).
 *
 *          On AVR, this is assembly: the value is consumed a byte at a time, each bit of the root
 *          costs ~21 cycles, and the normalisation ~6 cycles per byte and ~12 per pair of bits
 *          (avr-gcc's code for the same needs about twice as much; see test_datalog_math_avr).
 *
 * @tparam Normalise false: round(sqrt(value)). true: the value is first shifted left by pairs of
 *                   bits up to >= 2^30, so that the root has 15 to 16 significant bits.
 * @param value The value to take the square root of.
 * @param[out] fractionBits The number of fractional bits of the result (0 if not normalised).
 * @return round(sqrt(value) x 2^fractionBits), at most 65535.
 */
template< bool Normalise >
inline uint16_t squareRoot(uint32_t value, uint8_t& fractionBits)
{
  uint8_t shift{ 0 };
  uint24_t remainder;
  uint24_t twiceRoot;

  if (Normalise && !value)
  {
    fractionBits = 0;
    return 0;
  }

#ifdef __AVR__
  uint8_t bytes;
  uint8_t bits;
  if constexpr (Normalise)
  {
    asm(DATALOG_SQRT_NORMALISE DATALOG_SQRT_DIGITS:DATALOG_SQRT_OPERANDS : : "cc");
  }
  else
  {
    asm(DATALOG_SQRT_DIGITS:DATALOG_SQRT_OPERANDS : : "cc");
  }
#else
  if constexpr (Normalise)
  {
    while (value < (1UL << 24))  // whole bytes, 4 bits of the root each
    {
      value <<= 8;
      shift += 4;
    }
    while (value < (1UL << 30))  // then pairs of bits, 1 bit of the root each
    {
      value <<= 2;
      ++shift;
    }
  }
  remainder = 0;
  twiceRoot = 0;
  for (uint8_t step = 16; step; --step)
  {
    remainder = (remainder << 2) | (value >> 30);  // bring down the next two bits
    value <<= 2;
    twiceRoot <<= 1;            // root <<= 1
    if (remainder > twiceRoot)  // remainder >= 2 x root + 1
    {
      remainder -= twiceRoot + 1;
      twiceRoot |= 2;  // root |= 1
    }
  }
#endif

  fractionBits = shift;
  auto root{ static_cast< uint16_t >(twiceRoot >> 1) };
  // remainder = value - root^2: round up when value >= (root + 0.5)^2 = root^2 + root + 0.25
  if ((remainder > root) && (root < UINT16_MAX))
  {
    ++root;
  }
  return root;
}

#ifdef __AVR__
#undef DATALOG_SQRT_NORMALISE
#undef DATALOG_SQRT_DIGITS
#undef DATALOG_SQRT_OPERANDS
#endif

/**
 * @brief Integer square root, rounded to the nearest integer.
 *
 * @param value The value to take the square root of.
 * @return round(sqrt(value)), at most 65535.
 */
inline uint16_t isqrt(const uint32_t value)
{
  uint8_t fractionBits;
  return squareRoot< false >(value, fractionBits);
}

/**
 * @brief The square root of a mean square, with as many fractional bits as fit.
 *
 * @param meanSquare The mean of V_ADC^2 (in the scaling of the ISR).
 * @param[out] fractionBits The number of fractional bits of the result.
 * @return sqrt(meanSquare) x 2^fractionBits, rounded; >= 2^15 unless meanSquare is 0.
 */
inline uint16_t sqrtScaled(const uint32_t meanSquare, uint8_t& fractionBits)
{
  return squareRoot< true >(meanSquare, fractionBits);
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

  // rounded shift by (fractionBits + shift), without overflowing the 32 bits of product:
  // whole bytes first (byte moves on AVR), then the remaining bits one by one
  uint8_t total{ static_cast< uint8_t >(fractionBits + shift) };
  if (total)
  {
    --total;  // the last bit is shifted out after the rounding
    if (total >= 16)
    {
      product >>= 16;
      total -= 16;
    }
    if (total >= 8)
    {
      product >>= 8;
      total -= 8;
    }
    product = ((product >> total) + 1) >> 1;
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
