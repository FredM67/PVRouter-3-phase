/**
 * @file config_types.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief What config.h is written with: its types, constants and helpers
 * @version 1.0
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2026-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details config.h includes this file and nothing else. Nothing here depends on a setting, so
 *          config.h reads on its own, top to bottom, for the compiler as for the configurator.
 *          Nothing here depends on Arduino either: the native tests build it on the PC.
 *
 *          The settings types (RelayEngine, OverridePins, ScheduledBoosts, RotationGroups,
 *          TemperatureSensing)
 *          only hold what config.h says. The objects the firmware works with are built from them
 *          in config_derived.h, which the code includes instead of config.h.
 */

#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

#include <stdint.h>

#include "type_traits.hpp"  // MINUTES(), HOURS(), integral_constant, index_sequence
#include "load_map.h"       // Load::local(), Load::remote()
#include "utils_bits.h"     // unused_pin

/** Enum for serial output types */
enum class SerialOutputType
{
  HumanReadable, /**< Human-readable output for commissioning */
  IoT,           /**< Output for HomeAssistant or similar */
  JSON           /**< Output in JSON format */
};

/** Motherboard generation: they do not measure the same way */
enum class PcbVersion : uint8_t
{
  OLD, /**< the former 3-phase board: ADC reference AVCC (= VCC, 5 V or 3.3 V) */
  NEW  /**< the universal 3phaseDiverter board (rev. 6.0 and later): internal 1.1 V ADC reference */
};

/** Rotation modes */
enum class RotationModes : uint8_t
{
  OFF,  /**< Off */
  AUTO, /**< Once a day */
  PIN   /**< Pin triggered */
};


//--------------------------------------------------------------------------------------------------
// Targets of the override pins and of the scheduled boosts
//
/**
 * @brief Base value for virtual pins representing remote loads.
 * @details Values >= REMOTE_PIN_BASE are virtual pins for remote loads.
 *          REMOTE_LOAD(0) returns 128, REMOTE_LOAD(1) returns 129, etc.
 */
inline constexpr uint8_t REMOTE_PIN_BASE{ 128 };

inline constexpr uint8_t LOCAL_MASK_BITS{ 16 }; /**< pins in a target's local mask (uint16_t): D0-D15 */
inline constexpr uint8_t REMOTE_MASK_BITS{ 8 }; /**< remote loads in a target's remote mask (uint8_t) */

/**
 * @brief The codes LOAD(), RELAY() and ALL_...() put in a target list.
 *
 * @details Which pin load #1 is, and which relays exist, is only known once config.h is read:
 *          the helpers cannot give a pin here. They give a code, and config_derived.h turns
 *          each code into its pin(s) (TargetMap, in utils_override.h). A plain number in a list
 *          is a pin, and so is REMOTE_LOAD() (from REMOTE_PIN_BASE on).
 */
namespace TargetCode
{
inline constexpr uint8_t LOAD{ 0x40 };       /**< LOAD(n) is LOAD + n */
inline constexpr uint8_t LOCAL_LOAD{ 0x60 }; /**< LOCAL_LOAD(n) is LOCAL_LOAD + n */
inline constexpr uint8_t RELAY{ 0x70 };      /**< RELAY(n) is RELAY + n */

inline constexpr uint8_t MAX_LOADS{ 32 };       /**< LOAD(n): n below this */
inline constexpr uint8_t MAX_LOCAL_LOADS{ 16 }; /**< LOCAL_LOAD(n): n below this */
inline constexpr uint8_t MAX_RELAYS{ 16 };      /**< RELAY(n): n below this */

inline constexpr uint8_t ALL_LOCAL_LOADS{ 0xF0 };      /**< ALL_LOCAL_LOADS() */
inline constexpr uint8_t ALL_REMOTE_LOADS{ 0xF1 };     /**< ALL_REMOTE_LOADS() */
inline constexpr uint8_t ALL_LOADS{ 0xF2 };            /**< ALL_LOADS() */
inline constexpr uint8_t ALL_RELAYS{ 0xF3 };           /**< ALL_RELAYS() */
inline constexpr uint8_t ALL_LOADS_AND_RELAYS{ 0xF4 }; /**< ALL_LOADS_AND_RELAYS() */
}  // namespace TargetCode

