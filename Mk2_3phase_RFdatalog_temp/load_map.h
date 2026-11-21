/**
 * @file load_map.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Encoding of the load map: one packed byte per dump load
 * @version 1.0
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2025-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details Each entry of @c physicalLoadPin[] describes one dump load:
 *          - bits 6-7 : owning unit. 0 = local (driven by the router itself),
 *                       1..3 = remote unit number, reached over RF.
 *          - bits 0-5 : physical pin. For a local load this is the TRIAC pin.
 *                       For a remote load this is an *optional* status LED pin
 *                       on the router, 0 meaning "no LED".
 *
 *          Loads may be declared in any order; nothing assumes locals come first.
 *
 * @note This header is deliberately free of any Arduino dependency (its whole
 *       include closure is @c <stdint.h>) so the encoding and the census
 *       helpers can be exercised by the native test suite.
 */

#ifndef LOAD_MAP_H
#define LOAD_MAP_H

#include <stdint.h>

/** Load state (for use if loads are active high (Rev 2 PCB)) */
enum class LoadStates : uint8_t
{
  LOAD_OFF, /**< load is OFF */
  LOAD_ON   /**< load is ON */
};
// enum loadStates {LOAD_ON, LOAD_OFF}; /**< for use if loads are active low (original PCB) */

inline constexpr uint8_t loadTypeMask{ 0xC0 }; /**< bit mask of the owning-unit field */
inline constexpr uint8_t loadPinMask{ 0x3F };  /**< bit mask of the pin field */
inline constexpr uint8_t loadTypeShift{ 6 };   /**< position of the owning-unit field */

inline constexpr uint8_t MAX_REMOTE_UNITS{ 3 };   /**< only 2 bits are available for the unit number */
inline constexpr uint8_t MAX_LOADS_PER_UNIT{ 8 }; /**< one payload byte per unit, 1 bit per load */

/**
 * @brief The entry Load::remote() returns for a unit outside 1..MAX_REMOTE_UNITS.
 *
 * @details Packed as is, unit 4 would wrap to 0 (local) and remote(4, 8) would be local(8): a
 *          TRIAC on the LED pin. A static_assert cannot test a parameter (and C++17 has no
 *          consteval), so remote() returns this marker and validation.h reports it, with its
 *          own message (Load::hasValidRemoteUnits()).
 *
 *          It decodes as a remote load (unit 3) with a status LED on pin 62, which does not
 *          exist: the checks of local loads ignore it, isValidMap() and the pin check skip it,
 *          so a wrong unit gives one error, not a cascade.
 */
inline constexpr uint8_t invalidRemoteUnitEntry{ 0xFE };

/**
 * @brief Authoring and decoding helpers for the packed load map.
 */
