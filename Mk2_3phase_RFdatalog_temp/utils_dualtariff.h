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

#include "utils_override.h"  // PinList, REMOTE_PIN_BASE

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

/** @brief A number of minutes: the unit of the scheduled boosts, built with HOURS() or MINUTES(). */
struct Minutes
{
  uint16_t value;
};

/** @brief @p n hours, as Minutes. */
constexpr Minutes HOURS(const uint8_t n)
{
  return Minutes{ static_cast< uint16_t >(n * 60U) };
}

/** @brief @p n minutes, as Minutes. */
constexpr Minutes MINUTES(const uint16_t n)
{
  return Minutes{ n };
}

/** @brief Where a boost starts: @p minutes after the start of the off-peak period, or before its end if @p fromEnd. */
struct BoostStart
{
  bool fromEnd;
  uint16_t minutes;
};

/** @brief The length of a boost that only the meter ends: the end of the off-peak period stops it anyway. */
inline constexpr uint16_t UNTIL_THE_END{ UINT16_MAX };

/** @brief How long a boost lasts, in minutes, or UNTIL_THE_END. */
struct BoostLength
{
  uint16_t minutes;
};

/** @brief When a load is boosted during the off-peak period. */
struct BoostWindow
{
  BoostStart start;
  BoostLength length;
};

/** @brief A start @p m after the beginning of the off-peak period. */
constexpr BoostStart FROM_START(const Minutes m)
{
  return BoostStart{ false, m.value };
}

/** @brief A start @p m before the end of the off-peak period. */
constexpr BoostStart BEFORE_END(const Minutes m)
{
  return BoostStart{ true, m.value };
}

/** @brief A boost lasting @p m. */
constexpr BoostLength FOR(const Minutes m)
{
  return BoostLength{ m.value };
}

/** @brief The first @p d of the off-peak period. */
constexpr BoostWindow FIRST(const Minutes d)
{
  return BoostWindow{ FROM_START(MINUTES(0)), FOR(d) };
}

/** @brief The last @p d of the off-peak period, until the end. */
constexpr BoostWindow LAST(const Minutes d)
{
  return BoostWindow{ BEFORE_END(d), BoostLength{ UNTIL_THE_END } };
}

/** @brief When a set of loads is boosted: a window and the targets. */
template< uint8_t MaxPins = 16 >
struct BoostEntry
{
  BoostWindow window;
  PinList< MaxPins > targets;

  /** @brief A boost with its window and targets. */
  constexpr BoostEntry(const BoostWindow& w, const PinList< MaxPins >& t)
    : window(w), targets(t) {}

  /** @brief A boost from @p s until the end of the off-peak period. */
  constexpr BoostEntry(const BoostStart& s, const PinList< MaxPins >& t)
    : window{ s, BoostLength{ UNTIL_THE_END } }, targets(t) {}

  /** @brief A boost from @p s, lasting @p l. */
  constexpr BoostEntry(const BoostStart& s, const BoostLength& l, const PinList< MaxPins >& t)
    : window{ s, l }, targets(t) {}
};

/**
 * @brief The scheduled boosts: a window and the bitmasks of its targets, for each entry.
 *
 * @details Same accessors as OverridePins, so that a target check takes either.
 *
 * @tparam N number of boosts
 * @tparam MaxPins maximum number of pins (loads + relays) of a target list
 */
template< uint8_t N, uint8_t MaxPins = 16 >
class ScheduledBoosts
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
  /** @brief Builds the boosts from their entries. */
  constexpr ScheduledBoosts(const BoostEntry< MaxPins > (&boosts)[N])
  {
    for (uint8_t i = 0; i < N; ++i)
    {
      entries_[i] = { boosts[i].window, boosts[i].targets.toLocalBitmask(), boosts[i].targets.toRemoteBitmask() };
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
class ScheduledBoosts< 0, MaxPins >
{
public:
  constexpr ScheduledBoosts() = default;

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

/** @brief Deduction guide, as for OverridePins. */
template< uint8_t MaxPins, uint8_t N >
ScheduledBoosts(const BoostEntry< MaxPins > (&)[N])
  -> ScheduledBoosts< N, MaxPins >;

/** @brief Deduction guide: no argument, no boost. */
ScheduledBoosts() -> ScheduledBoosts< 0 >;

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
  // 16 bits: 12 h (the most validation.h allows) is 43200 s. In int, 10 h and more overflowed (#214).
  const uint16_t uiPeakDurationInSec{ static_cast< uint16_t >(offPeakHours * 3600U) };

  // unsigned 32-bit arithmetic: a negative offset wraps around, back below the end of the period
  uint32_t start{ static_cast< uint32_t >(((force.getStartOffset() >= 0) ? 0 : uiPeakDurationInSec) + force.getStartOffset() * (offsetInMinutes(force) ? 60ul : 3600ul)) };
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
  const uint32_t minutes{ window.start.fromEnd ? (static_cast< uint32_t >(offPeakHours) * 60UL - window.start.minutes) : window.start.minutes };
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

/** @brief Whether the window starts within the off-peak period: otherwise, it would never open. */
constexpr bool startsWithinOffPeak(const BoostWindow& window, const uint8_t offPeakHours)
{
  const int32_t periodInMin{ static_cast< int32_t >(offPeakHours * 60L) };
  const int32_t startInMin{ window.start.fromEnd ? periodInMin - window.start.minutes : static_cast< int32_t >(window.start.minutes) };

  return (startInMin >= 0) && (startInMin < periodInMin);
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
constexpr Boost resolve(const ScheduledBoosts< N, MaxPins >& boosts, const uint8_t i, const uint8_t offPeakHours)
{
  const BoostWindow window{ boosts.getWindow(i) };
  return Boost{ windowStart(window, offPeakHours), windowEnd(window, offPeakHours), boosts.getLocalBitmask(i), boosts.getRemoteBitmask(i) };
}
}  // namespace DualTariff

#endif  // UTILS_DUALTARIFF_H