/** @brief Load @p loadNum of physicalLoadPin[], local or remote (0, no load, if out of range). */
constexpr uint8_t LOAD(const uint8_t loadNum)
{
  return loadNum < TargetCode::MAX_LOADS ? static_cast< uint8_t >(TargetCode::LOAD + loadNum) : 0;
}

/** @brief Same as LOAD(), for a load known to be local. */
constexpr uint8_t LOCAL_LOAD(const uint8_t loadNum)
{
  return loadNum < TargetCode::MAX_LOCAL_LOADS ? static_cast< uint8_t >(TargetCode::LOCAL_LOAD + loadNum) : 0;
}

/**
 * @brief The @p loadNum-th REMOTE load, counting remote loads only, in physicalLoadPin[] order.
 * @return its virtual pin, REMOTE_PIN_BASE + @p loadNum (0, no load, from REMOTE_MASK_BITS on).
 */
constexpr uint8_t REMOTE_LOAD(const uint8_t loadNum)
{
  return loadNum < REMOTE_MASK_BITS ? static_cast< uint8_t >(REMOTE_PIN_BASE + loadNum) : 0;
}

/** @brief Relay @p relayNum of relays (0, no relay, if out of range or without relay diversion). */
constexpr uint8_t RELAY(const uint8_t relayNum)
{
  return relayNum < TargetCode::MAX_RELAYS ? static_cast< uint8_t >(TargetCode::RELAY + relayNum) : 0;
}

/** @brief Every local load. */
constexpr uint8_t ALL_LOCAL_LOADS()
{
  return TargetCode::ALL_LOCAL_LOADS;
}

/** @brief Every remote load. */
constexpr uint8_t ALL_REMOTE_LOADS()
{
  return TargetCode::ALL_REMOTE_LOADS;
}

/** @brief Every load, local and remote. */
constexpr uint8_t ALL_LOADS()
{
  return TargetCode::ALL_LOADS;
}

/** @brief Every relay (none without relay diversion). */
constexpr uint8_t ALL_RELAYS()
{
  return TargetCode::ALL_RELAYS;
}

/** @brief Every load and every relay. */
constexpr uint8_t ALL_LOADS_AND_RELAYS()
{
  return TargetCode::ALL_LOADS_AND_RELAYS;
}

/**
 * @brief Wrapper for a list of pins, constructible from variadic arguments.
 * @tparam MaxPins Maximum number of pins supported.
 *
 * @details Supports both physical pins (0-127) and virtual pins for remote loads (128+).
 *          Virtual pins are created using REMOTE_LOAD(n) which returns REMOTE_PIN_BASE + n.
 */
template< uint8_t MaxPins >
struct PinList
{
  uint8_t pins[MaxPins];
  uint8_t count;

  /**
   * @brief Default constructor. Initializes with zero pins.
   */
  constexpr PinList()
    : pins{}, count(0) {}

  /**
   * @brief Constructor from local-only bitmask.
   * @param bitmask Local pin bitmask (bits 0-15 correspond to physical pins).
   */
  constexpr PinList(uint16_t bitmask)
    : pins{}, count(0)
  {
    for (uint8_t pin = 0; pin < 16 && count < MaxPins; ++pin)
    {
      if (bitmask & (1U << pin))
      {
        pins[count++] = pin;  // Store the physical pin number
      }
    }
  }

  /**
   * @brief Constructor from combined bitmask. Extracts both local and remote pins.
   * @param bitmask Combined bitmask value:
   *                - Lower 16 bits (0-15): local/physical pin bitmask
   *                - Upper 16 bits (16-31): remote load bitmask (bit 16 = remote 0, etc.)
   */
  constexpr PinList(uint32_t bitmask)
    : pins{}, count(0)
  {
    // Extract local pins from lower 16 bits
    for (uint8_t pin = 0; pin < 16 && count < MaxPins; ++pin)
    {
      if (bitmask & (1UL << pin))
      {
        pins[count++] = pin;  // Store the physical pin number
      }
    }
    // Extract remote loads from upper 16 bits
    for (uint8_t i = 0; i < 8 && count < MaxPins; ++i)
    {
      if (bitmask & (1UL << (16 + i)))
      {
        pins[count++] = REMOTE_PIN_BASE + i;  // Store virtual pin for remote load
      }
    }
  }

