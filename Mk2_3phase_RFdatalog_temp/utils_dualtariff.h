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

#include "utils_override.h"  // PinList, REMOTE_PIN_BASE, MINUTES() (through type_traits.hpp)

/** @brief A number of minutes: the unit of the scheduled boosts, built with HOURS() or MINUTES(). */
struct Minutes
{
  uint16_t value;

  /** @brief @p v minutes: explicit, so that a bare number is not taken for a duration. */
  explicit constexpr Minutes(const uint16_t v)
    : value(v) {}

  /** @brief From MINUTES() or HOURS() (type_traits.hpp), the same macros as for the relays. */
  template< uint16_t V >
  constexpr Minutes(integral_constant< uint16_t, V > /* ic */)
    : value(V) {}
};

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
constexpr Boost resolve(const ScheduledBoosts< N, MaxPins >& boosts, const uint8_t i, const uint8_t offPeakHours)
{
  const BoostWindow window{ boosts.getWindow(i) };
  return Boost{ windowStart(window, offPeakHours), windowEnd(window, offPeakHours), boosts.getLocalBitmask(i), boosts.getRemoteBitmask(i) };
}
}  // namespace DualTariff

#endif  // UTILS_DUALTARIFF_H
