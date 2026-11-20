/**
 * @file utils_dualtariff.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions for dual tariff feature
 * @version 0.2
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2023-2026
 *
 * @note This header has no Arduino dependency, so it can be exercised by the native test
 *       suite (test/native/test_dual_tariff).
 */

#ifndef UTILS_DUALTARIFF_H
#define UTILS_DUALTARIFF_H

#include <stdint.h>

/** @brief Config parameters for overriding a load
 *  @details This class allows the user to define when and how long a load will be forced at
 *           full power during off-peak period.
 *
 *           For each load, the user defines a pair of values: pairForceLoad => { offset, duration }.
 *           The load will be started with full power at ('start_offpeak' + 'offset') for a duration of 'duration'
 *             - all values are in hours (if between -24 and 24) or in minutes.
 *             - if the offset is negative, it's calculated from the end of the off-peak period (ie -3 means 3 hours back from the end).
 *             - to leave the load at full power till the end of the off-peak period, set the duration to 'UINT16_MAX' (somehow infinite time)
 *
 *           A default pair, as for the loads left out of rg_ForceLoad, is { 0, UINT16_MAX }:
 *           the whole off-peak period. { 0, 0 } is no scheduled boost.
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

/**
 * @brief The forcing window of each load, in milliseconds from the start of the off-peak period.
 *
 * @ingroup DualTariff
 */
namespace DualTariff
{
/** @brief An offset beyond +-24 is in minutes, in hours otherwise. */
constexpr bool offsetInMinutes(const pairForceLoad& force)
{
  return force.getStartOffset() > 24 || force.getStartOffset() < -24;
}

/** @brief A duration beyond 24 is in minutes (but UINT16_MAX: until the end), in hours otherwise. */
constexpr bool durationInMinutes(const pairForceLoad& force)
{
  return force.getDuration() > 24 && UINT16_MAX != force.getDuration();
}

/**
 * @brief The start of the window, in ms from the start of the off-peak period.
 *
 * @details A negative offset counts back from the end of the period. A start before the
 *          period wraps around to a very large value: the window then never opens.
 *          validation.h rejects such a window at compile time, with startsWithinOffPeak().
 *
 * @param offPeakHours the duration of the off-peak period, in hours (12 at most)
 */
constexpr uint32_t windowStart(const pairForceLoad& force, const uint8_t offPeakHours)
{
  // 32 bits, as in startsWithinOffPeak(). In int (16 bits on the AVR), 10 h and more overflowed (#214).
  const uint32_t ulPeakDurationInSec{ static_cast< uint32_t >(offPeakHours * 3600UL) };

  // unsigned 32-bit arithmetic: a negative offset wraps around, back below the end of the period
  uint32_t start{ static_cast< uint32_t >(((force.getStartOffset() >= 0) ? 0 : ulPeakDurationInSec) + force.getStartOffset() * (offsetInMinutes(force) ? 60ul : 3600ul)) };
  start *= 1000ul;  // convert in milli-seconds
  return start;
}

/**
 * @brief The end of the window (excluded), in ms from the start of the off-peak period.
 *
 * @return UINT32_MAX for a duration of UINT16_MAX: until the end of the off-peak period,
 *         which stops any forcing anyway
 */
constexpr uint32_t windowEnd(const pairForceLoad& force, const uint8_t offPeakHours)
{
  if (UINT16_MAX == force.getDuration())
  {
    return UINT32_MAX;
  }
  return static_cast< uint32_t >(windowStart(force, offPeakHours) + force.getDuration() * (durationInMinutes(force) ? 60ul : 3600ul) * 1000ul);
}

/**
 * @brief Whether the load is in its forcing window.
 *
 * @param elapsed ms since the start of the off-peak period: millis() - its start, which stays
 *                right when millis() wraps around
 */
constexpr bool inWindow(const uint32_t elapsed, const uint32_t start, const uint32_t end)
{
  return (elapsed >= start) && (elapsed < end);
}

/**
 * @brief Whether the window starts within the off-peak period: otherwise, it would never open.
 *
 * @details A duration of 0 (no scheduled boost) is always valid, whatever its offset.
 */
constexpr bool startsWithinOffPeak(const pairForceLoad& force, const uint8_t offPeakHours)
{
  if (0 == force.getDuration())
  {
    return true;
  }
  const int32_t periodInSec{ static_cast< int32_t >(offPeakHours * 3600L) };
  const int32_t offsetInSec{ static_cast< int32_t >(force.getStartOffset() * (offsetInMinutes(force) ? 60L : 3600L)) };
  const int32_t startInSec{ (force.getStartOffset() >= 0) ? offsetInSec : periodInSec + offsetInSec };

  return (startInSec >= 0) && (startInSec < periodInSec);
}
}  // namespace DualTariff

#endif  // UTILS_DUALTARIFF_H
