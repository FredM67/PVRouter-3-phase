/**
 * @file utils_override.h
 * @brief Compile-time utilities for managing override pins and index-to-bitmask mapping.
 *
 * This header provides types and functions for representing and manipulating sets of override pins
 * and their associated pins, all at compile time. It enables efficient bitmask computation and
 * static configuration of pin mappings for embedded systems, such as PVRouter.
 *
 * Pin features:
 * - Compile-time conversion of index lists to bitmasks
 * - Type-safe representation of pin/index associations
 * - Static configuration of override pin mappings
 * - Template deduction guide for convenient usage
 *
 * config.h writes the override pins with codes (OverridePins, in config_types.h): TargetMap
 * turns each code into its pins, and OverrideTable holds the resulting bitmasks.
 *
 * Usage example, with plain pins:
 * @code
 * constexpr OverrideTable table{ { { 2, { 5, 6 } },
 *                                  { 3, { 7 } } } };
 * @endcode
 *
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @version 0.2
 * @date 2026-11-21
 * @copyright Copyright (c) 2025-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UTILS_OVERRIDE_H
#define UTILS_OVERRIDE_H

#include "type_traits.hpp"

#include "config_types.h"  // PinList, KeyIndexPair, OverridePins, RelayEngine, the target codes
#include "utils_bits.h"
#include "load_map.h"

#ifdef ARDUINO
#include "debug.h"
#endif

/**
 * @brief Override pin of a load: its physical pin if local, a virtual pin if remote.
 *
 * @details The virtual pin is REMOTE_PIN_BASE + the load's rank among the remote
 *          loads, which is also its bit in the remote override bitmask.
 *
 * @param loadMap The packed load map.
 * @param loadNum Index into the map.
 */
template< uint8_t N >
constexpr uint8_t overridePinOf(const uint8_t (&loadMap)[N], uint8_t loadNum)
{
  return Load::isLocal(loadMap[loadNum])
           ? Load::pinOf(loadMap[loadNum])
           : static_cast< uint8_t >(REMOTE_PIN_BASE + Load::remoteOrdinal(loadMap, loadNum));
}

/**
 * @brief Bitmask of the pins of every local load (lower 16 bits).
 */
template< uint8_t N >
constexpr uint32_t localLoadsMask(const uint8_t (&loadMap)[N])
{
  uint32_t mask{ 0 };
  for (const auto& entry : loadMap)
  {
    if (Load::isLocal(entry))
    {
      bit_set(mask, Load::pinOf(entry));
    }
  }
  return mask;
}

/**
 * @brief Bitmask of every remote load (bit 16 + remote rank).
 */
template< uint8_t N >
constexpr uint32_t remoteLoadsMask(const uint8_t (&loadMap)[N])
{
  uint32_t mask{ 0 };
  for (uint8_t i = 0; i != Load::countRemoteLoads(loadMap); ++i)
  {
    bit_set(mask, 16 + i);
  }
  return mask;
}

// Valid pins: 2-13, so valid mask is 0b11111111111100
constexpr uint16_t validPinMask{ 0b11111111111100 };

/**
 * @brief Compile-time validation function for pin values.
 * @tparam Pins List of pins to validate.
 * @return true if all pins are valid, false otherwise.
 */
template< uint8_t... Pins >
constexpr bool are_pins_valid()
{
  return ((validPinMask & (1U << Pins)) && ...);
}

/**
 * @brief Helper macro to validate pins at compile time.
 * Usage: VALIDATE_PINS(2, 3, 5) will cause a compile error if any pin is invalid.
 */
#define VALIDATE_PINS(...) static_assert(are_pins_valid< __VA_ARGS__ >(), "Invalid pin(s) specified")

/**
 * @brief Helper to convert pins to a bitmask at compile-time.
 * @tparam Pins List of pins to set in the bitmask.
 * @return Bitmask with bits set at the specified pins.
 */
template< uint8_t... Pins >
constexpr uint16_t indicesToBitmask()
{
  return ((1U << Pins) | ...);
}

/**
 * @class TargetMap
 * @brief What the target codes of config.h stand for, once the load map and the relays are known.
 *
 * @details LOAD(n) is the override pin of load n (overridePinOf()), LOCAL_LOAD(n) its local pin,
 *          RELAY(n) the pin of relay n, and ALL_...() every pin of their kind. Out of range, or
 *          RELAY() without relay diversion, a code gives pin 0, which no load or relay uses:
 *          validation.h reports it. Any other value is a pin already.
 *
 * @tparam NL Number of loads in the load map.
 * @tparam NR Number of relays.
 * @tparam D Duration of the sliding average of the relays (unused here).
 */
template< uint8_t NL, uint8_t NR, uint8_t D >
class TargetMap
{
public:
  /**
   * @param loadMap The load map (physicalLoadPin).
   * @param relays The relays, as in config.h.
   * @param relayDiversion Whether the relays are in use (RELAY_DIVERSION).
   */
  constexpr TargetMap(const uint8_t (&loadMap)[NL], const RelayEngine< NR, D >& relays, const bool relayDiversion)
    : loadMap_(loadMap), relays_(relays), relayDiversion_(relayDiversion)
  {
  }

