/**
 * @file ct_filter.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The CT filter: a low-pass of the current added back with a gain, one per phase
 * @version 1.0
 * @date 2026-11-22
 *
 * @copyright Copyright (c) 2026-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details A current transformer acts as a high-pass filter. Robin's sketches offset it with
 *          an IIR low-pass filter of the current, added back with a gain (lpf_gain, alpha in
 *          calibration.h). This is his filter, Mk2_fasterControl_Full/processing.cpp, in fixed
 *          point instead of his float maths (two float multiplies and four conversions took
 *          ~1100 cycles of the ADC interrupt, per phase; this takes ~210):
 *          - his state, at x256 scale (one ADC count = 256): it moves as soon as alpha x the
 *            difference reaches one unit (3.6 ADC counts with alpha 0.0011);
 *          - his update, lpf += alpha x (sample - lpf), truncated towards zero as his float to
 *            int32 conversion does. That truncation holds his state up to 0.5 / alpha units
 *            short of the input's mean (1.8 counts with alpha 0.0011, 16 counts at the output
 *            with a gain of 9): the filter the old boards were tuned with, kept as it is;
 *          - alpha and the gain turned into fixed point at compile time (coefficientsOf()),
 *            from the float settings of calibration.h: alpha x 2^(16 + shift), gain x 2^8.
 *
 *          The firmware's sample is DC-removed and left-aligned (x64 scale, int16): it is
 *          filtered at x256, the output is back at x64, saturated to the int16 range, so that a
 *          peak beyond full scale cannot wrap and flip the sign of the power.
 *
 *          Pure C++, no Arduino: the native tests include it.
 */

#ifndef CT_FILTER_H
#define CT_FILTER_H

#include <stdint.h>

#include "mult_asm.h"

namespace CtFilter
{
/** @brief The fractional bits of the low-pass state below the x64 scale of the samples: x256. */
inline constexpr uint8_t LPF_FRACTION_BITS{ 2 };

/** @brief The largest extra shift of alpha: more resolution for a small alpha, one more turn each. */
inline constexpr uint8_t MAX_ALPHA_SHIFT{ 4 };

/**
 * @brief alpha x 2^(16 + shift) stays below this: 4 x alpha fits an int16, and 4 x alpha x the
 *        largest difference (2^16 at x64) an int32 with 2 bits to spare.
 */
inline constexpr int16_t ALPHA_FIXED_LIMIT{ 4096 };

/** @brief The coefficients of the filter, in fixed point: the multiples the filter needs, ready. */
struct Coefficients
{
  int16_t alpha;       /**< alpha x 2^(16 + alphaShift) */
  int16_t alphaX2;     /**< 2 x alpha */
  int16_t alphaX4;     /**< 4 x alpha */
  uint8_t alphaShift;  /**< the extra resolution of alpha */
  int16_t gain;        /**< gain x 2^8 */
  int16_t gainHalf;    /**< gain x 2^7 */
  int16_t gainQuarter; /**< gain x 2^6 */
};

/**
 * @brief Converts the float settings to fixed point, at compile time.
 *
 * @param gain The gain of the low-pass added back (lpf_gain), 0 to below 64.
 * @param alpha The coefficient of the low-pass, above 0 to below 0.0625.
 * @return The coefficients, alpha with the most resolution (up to MAX_ALPHA_SHIFT extra bits).
 */
constexpr Coefficients coefficientsOf(const float gain, const float alpha)
{
  uint8_t shift{ MAX_ALPHA_SHIFT };
  while ((shift != 0) && (alpha * static_cast< float >(1UL << (16 + shift)) + 0.5F >= static_cast< float >(ALPHA_FIXED_LIMIT)))
  {
    --shift;
  }
  const int16_t alphaFixed{ static_cast< int16_t >(alpha * static_cast< float >(1UL << (16 + shift)) + 0.5F) };
  return { alphaFixed,
           static_cast< int16_t >(2 * alphaFixed),
           static_cast< int16_t >(4 * alphaFixed),
           shift,
           static_cast< int16_t >(gain * 256.0F + 0.5F),
           static_cast< int16_t >(gain * 128.0F + 0.5F),
           static_cast< int16_t >(gain * 64.0F + 0.5F) };
}

/**
 * @brief Whether the settings fit the fixed point, the coefficients within 1 % of them.
 *
 * @details alpha below 0.0625 and the gain below 64, so that nothing overflows; alpha above
 *          about 0.00005, below which its 20 fractional bits are too coarse for its time constant.
 */
constexpr bool isAccurate(const float gain, const float alpha)
{
  if (!(alpha > 0.0F) || !(alpha < 0.0625F) || !(gain >= 0.0F) || !(gain < 64.0F))
  {
    return false;
  }
  const Coefficients coefficients{ coefficientsOf(gain, alpha) };
  const float fixedAlpha{ static_cast< float >(coefficients.alpha) / static_cast< float >(1UL << (16 + coefficients.alphaShift)) };
  const float fixedGain{ static_cast< float >(coefficients.gain) / 256.0F };
  const float alphaError{ fixedAlpha > alpha ? fixedAlpha - alpha : alpha - fixedAlpha };
  const float gainError{ fixedGain > gain ? fixedGain - gain : gain - fixedGain };
  return (coefficients.alpha < ALPHA_FIXED_LIMIT) && (alphaError <= alpha * 0.01F) && (gainError <= gain * 0.01F);
}

/**
 * @brief The state of the filter of one CT (one per phase), and the filter itself.
 *
 * @details The state is Robin's lpf_long (x256), kept as its x64 part and its 2 low bits:
 *          lpf_long = 4 x lpfX64 + lpfLowBits. Every multiply is then 16 x 16, and no shift of
 *          the state needs a loop.
 */
struct LowPassBoost
{
  int16_t lpfX64{};     /**< low-pass of the current, x64 scale (lpf_long >> 2) */
  uint8_t lpfLowBits{}; /**< its 2 bits below, 0..3 (lpf_long & 3) */

