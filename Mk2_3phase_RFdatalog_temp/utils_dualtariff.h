/**
 * @file utils_dualtariff.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions for dual tariff feature
 * @version 0.2
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2023-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @note This header has no Arduino dependency, so it can be exercised by the native test
 *       suite (test/native/test_dual_tariff).
 */

#ifndef UTILS_DUALTARIFF_H
#define UTILS_DUALTARIFF_H

#include <stdint.h>

#include "config_types.h"  // the boost windows, BoostEntry, ScheduledBoosts, PinList

/**
 * @brief The scheduled boosts: a window and the bitmasks of its targets, for each entry.
 *
 * @details Same accessors as OverrideTable, so that a target check takes either. Built from the
 *          ScheduledBoosts of config.h and a TargetMap (utils_override.h), or from plain pins.
 *
 * @tparam N number of boosts
 * @tparam MaxPins maximum number of pins (loads + relays) of a target list
 */
template< uint8_t N, uint8_t MaxPins = 16 >
class BoostSchedule
{
private:
  struct Entry
  {
    BoostWindow window;    /**< when the targets are boosted */
    uint16_t localBitmask; /**< bitmask of the local loads/relays */
    uint8_t remoteBitmask; /**< bitmask of the remote loads */
  };

  Entry entries_[N]{};

public:
  /** @brief Builds the boosts from their entries, with plain pins. */
  constexpr BoostSchedule(const BoostEntry< MaxPins > (&boosts)[N])
  {
    for (uint8_t i = 0; i < N; ++i)
    {
      entries_[i] = { boosts[i].window, boosts[i].targets.toLocalBitmask(), boosts[i].targets.toRemoteBitmask() };
    }
  }

  /** @brief Builds the boosts of config.h, their codes turned into pins by @p map (a TargetMap). */
  template< typename Map >
  constexpr BoostSchedule(const ScheduledBoosts< N, MaxPins >& boosts, const Map& map)
  {
    for (uint8_t i = 0; i < N; ++i)
    {
      entries_[i] = { boosts.getWindow(i), map.localBitmask(boosts.getTargets(i)), map.remoteBitmask(boosts.getTargets(i)) };
    }
  }

  /** @brief The number of boosts. */
  constexpr uint8_t size() const
  {
    return N;
  }

  /** @brief The window of boost @p i, or a window of length 0 if out of bounds. */
  constexpr BoostWindow getWindow(uint8_t i) const
  {
    return i < N ? entries_[i].window : BoostWindow{ FROM_START(MINUTES(0)), BoostLength{ 0 } };
  }

  /** @brief The local bitmask of boost @p i, or 0 if out of bounds. */
  constexpr uint16_t getLocalBitmask(uint8_t i) const
  {
    return i < N ? entries_[i].localBitmask : 0;
  }

  /** @brief The remote bitmask of boost @p i, or 0 if out of bounds. */
  constexpr uint8_t getRemoteBitmask(uint8_t i) const
  {
    return i < N ? entries_[i].remoteBitmask : 0;
  }
};

/** @brief No boost at all. */
template< uint8_t MaxPins >
class BoostSchedule< 0, MaxPins >
{
public:
  constexpr BoostSchedule() = default;

  /** @brief No boost in config.h either. */
  template< typename Map >
  constexpr BoostSchedule(const ScheduledBoosts< 0, MaxPins >& /* boosts */, const Map& /* map */)
  {
  }

  constexpr uint8_t size() const
  {
    return 0;
  }

  constexpr BoostWindow getWindow(uint8_t) const
  {
    return BoostWindow{ FROM_START(MINUTES(0)), BoostLength{ 0 } };
  }

  constexpr uint16_t getLocalBitmask(uint8_t) const
  {
    return 0;
  }

  constexpr uint8_t getRemoteBitmask(uint8_t) const
  {
    return 0;
  }
};

/** @brief Deduction guide, as for OverrideTable. */
template< uint8_t MaxPins, uint8_t N >
BoostSchedule(const BoostEntry< MaxPins > (&)[N])
  -> BoostSchedule< N, MaxPins >;

/** @brief Deduction guide: no argument, no boost. */
BoostSchedule() -> BoostSchedule< 0 >;

