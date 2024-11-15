/**
 * @file utils_dualtariff.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions for dual tariff feature
 * @version 0.1
 * @date 2023-05-01
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef UTILS_DUALTARIFF_H
#define UTILS_DUALTARIFF_H

#include <Arduino.h>

/** @brief Config parameters for overriding a load
 *  @details This class allows the user to define when and how long a load will be forced at
 *           full power during off-peak period.
 *
 *           For each load, the user defines a pair of values: pairForceLoad => { offset, duration }.
 *           The load will be started with full power at ('start_offpeak' + 'offset') for a duration of 'duration'
 *             - all values are in hours (if between -24 and 24) or in minutes.
 *             - if the offset is negative, it's calculated from the end of the off-peak period (ie -3 means 3 hours back from the end).
 *             - to leave the load at full power till the end of the off-peak period, set the duration to 'UINT16_MAX' (somehow infinite time)
 */
class pairForceLoad
{
public:
  constexpr pairForceLoad() = default;
  explicit constexpr pairForceLoad(int16_t _iStartOffset)
    : iStartOffset(_iStartOffset), uiDuration(UINT16_MAX)
  {
  }
  constexpr pairForceLoad(int16_t _iStartOffset, uint16_t _uiDuration)
    : iStartOffset(_iStartOffset), uiDuration(_uiDuration)
  {
  }

  [[nodiscard]] constexpr int16_t getStartOffset() const
  {
    return iStartOffset;
  }
  [[nodiscard]] constexpr uint16_t getDuration() const
  {
    return uiDuration;
  }

private:
  int16_t iStartOffset{ 0 };         /**< the start offset from the off-peak begin in hours or minutes */
  uint16_t uiDuration{ UINT16_MAX }; /**< the duration for overriding the load in hours or minutes */
};

#endif  // UTILS_DUALTARIFF_H