  /** @brief The low-pass state, x256 scale (Robin's lpf_long). */
  int32_t lowPass() const
  {
    return 4 * static_cast< int32_t >(lpfX64) + lpfLowBits;
  }

  /**
   * @brief Filters one sample.
   *
   * @details As Robin's, at x256: lpf += alpha x (sample - lpf), truncated towards zero;
   *          then sample + gain x lpf, back to x64.
   *
   * @param sampleMinusDC The DC-removed current sample, x64 scale (left-aligned ADC).
   * @param coefficients The coefficients, from coefficientsOf(); a gain of 0 returns the sample.
   * @return The filtered sample, x64 scale, saturated to the int16 range.
   */
  int16_t apply(const int16_t sampleMinusDC, const Coefficients &coefficients)
  {
    // alpha x (4 x sample - lpf), in units of 2^-(16 + shift) at x256:
    // 4 x alpha x sample - 4 x alpha x lpfX64 - alpha x lpfLowBits
    int32_t alphaSample;
    int32_t alphaLpf;
    multS16x16_to32(alphaSample, sampleMinusDC, coefficients.alphaX4);
    multS16x16_to32(alphaLpf, lpfX64, coefficients.alphaX4);
    int32_t increment{ alphaSample - alphaLpf };  // below 2^30
    if (lpfLowBits & 1)
    {
      increment -= coefficients.alpha;
    }
    if (lpfLowBits & 2)
    {
      increment -= coefficients.alphaX2;
    }

    // lpf + increment / 2^(16 + shift), truncated towards zero: floored, plus one when the
    // result is negative with a fraction dropped
    const uint8_t shift{ coefficients.alphaShift };
    const int16_t incrementHigh{ static_cast< int16_t >(increment >> 16) };  // a word move, below 2^14
    const bool fractionDropped{ (static_cast< uint16_t >(increment) != 0) || ((incrementHigh & ((1 << shift) - 1)) != 0) };
    const int16_t sum{ static_cast< int16_t >(lpfLowBits + (incrementHigh >> shift)) };
    lpfX64 += sum >> 2;  // floored
    lpfLowBits = static_cast< uint8_t >(sum & 3);
    if ((lpfX64 < 0) && fractionDropped)
    {
      if (++lpfLowBits == 4)
      {
        lpfLowBits = 0;
        ++lpfX64;
      }
    }

    // gain x lpf at x64, x 2^8: gain x 2^8 x lpfX64 + gain x 2^6 x lpfLowBits
    int32_t boost;
    multS16x16_to32(boost, lpfX64, coefficients.gain);  // below 2^29: the gain below 64
    if (lpfLowBits & 1)
    {
      boost += coefficients.gainQuarter;
    }
    if (lpfLowBits & 2)
    {
      boost += coefficients.gainHalf;
    }

    const int32_t sample{ static_cast< int32_t >(sampleMinusDC) + (boost >> 8) };  // a byte move

    if (sample > INT16_MAX)
    {
      return INT16_MAX;
    }
    if (sample < INT16_MIN)
    {
      return INT16_MIN;
    }
    return static_cast< int16_t >(sample);
  }
};
}  // namespace CtFilter

#endif  // CT_FILTER_H