  /**
   * @brief Variadic constructor. Initializes with provided pins.
   * @param args List of pins (can include both physical and virtual pins).
   */
  template< typename... Args >
  constexpr PinList(Args... args)
    : pins{ static_cast< uint8_t >(args)... }, count(sizeof...(args)) {}

  /**
   * @brief Converts the pin list to a bitmask for local/physical pins only.
   * @return Bitmask with bits set for pins < REMOTE_PIN_BASE.
   */
  constexpr uint16_t toLocalBitmask() const
  {
    uint16_t result{ 0 };
    for (uint8_t i = 0; i < count; ++i)
    {
      if (pins[i] < REMOTE_PIN_BASE)
      {
        result |= (1U << pins[i]);
      }
    }
    return result;
  }

  /**
   * @brief Converts the pin list to a bitmask for remote loads only.
   * @return Bitmask where bit n is set if remote load n is in the list.
   *         (Pins >= REMOTE_PIN_BASE are converted to indices 0-7)
   */
  constexpr uint8_t toRemoteBitmask() const
  {
    uint8_t result{ 0 };
    for (uint8_t i = 0; i < count; ++i)
    {
      if (pins[i] >= REMOTE_PIN_BASE)
      {
        const uint8_t remoteIndex = pins[i] - REMOTE_PIN_BASE;
        if (remoteIndex < REMOTE_MASK_BITS)
        {
          result |= (1U << remoteIndex);
        }
      }
    }
    return result;
  }
};

/**
 * @brief Structure holding a pin and its associated index list.
 * @tparam MaxPins Maximum number of pins supported.
 */
template< uint8_t MaxPins >
struct KeyIndexPair
{
  uint8_t pin;
  PinList< MaxPins > indexList;

  /**
   * @brief Constructor.
   * @param k Pin value.
   * @param list Index list.
   */
  constexpr KeyIndexPair(uint8_t k, const PinList< MaxPins >& list)
    : pin(k), indexList(list) {}

  /**
   * @brief Returns the bitmask for local pins in the index list.
   * @return Bitmask for physical pins (< REMOTE_PIN_BASE).
   */
  constexpr uint16_t getLocalBitmask() const
  {
    return indexList.toLocalBitmask();
  }

  /**
   * @brief Returns the bitmask for remote loads in the index list.
   * @return Bitmask where bit n is set if remote load n is in the list.
   */
  constexpr uint8_t getRemoteBitmask() const
  {
    return indexList.toRemoteBitmask();
  }
};

/**
 * @brief The override pins, as in config.h: each pin with the targets it forces.
 *
 * @details The targets stay codes here; OverrideTable (utils_override.h) holds what they force.
 *
 * @tparam N Number of override pins.
 * @tparam MaxPins Maximum number of targets per pin.
 */
template< uint8_t N, uint8_t MaxPins = 16 >
class OverridePins
{
public:
  /** @brief The override pins, from a list of { pin, targets }. */
  constexpr OverridePins(const KeyIndexPair< MaxPins > (&pairs)[N])
    : OverridePins(pairs, make_index_sequence< N >{})
  {
  }

  /** @brief Number of override pins. */
  constexpr uint8_t size() const
  {
    return N;
  }

  /** @brief The override pin at @p index (0 if out of range). */
  constexpr uint8_t getPin(const uint8_t index) const
  {
    return index < N ? entries_[index].pin : 0;
  }

  /** @brief The targets of the override pin at @p index, as codes. */
  constexpr const PinList< MaxPins >& getTargets(const uint8_t index) const
  {
    return entries_[index].indexList;
  }

private:
  template< uint8_t... I >
  constexpr OverridePins(const KeyIndexPair< MaxPins > (&pairs)[N], index_sequence< I... > /* is */)
    : entries_{ pairs[I]... }
  {
  }

  const KeyIndexPair< MaxPins > entries_[N]; /**< the pins and their targets */
};

/** @brief Deduction guide: N and MaxPins from the list. */
template< uint8_t MaxPins, uint8_t N >
OverridePins(const KeyIndexPair< MaxPins > (&)[N])
  -> OverridePins< N, MaxPins >;