  /** @brief The pin that a single code stands for (any code but ALL_...()). */
  constexpr uint8_t pinOf(const uint8_t code) const
  {
    if (code >= TargetCode::LOAD && code < TargetCode::LOAD + TargetCode::MAX_LOADS)
    {
      const uint8_t loadNum = code - TargetCode::LOAD;
      return loadNum < NL ? overridePinOf(loadMap_, loadNum) : 0;
    }
    if (code >= TargetCode::LOCAL_LOAD && code < TargetCode::LOCAL_LOAD + TargetCode::MAX_LOCAL_LOADS)
    {
      const uint8_t loadNum = code - TargetCode::LOCAL_LOAD;
      return loadNum < NL ? Load::pinOf(loadMap_[loadNum]) : 0;
    }
    if (code >= TargetCode::RELAY && code < TargetCode::RELAY + TargetCode::MAX_RELAYS)
    {
      const uint8_t relayNum = code - TargetCode::RELAY;
      return relayDiversion_ && relayNum < NR ? relays_.get_relay(relayNum).pin : 0;
    }
    return code;
  }

  /** @brief Whether @p code is one of ALL_...(): several pins. */
  static constexpr bool isGroup(const uint8_t code)
  {
    return code >= TargetCode::ALL_LOCAL_LOADS && code <= TargetCode::ALL_LOADS_AND_RELAYS;
  }

  /** @brief The pins of an ALL_...() code: local pins in the lower 16 bits, remote loads from bit 16. */
  constexpr uint32_t groupMask(const uint8_t code) const
  {
    switch (code)
    {
      case TargetCode::ALL_LOCAL_LOADS:
        return localLoadsMask(loadMap_);
      case TargetCode::ALL_REMOTE_LOADS:
        return remoteLoadsMask(loadMap_);
      case TargetCode::ALL_LOADS:
        return localLoadsMask(loadMap_) | remoteLoadsMask(loadMap_);
      case TargetCode::ALL_RELAYS:
        return relaysMask();
      case TargetCode::ALL_LOADS_AND_RELAYS:
        return localLoadsMask(loadMap_) | remoteLoadsMask(loadMap_) | relaysMask();
      default:
        return 0;
    }
  }

  /** @brief The local loads and relays that @p targets force, as a bitmask of pins. */
  template< uint8_t MaxPins >
  constexpr uint16_t localBitmask(const PinList< MaxPins >& targets) const
  {
    uint16_t result{ 0 };
    for (uint8_t i = 0; i < targets.count; ++i)
    {
      const uint8_t code{ targets.pins[i] };
      result |= isGroup(code) ? static_cast< uint16_t >(groupMask(code)) : PinList< 1 >{ pinOf(code) }.toLocalBitmask();
    }
    return result;
  }

  /** @brief The remote loads that @p targets force, as a bitmask (bit 0 = remote load 0). */
  template< uint8_t MaxPins >
  constexpr uint8_t remoteBitmask(const PinList< MaxPins >& targets) const
  {
    uint8_t result{ 0 };
    for (uint8_t i = 0; i < targets.count; ++i)
    {
      const uint8_t code{ targets.pins[i] };
      result |= isGroup(code) ? static_cast< uint8_t >(groupMask(code) >> 16) : PinList< 1 >{ pinOf(code) }.toRemoteBitmask();
    }
    return result;
  }

private:
  /** @brief The pins of the relays, none without relay diversion. */
  constexpr uint32_t relaysMask() const
  {
    uint32_t mask{ 0 };
    if (relayDiversion_)
    {
      for (uint8_t i = 0; i < NR; ++i)
      {
        bit_set(mask, relays_.get_relay(i).pin);
      }
    }
    return mask;
  }

  const uint8_t (&loadMap_)[NL];       /**< the load map */
  const RelayEngine< NR, D >& relays_; /**< the relays */
  const bool relayDiversion_;          /**< whether the relays are in use */
};

/**
 * @class OverrideTable
 * @brief Manages override pins and their associated bitmasks for forced operation.
 *
 * This class provides compile-time mapping between override pins and the loads/relays they control.
 * Each pin can be associated with a set of pins (loads/relays), represented as separate bitmasks
 * for local loads/relays and remote loads.
 *
 * @tparam N Number of pin-index pairs (entries).
 * @tparam MaxPins Maximum number of pins supported (loads + relays). Default is 16.
 */