/** @brief Deduction guide: from the boosts of config.h. */
template< uint8_t N, uint8_t MaxPins, typename Map >
BoostSchedule(const ScheduledBoosts< N, MaxPins >&, const Map&) -> BoostSchedule< N, MaxPins >;

/**
 * @brief The windows of the scheduled boosts, in milliseconds from the start of the off-peak period.
 *
 * @ingroup DualTariff
 */
namespace DualTariff
{
/**
 * @brief Whether the boost is in its window.
 *
 * @param elapsed ms since the start of the off-peak period: millis() - its start, which stays
 *                right when millis() wraps around
 */
constexpr bool inWindow(const uint32_t elapsed, const uint32_t start, const uint32_t end)
{
  return (elapsed >= start) && (elapsed < end);
}

/**
 * @brief The start of the boost window, in ms from the start of the off-peak period.
 *
 * @details A start before the period wraps around to a very large value: the window then never
 *          opens. validation.h rejects such a window at compile time, with startsWithinOffPeak().
 *
 * @param offPeakHours the duration of the off-peak period, in hours (12 at most)
 */
constexpr uint32_t windowStart(const BoostWindow& window, const uint8_t offPeakHours)
{
  const uint32_t minutes{ window.start.fromEnd ? (static_cast< uint32_t >(offPeakHours) * 60U - window.start.minutes) : window.start.minutes };
  return minutes * 60000UL;
}

/**
 * @brief The end of the boost window (excluded), in ms from the start of the off-peak period.
 *
 * @return UINT32_MAX for UNTIL_THE_END: the end of the off-peak period stops any boost anyway
 */
constexpr uint32_t windowEnd(const BoostWindow& window, const uint8_t offPeakHours)
{
  if (UNTIL_THE_END == window.length.minutes)
  {
    return UINT32_MAX;
  }
  return windowStart(window, offPeakHours) + window.length.minutes * 60000UL;
}

/** @brief The start of the window, in minutes from the start of the off-peak period: negative if before it. */
constexpr int32_t startInMinutes(const BoostWindow& window, const uint8_t offPeakHours)
{
  const int32_t periodInMin{ static_cast< int32_t >(offPeakHours * 60L) };
  return window.start.fromEnd ? periodInMin - window.start.minutes : static_cast< int32_t >(window.start.minutes);
}

/** @brief Whether the window starts within the off-peak period: otherwise, it would never open. */
constexpr bool startsWithinOffPeak(const BoostWindow& window, const uint8_t offPeakHours)
{
  const int32_t startInMin{ startInMinutes(window, offPeakHours) };

  return (startInMin >= 0) && (startInMin < offPeakHours * 60L);
}

/**
 * @brief Whether a window with FOR() ends within the off-peak period (its end may be the end of
 *        the period): a longer one is a mistake, since the meter would cut it short.
 *
 * @return true for UNTIL_THE_END: only the meter ends such a boost
 */
constexpr bool endsWithinOffPeak(const BoostWindow& window, const uint8_t offPeakHours)
{
  if (UNTIL_THE_END == window.length.minutes)
  {
    return true;
  }
  return startInMinutes(window, offPeakHours) + window.length.minutes <= offPeakHours * 60L;
}

/** @brief Whether the boost lasts at all: a length of 0 is no boost. */
constexpr bool hasLength(const BoostWindow& window)
{
  return 0 != window.length.minutes;
}

/** @brief A boost resolved for a given off-peak duration: its window in ms and its target masks. */
struct Boost
{
  uint32_t start;
  uint32_t end;
  uint16_t local;
  uint8_t remote;
};

/** @brief Boost @p i of @p boosts, for an off-peak period of @p offPeakHours. */
template< uint8_t N, uint8_t MaxPins >
constexpr Boost resolve(const BoostSchedule< N, MaxPins >& boosts, const uint8_t i, const uint8_t offPeakHours)
{
  const BoostWindow window{ boosts.getWindow(i) };
  return Boost{ windowStart(window, offPeakHours), windowEnd(window, offPeakHours), boosts.getLocalBitmask(i), boosts.getRemoteBitmask(i) };
}
}  // namespace DualTariff

#endif  // UTILS_DUALTARIFF_H
