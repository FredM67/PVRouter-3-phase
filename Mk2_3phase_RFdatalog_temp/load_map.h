/**
 * @file load_map.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Encoding of the load map: one packed byte per dump load
 * @version 1.0
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2025-2026
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
 */
constexpr uint8_t remote(uint8_t unit, uint8_t ledPin = 0)
{
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
}  // namespace Load

#endif  // LOAD_MAP_H