template< uint8_t N, uint8_t MaxPins = 16 >
class OverrideTable
{
private:
  /**
   * @struct Entry
   * @brief Internal structure representing a pin and its associated bitmasks.
   *
   * @var Entry::pin
   * Pin number for override control.
   * @var Entry::localBitmask
   * Bitmask representing the local loads/relays (physical pins 2-13) controlled by this pin.
   * @var Entry::remoteBitmask
   * Bitmask representing the remote loads (bit 0 = remote load 0, etc.) controlled by this pin.
   */
  struct Entry
  {
    uint8_t pin;           /**< Pin value. */
    uint16_t localBitmask; /**< Bitmask for local loads/relays. */
    uint8_t remoteBitmask; /**< Bitmask for remote loads. */
  };

  const Entry entries_[N]; /**< Array of entries for all pin-index pairs. */

public:
  /**
   * @brief Constructor. Initializes the override pin mapping from pin-index pairs.
   *
   * @param pairs Array of pin-index pairs, each specifying a pin and its associated pins.
   */
  constexpr OverrideTable(const KeyIndexPair< MaxPins > (&pairs)[N])
    : entries_{}  // default initialize
  {
    Entry temp[N]{};
    for (uint8_t i = 0; i < N; ++i)
    {
      temp[i] = { pairs[i].pin, pairs[i].getLocalBitmask(), pairs[i].getRemoteBitmask() };
    }
    for (uint8_t i = 0; i < N; ++i)
    {
      const_cast< Entry& >(entries_[i]) = temp[i];
    }
  }

  /**
   * @brief Constructor. The override pins of config.h, their codes turned into pins by @p map.
   *
   * @param pins The override pins, as in config.h.
   * @param map What the codes stand for (a TargetMap).
   */
  template< typename Map >
  constexpr OverrideTable(const OverridePins< N, MaxPins >& pins, const Map& map)
    : entries_{}  // default initialize
  {
    Entry temp[N]{};
    for (uint8_t i = 0; i < N; ++i)
    {
      temp[i] = { pins.getPin(i), map.localBitmask(pins.getTargets(i)), map.remoteBitmask(pins.getTargets(i)) };
    }
    for (uint8_t i = 0; i < N; ++i)
    {
      const_cast< Entry& >(entries_[i]) = temp[i];
    }
  }

  /**
   * @brief Returns the number of override pin entries.
   * @return Number of pin-index pairs (entries).
   */
  constexpr uint8_t size() const
  {
    return N;
  }

  /**
   * @brief Returns the pin number at the specified entry index.
   * @param index Index in the entries array.
   * @return Pin value, or 0 if out of bounds.
   */
  constexpr uint8_t getPin(uint8_t index) const
  {
    return index < N ? entries_[index].pin : 0;
  }

  /**
   * @brief Returns the local bitmask for the specified entry index.
   * @param index Index in the entries array.
   * @return Local bitmask for physical pins, or 0 if out of bounds.
   */
  constexpr uint16_t getLocalBitmask(uint8_t index) const
  {
    return index < N ? entries_[index].localBitmask : 0;
  }

  /**
   * @brief Returns the remote bitmask for the specified entry index.
   * @param index Index in the entries array.
   * @return Remote bitmask (bit n = remote load n), or 0 if out of bounds.
   */
  constexpr uint8_t getRemoteBitmask(uint8_t index) const
  {
    return index < N ? entries_[index].remoteBitmask : 0;
  }

  /**
   * @brief Finds the local bitmask associated with a given pin number.
   * @param pin Pin value to search for.
   * @return Local bitmask for the pin, or 0 if not found.
   */
  constexpr uint16_t findLocalBitmask(uint8_t pin) const
  {
    for (uint8_t i = 0; i < N; ++i)
    {
      if (entries_[i].pin == pin)
      {
        return entries_[i].localBitmask;
      }
    }
    return 0;
  }

  /**
   * @brief Finds the remote bitmask associated with a given pin number.
   * @param pin Pin value to search for.
   * @return Remote bitmask for the pin, or 0 if not found.
   */
  constexpr uint8_t findRemoteBitmask(uint8_t pin) const
  {
    for (uint8_t i = 0; i < N; ++i)
    {
      if (entries_[i].pin == pin)
      {
        return entries_[i].remoteBitmask;
      }
    }
    return 0;
  }

#ifdef ARDUINO
  /**
   * @brief Print the configured override pins and their bitmasks to Serial during startup.
   *
   * This method outputs the pin number and associated bitmasks (local and remote) for each entry.
   */
  void printOverrideConfig() const
  {
    infoln(F("*** Override Pins Configuration ***"));
    for (uint8_t i = 0; i < N; ++i)
    {
      info(F("\tPin: "));
      info(entries_[i].pin);
      info(F("\tLocal: 0b"));
      info(entries_[i].localBitmask, BIN);
      info(F("\tRemote: 0b"));
      infoln(entries_[i].remoteBitmask, BIN);
    }
  }
#endif
};

/**
 * @brief Deduction guide for OverrideTable, from plain pins.
 * Allows template argument deduction from constructor arguments.
 */
template< uint8_t MaxPins, uint8_t N >
OverrideTable(const KeyIndexPair< MaxPins > (&)[N])
  -> OverrideTable< N, MaxPins >;

#endif /* UTILS_OVERRIDE_H */