namespace Load
{
/**
 * @brief Declares a load driven locally by the router.
 * @param pin The digital pin of the TRIAC output.
 */
constexpr uint8_t local(uint8_t pin)
{
  return pin & loadPinMask;
}

/**
 * @brief Declares a load driven by a remote unit over RF.
 * @param unit The remote unit number (1..3).
 * @param ledPin Optional status LED pin on the router, 0 for none.
 * @return The packed entry, or invalidRemoteUnitEntry if the unit is not 1..MAX_REMOTE_UNITS.
 */
constexpr uint8_t remote(uint8_t unit, uint8_t ledPin = 0)
{
  if ((unit == 0) || (unit > MAX_REMOTE_UNITS))
  {
    return invalidRemoteUnitEntry;
  }
  return static_cast< uint8_t >(static_cast< uint8_t >(unit << loadTypeShift) | (ledPin & loadPinMask));
}

/** @brief True if the entry describes a locally driven load. */
constexpr bool isLocal(uint8_t entry)
{
  return (entry & loadTypeMask) == 0;
}

/** @brief The owning unit of the entry: 0 for a local load, 1..3 for a remote one. */
constexpr uint8_t unitOf(uint8_t entry)
{
  return static_cast< uint8_t >((entry & loadTypeMask) >> loadTypeShift);
}

/** @brief The pin of the entry: TRIAC pin if local, status LED (0 = none) if remote. */
constexpr uint8_t pinOf(uint8_t entry)
{
  return entry & loadPinMask;
}

/**
 * @brief Highest remote unit number appearing in the map (0 if fully local).
 */
template< uint8_t N >
constexpr uint8_t countUnits(const uint8_t (&map)[N])
{
  uint8_t highest{ 0 };
  for (uint8_t i = 0; i < N; ++i)
  {
    if (unitOf(map[i]) > highest)
    {
      highest = unitOf(map[i]);
    }
  }
  return highest;
}

/**
 * @brief Number of entries of the map that are driven remotely.
 */
template< uint8_t N >
constexpr uint8_t countRemoteLoads(const uint8_t (&map)[N])
{
  uint8_t count{ 0 };
  for (uint8_t i = 0; i < N; ++i)
  {
    if (!isLocal(map[i]))
    {
      ++count;
    }
  }
  return count;
}

/**
 * @brief Largest number of loads assigned to any single remote unit.
 */
template< uint8_t N >
constexpr uint8_t maxLoadsPerUnit(const uint8_t (&map)[N])
{
  uint8_t worst{ 0 };
  for (uint8_t unit = 1; unit <= countUnits(map); ++unit)
  {
    uint8_t count{ 0 };
    for (uint8_t i = 0; i < N; ++i)
    {
      if (unitOf(map[i]) == unit)
      {
        ++count;
      }
    }
    if (count > worst)
    {
      worst = count;
    }
  }
  return worst;
}

/**
 * @brief Rank of a remote load among the remote loads, in ascending map order.
 *
 * @details This is the ordinal used by the virtual-pin space of the override
 *          subsystem (@c REMOTE_PIN_BASE + ordinal) and by bit 16+ordinal of the
 *          @c ALL_REMOTE_LOADS() bitmask.
 *
 * @param map The load map.
 * @param loadNum Index into the map, which must designate a remote load.
 */
template< uint8_t N >
constexpr uint8_t remoteOrdinal(const uint8_t (&map)[N], uint8_t loadNum)
{
  uint8_t ordinal{ 0 };
  for (uint8_t i = 0; i < loadNum; ++i)
  {
    if (!isLocal(map[i]))
    {
      ++ordinal;
    }
  }
  return ordinal;
}

/**
 * @brief Rank of a load among the loads of its own unit, in ascending map order.
 *
 * @details This is the bit position of the load inside its unit's RF payload.
 */
template< uint8_t N >
constexpr uint8_t bitOf(const uint8_t (&map)[N], uint8_t loadNum)
{
  uint8_t bit{ 0 };
  for (uint8_t i = 0; i < loadNum; ++i)
  {
    if (unitOf(map[i]) == unitOf(map[loadNum]))
    {
      ++bit;
    }
  }
  return bit;
}

/**
 * @brief Override bit of each load, in the local or in the remote override bitmask.
 *
 * @details Built at compile time, so that the ISR tests a load's override with a mask
 *          instead of shifting by its pin (a loop on AVR) or counting its remote ordinal.
 */
template< uint8_t N >
struct OverrideMasks
{
  uint16_t local[N]; /**< bit of the load's pin in the local override bitmask, 0 for a remote load */
  uint8_t remote[N]; /**< bit of the load's ordinal in the remote override bitmask, 0 for a local load */
};

/**
 * @brief Build the override bits of a load map.
 *
 * @details An entry out of range (rejected by isValidMap() and the remote load count
 *          checks) gets no bit, so that the error reported is that one.
 *
 * @param map The load map.
 */
template< uint8_t N >
constexpr OverrideMasks< N > overrideMasks(const uint8_t (&map)[N])
{
  OverrideMasks< N > masks{};
  for (uint8_t i = 0; i < N; ++i)
  {
    if (isLocal(map[i]))
    {
      masks.local[i] = pinOf(map[i]) < 16 ? static_cast< uint16_t >(1U << pinOf(map[i])) : 0;
    }
    else
    {
      masks.remote[i] = remoteOrdinal(map, i) < 8 ? static_cast< uint8_t >(1U << remoteOrdinal(map, i)) : 0;
    }
  }
  return masks;
}

/**
 * @brief Check that every entry of a load map designates a usable pin.
 *
 * @details A local load needs a real TRIAC pin, 2..13 (0 and 1 are the serial
 *          interface, 14 and above do not exist). A remote load needs no pin, but its
 *          optional status LED, when present, obeys the same range.
 *
 *          This is also what catches an entry left at @c unused_pin: 0xFF decodes as
 *          unit 3 / pin 63, which is out of range.
 */
template< uint8_t N >
constexpr bool isValidMap(const uint8_t (&map)[N])
{
  for (const auto &entry : map)
  {
    const uint8_t pin{ pinOf(entry) };

    if (entry == invalidRemoteUnitEntry)
    {
      continue;  // a wrong unit, reported by hasValidRemoteUnits()
    }

    if (!isLocal(entry) && (pin == 0))
    {
      continue;  // remote load without status LED
    }

    if ((pin < 2) || (pin > 13))
    {
      return false;
    }
  }
  return true;
}

/**
 * @brief Check that every remote load of a map was declared with a unit from 1 to 3.
 *
 * @details Load::remote() returns invalidRemoteUnitEntry otherwise: see there.
 */
template< uint8_t N >
constexpr bool hasValidRemoteUnits(const uint8_t (&map)[N])
{
  for (const auto &entry : map)
  {
    if (entry == invalidRemoteUnitEntry)
    {
      return false;
    }
  }
  return true;
}
}  // namespace Load

#endif  // LOAD_MAP_H