//--------------------------------------------------------------------------------------------------
// Scheduled boosts
//
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

/**
 * @brief A boost lasting @p m.
 *
 * @note MINUTES(65535) would read as UNTIL_THE_END: it becomes 65534, which still ends after any
 *       off-peak period, so validation.h rejects it instead of boosting until the end.
 */
constexpr BoostLength FOR(const Minutes m)
{
  return BoostLength{ m.value == UNTIL_THE_END ? static_cast< uint16_t >(UNTIL_THE_END - 1) : m.value };
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
 * @brief The scheduled boosts, as in config.h: each window with the targets it forces.
 *
 * @details The targets stay codes here; BoostSchedule (utils_dualtariff.h) holds what they force.
 *
 * @tparam N Number of boosts.
 * @tparam MaxPins Maximum number of targets per boost.
 */
template< uint8_t N, uint8_t MaxPins = 16 >
class ScheduledBoosts
{
public:
  /** @brief The boosts, from a list of { window, targets }. */
  constexpr ScheduledBoosts(const BoostEntry< MaxPins > (&boosts)[N])
    : ScheduledBoosts(boosts, make_index_sequence< N >{})
  {
  }

  /** @brief Number of boosts. */
  constexpr uint8_t size() const
  {
    return N;
  }

  /** @brief The window of boost @p i. */
  constexpr BoostWindow getWindow(const uint8_t i) const
  {
    return entries_[i].window;
  }

  /** @brief The targets of boost @p i, as codes. */
  constexpr const PinList< MaxPins >& getTargets(const uint8_t i) const
  {
    return entries_[i].targets;
  }

private:
  template< uint8_t... I >
  constexpr ScheduledBoosts(const BoostEntry< MaxPins > (&boosts)[N], index_sequence< I... > /* is */)
    : entries_{ boosts[I]... }
  {
  }

  const BoostEntry< MaxPins > entries_[N]; /**< the windows and their targets */
};

/** @brief No boost at all. */
template< uint8_t MaxPins >
class ScheduledBoosts< 0, MaxPins >
{
public:
  constexpr ScheduledBoosts() = default;

  /** @brief Number of boosts: none. */
  constexpr uint8_t size() const
  {
    return 0;
  }

  /** @brief No window: never called, there for the loops over size(). */
  constexpr BoostWindow getWindow(uint8_t) const
  {
    return BoostWindow{ FROM_START(MINUTES(0)), BoostLength{ 0 } };
  }

  /** @brief No targets: never called, there for the loops over size(). */
  constexpr PinList< MaxPins > getTargets(uint8_t) const
  {
    return PinList< MaxPins >{};
  }
};

/** @brief Deduction guide, as for OverridePins. */
template< uint8_t MaxPins, uint8_t N >
ScheduledBoosts(const BoostEntry< MaxPins > (&)[N])
  -> ScheduledBoosts< N, MaxPins >;

/** @brief Deduction guide: no argument, no boost. */
ScheduledBoosts() -> ScheduledBoosts< 0 >;

/**
 * @brief The rotation groups, as in config.h: the loads that take turns at the top of the
 *        priority list, one group per appliance (the elements of a 3-phase water heater).
 *
 * @details Each group is a list of target codes, as for the override pins: LOAD(n),
 *          LOCAL_LOAD(n), REMOTE_LOAD(n), ALL_LOCAL_LOADS()... The codes stay codes here;
 *          Rotation::Table (rotation_groups.h) holds the priority places each group rotates in.
 *          One constructor takes up to 10 groups: each group needs at least 2 loads, and the
 *          router drives at most 12 local and 8 remote loads, so no valid config needs more.
 *
 * @tparam N Number of groups.
 * @tparam MaxPins Maximum number of targets per group.
 */
template< uint8_t N, uint8_t MaxPins = 16 >
class RotationGroups
{
  static_assert(N >= 1 && N <= 10, "rotationGroups holds 1 to 10 groups");

public:
  /** @brief The groups, one list of targets each. */
  constexpr RotationGroups(const PinList< MaxPins >& g0, const PinList< MaxPins >& g1 = {},
                           const PinList< MaxPins >& g2 = {}, const PinList< MaxPins >& g3 = {},
                           const PinList< MaxPins >& g4 = {}, const PinList< MaxPins >& g5 = {},
                           const PinList< MaxPins >& g6 = {}, const PinList< MaxPins >& g7 = {},
                           const PinList< MaxPins >& g8 = {}, const PinList< MaxPins >& g9 = {})
    : RotationGroups(Ten{ { g0, g1, g2, g3, g4, g5, g6, g7, g8, g9 } }, make_index_sequence< N >{})
  {
  }

  /** @brief Number of groups. */
  constexpr uint8_t size() const
  {
    return N;
  }

  /** @brief The targets of group @p index, as codes. */
  constexpr const PinList< MaxPins >& getGroup(const uint8_t index) const
  {
    return groups_[index];
  }

private:
  struct Ten
  {
    PinList< MaxPins > groups[10];
  };

  template< uint8_t... I >
  constexpr RotationGroups(const Ten& ten, index_sequence< I... > /* is */)
    : groups_{ ten.groups[I]... }
  {
  }

  const PinList< MaxPins > groups_[N]; /**< the targets of each group */
};

/** @brief No rotation group. */
template< uint8_t MaxPins >
class RotationGroups< 0, MaxPins >
{
public:
  constexpr RotationGroups() = default;

  /** @brief Number of groups: none. */
  constexpr uint8_t size() const
  {
    return 0;
  }

  /** @brief No targets: never called, there for the loops over size(). */
  constexpr PinList< MaxPins > getGroup(uint8_t) const
  {
    return PinList< MaxPins >{};
  }
};

/** @brief Deduction guides: the number of groups is the number of lists. */
RotationGroups() -> RotationGroups< 0 >;
using RotationGroupTargets = PinList< 16 >; /**< the targets of one rotation group */
RotationGroups(const RotationGroupTargets&) -> RotationGroups< 1 >;
RotationGroups(const RotationGroupTargets&, const RotationGroupTargets&) -> RotationGroups< 2 >;
RotationGroups(const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&) -> RotationGroups< 3 >;
RotationGroups(const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&) -> RotationGroups< 4 >;
RotationGroups(const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&) -> RotationGroups< 5 >;
RotationGroups(const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&) -> RotationGroups< 6 >;
RotationGroups(const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&) -> RotationGroups< 7 >;
RotationGroups(const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&) -> RotationGroups< 8 >;
RotationGroups(const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&) -> RotationGroups< 9 >;
RotationGroups(const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&, const RotationGroupTargets&) -> RotationGroups< 10 >;

//--------------------------------------------------------------------------------------------------
// Relays
//

/**
 * @brief The settings of one relay, as in config.h: { pin, surplus, import, minON, minOFF }.
 *
 * @details Thresholds in W, minimum times in minutes. Left out, they take the defaults of
 *          RelayLogic::Relay: 1000 W, 200 W, 5 and 5 minutes.
 */
struct RelaySetting
{
  uint8_t pin;              /**< the pin of the relay */
  int16_t surplusThreshold; /**< surplus in W to turn the relay ON */
  int16_t importThreshold;  /**< import in W to turn it OFF (negative: surplus below which it turns OFF) */
  uint16_t minON;           /**< minimum ON time, in minutes */
  uint16_t minOFF;          /**< minimum OFF time, in minutes */

  /** @brief A relay on @p p; what is left out takes its default. */
  constexpr RelaySetting(const uint8_t p, const int16_t surplus = 1000, const int16_t import = 200, const uint16_t on = 5, const uint16_t off = 5)
    : pin{ p }, surplusThreshold{ surplus }, importThreshold{ import }, minON{ on }, minOFF{ off }
  {
  }
};

/**
 * @brief The relays, as in config.h: the sliding average and the settings of each relay.
 *
 * @details RelayDiverter (utils_relay.h) is the engine built from them.
 *
 * @tparam N Number of relays.
 * @tparam D Duration of the sliding average, in minutes (RELAY_FILTER_DELAY).
 */
template< uint8_t N, uint8_t D = 10 >
class RelayEngine
{
public:
  /** @brief The relays, with the default sliding average. */
  explicit constexpr RelayEngine(const RelaySetting (&ref)[N])
    : RelayEngine(ref, make_index_sequence< N >{})
  {
  }

  /** @brief The relays, with a sliding average given as an integral_constant< uint8_t >. */
  constexpr RelayEngine(integral_constant< uint8_t, D > /* ic */, const RelaySetting (&ref)[N])
    : RelayEngine(ref, make_index_sequence< N >{})
  {
  }

  /** @brief The relays, with a sliding average given with MINUTES(). */
  template< uint16_t V >
  constexpr RelayEngine(integral_constant< uint16_t, V > /* ic */, const RelaySetting (&ref)[N])
    : RelayEngine(ref, make_index_sequence< N >{})
  {
    static_assert(V <= UINT8_MAX, "******** RELAY_FILTER_DELAY cannot exceed 255 minutes ! Please check your config.h ! ********");
    static_assert(V == D, "******** The MINUTES() given to RelayEngine differs from its template parameter D (RELAY_FILTER_DELAY) ! Please check your config.h ! ********");
  }

  /** @brief Number of relays. */
  constexpr uint8_t size() const
  {
    return N;
  }

  /** @brief The settings of relay @p idx. */
  constexpr const RelaySetting& get_relay(const uint8_t idx) const
  {
    return relay_[idx];
  }

private:
  template< uint8_t... I >
  constexpr RelayEngine(const RelaySetting (&ref)[N], index_sequence< I... > /* is */)
    : relay_{ ref[I]... }
  {
  }

  const RelaySetting relay_[N]; /**< the settings of each relay */
};

/** @brief Deduction guide: the default sliding average. */
template< uint8_t N >
RelayEngine(const RelaySetting (&)[N]) -> RelayEngine< N, 10 >;

/** @brief Deduction guide: the sliding average as an integral_constant< uint8_t >. */
template< uint8_t N, uint8_t D >
RelayEngine(integral_constant< uint8_t, D >, const RelaySetting (&)[N]) -> RelayEngine< N, D >;

/** @brief Deduction guide: the sliding average from MINUTES(). */
template< uint8_t N, uint16_t D >
RelayEngine(integral_constant< uint16_t, D >, const RelaySetting (&)[N]) -> RelayEngine< N, static_cast< uint8_t >(D) >;

//--------------------------------------------------------------------------------------------------
// Temperature sensors
//
/**
 * @struct DeviceAddress
 * @brief Structure representing the address of a device.
 *
 * This structure is used to store the unique address of a device, such as a DS18B20 temperature sensor.
 * The address is an array of 8 bytes, typically represented in hexadecimal.
 *
 * @details
 * - Each byte in the array represents a part of the unique address of the device.
 * - This structure is used to identify and communicate with specific devices on the OneWire bus.
 *
 * @ingroup TemperatureSensing
 */
struct DeviceAddress
{
  uint8_t addr[8]{}; /**< The address of the device as an array of 8 bytes. */
};

/**
 * @brief The temperature sensors, as in config.h: the pin of the bus and the address of each sensor.
 *
 * @details TempSensors (utils_temp.h) reads them.
 *
 * @tparam N Number of sensors.
 */
template< uint8_t N >
class TemperatureSensing
{
public:
  constexpr TemperatureSensing() = delete;

  /** @brief The sensors on @p pin, from their addresses. */
  constexpr TemperatureSensing(const uint8_t pin, const DeviceAddress (&ref)[N])
    : TemperatureSensing(pin, ref, make_index_sequence< N >{})
  {
  }

  /** @brief Number of sensors. */
  constexpr uint8_t size() const
  {
    return N;
  }

  /** @brief The pin of the bus. */
  constexpr uint8_t get_pin() const
  {
    return sensorPin;
  }

protected:
  const uint8_t sensorPin{ unused_pin }; /**< The pin of the sensor(s) */
  const DeviceAddress sensorAddrs[N];    /**< Array of sensors */

private:
  template< uint8_t... I >
  constexpr TemperatureSensing(const uint8_t pin, const DeviceAddress (&ref)[N], index_sequence< I... > /* is */)
    : sensorPin{ pin }, sensorAddrs{ ref[I]... }
  {
  }
};

#endif /* CONFIG_TYPES_H */
