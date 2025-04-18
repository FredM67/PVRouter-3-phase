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
 * @class EWMA_average
 * @brief Implements an Exponentially Weighted Moving Average (EWMA).
 *
 * The `EWMA_average` class calculates the Exponentially Weighted Moving Average (EMA),
 * Double EMA (DEMA), and Triple EMA (TEMA) for a given input series. It uses integer
 * math for efficiency, making it suitable for systems with limited computational power.
 *
 * @tparam A The smoothing factor, which determines the weight of recent values in the average.
 *
 * @details
 * - The smoothing factor is rounded to the nearest power of 2 for faster integer calculations.
 * - EMA provides a smoothed average of the input series.
 * - DEMA and TEMA offer improved responsiveness, especially for peak inputs.
 * - The class is optimized for use in embedded systems like Arduino.
 *
 * @ingroup GeneralProcessing
 */
template< uint8_t A = 10 >
class EWMA_average
{
public:
  /**
   * @brief Add a new value and update the EMA, DEMA, and TEMA.
   *
   * This method processes a new input value and updates the Exponentially Weighted
   * Moving Average (EMA), Double EMA (DEMA), and Triple EMA (TEMA).
   *
   * @param input The new input value to process.
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
   * @brief Get the Exponentially Weighted Moving Average (EMA).
   *
   * @return auto The EMA value.
   */
  auto getAverageS() const
  {
    return ema;
  }

  /**
   * @brief Get the Double Exponentially Weighted Moving Average (DEMA).
   *
   * @return auto The DEMA value.
   */
  auto getAverageD() const
  {
    return (ema << 1) - ema_ema;
  }

  /**
   * @brief Get the Triple Exponentially Weighted Moving Average (TEMA).
   *
   * @return auto The TEMA value.
   */
  auto getAverageT() const
  {
    return 3 * (ema - ema_ema) + ema_ema_ema;
  }

private:
  int32_t ema_ema_ema_raw{ 0 }; /**< Raw value for TEMA calculation. */
  int32_t ema_ema_ema{ 0 };     /**< TEMA value. */
  int32_t ema_ema_raw{ 0 };     /**< Raw value for DEMA calculation. */
  int32_t ema_ema{ 0 };         /**< DEMA value. */
  int32_t ema_raw{ 0 };         /**< Raw value for EMA calculation. */
  int32_t ema{ 0 };             /**< EMA value. */
};

#endif /* EWMA_AVG_H */
