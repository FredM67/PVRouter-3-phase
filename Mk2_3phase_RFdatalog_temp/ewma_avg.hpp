/**
 * @file ewma_avg.hpp
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief This file implements an Exponentially Weighted Moving Average template class
 * @version 0.1
 * @date 2024-02-27
 * 
 * @section description Description
 * The Exponentially Weighted Moving Average (EWMA) is a quantitative or statistical measure used to model or describe a time series.
 * The EWMA is widely used in finance, the main applications being technical analysis and volatility modeling.
 * 
 * The moving average is designed as such that older observations are given lower weights.
 * The weights fall exponentially as the data point gets older – hence the name exponentially weighted.
 * 
 * The only decision a user of the EWMA must make is the parameter alpha.
 * The parameter decides how important the current observation is in the calculation of the EWMA.
 * The higher the value of alpha, the more closely the EWMA tracks the original time series.
 * 
 * Computation of DEMA (Double EMA) with half-alpha has been added to get a better response of the average,
 * especially when "peak inputs" are recorded.
 *
 * Computation of TEMA (Triple EMA) with quarter-alpha has been added to get a even better response of the average,
 * especially when "peak inputs" are recorded. This seems to be the "optimal" solution.
 * 
 * @section note Note
 * This class is implemented in way to use only integer math.
 * This comes with some restrictions on the alpha parameter, but the benefit of full integer math wins
 * on the side-drawback.
 *
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef EWMA_AVG_H
#define EWMA_AVG_H

#include <Arduino.h>

#include "type_traits.hpp"

/**
 * @brief Helper compile-time function to retrieve the previous power of 2 of the given number (120 => 64 => 6)
 * 
 * @param v The input number
 * @return constexpr uint8_t The next power of two
 */
constexpr uint8_t round_up_to_power_of_2(uint16_t v)
{
  if (__builtin_popcount(v) == 1) { return __builtin_ctz(v) - 1; }

  uint8_t next_pow_of_2{ 0 };

  while (v)
  {
    v >>= 1;
    ++next_pow_of_2;
  }

  return --next_pow_of_2;
}

/**
 * @brief Exponentially Weighted Moving Average
 * 
 * @details The smoothing factor is the approximate amount of values taken to calculate the average.
 *          Since the Arduino is very slow and does not provide any dedicated math co-processor,
 *          the smoothing factor will be rounded to the previous power of 2. Ie 120 will be rounded to 64.
 *          This allows to perform all the calculations with integer math, which is much faster !
 * 
 * @note    Because of the 'sign extension', the sign is copied into lower bits.
 * 
 * @tparam A Smoothing factor
 * @param input Input value
 * @return long Output value
 */
template< uint8_t A = 10 >
class EWMA_average
{
public:
  /**
   * @brief Add a new value and actualize the EMA, DEMA and TEMA
   * 
   * @param input The new value
   */
  void addValue(int32_t input)
  {
    ema_raw = ema_raw - ema + input;
    ema = ema_raw >> round_up_to_power_of_2(A);

    ema_ema_raw = ema_ema_raw - ema_ema + ema;
    ema_ema = ema_ema_raw >> (round_up_to_power_of_2(A) - 1);

    ema_ema_ema_raw = ema_ema_ema_raw - ema_ema_ema + ema_ema;
    ema_ema_ema = ema_ema_ema_raw >> (round_up_to_power_of_2(A) - 2);
  }

  /**
   * @brief Get the EMA
   * 
   * @return auto The EMA value
   */
  auto getAverageS() const
  {
    return ema;
  }

  /**
   * @brief Get the DEMA
   * 
   * @return auto The DEMA value
   */
  auto getAverageD() const
  {
    return (ema << 1) - ema_ema;
  }

  /**
   * @brief Get the TEMA
   * 
   * @return auto The TEMA value
   */
  auto getAverageT() const
  {
    return 3 * (ema - ema_ema) + ema_ema_ema;
  }

private:
  int32_t ema_ema_ema_raw{ 0 };
  int32_t ema_ema_ema{ 0 };
  int32_t ema_ema_raw{ 0 };
  int32_t ema_ema{ 0 };
  int32_t ema_raw{ 0 };
  int32_t ema{ 0 };
};

#endif