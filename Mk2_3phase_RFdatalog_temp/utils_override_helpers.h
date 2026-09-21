/**
 * @file utils_override_helpers.h
 * @brief Config-dependent helper functions for override pin configuration
 *
 * This file provides convenience functions like LOAD(), RELAY(), ALL_LOADS(), etc.
 * that depend on configuration values defined in config.h.
 *
 * IMPORTANT: This file must be included AFTER physicalLoadPin[], relays,
 * NO_OF_DUMPLOADS, and RELAY_DIVERSION are defined in config.h.
 *
 * @author Frederic Metrich (frederic.metrich@live.fr)
 * @version 0.1
 * @date 2026-01-28
 * @copyright Copyright (c) 2025-2026
 */

#ifndef UTILS_OVERRIDE_HELPERS_H
#define UTILS_OVERRIDE_HELPERS_H

#include "utils_override.h"

/**
 * @brief Returns the pin number for a given load index at compile time.
 * @param loadNum The load index (0-based).
 * @return The pin number for the load.
 */
constexpr uint8_t LOAD(uint8_t loadNum)
{
  return physicalLoadPin[loadNum];
}

/**
 * @brief Returns the pin number for a given relay index at compile time.
 * @param relayNum The relay index (0-based).
 * @return The pin number for the relay.
 * @note Only use when RELAY_DIVERSION is enabled.
 */
constexpr uint8_t RELAY(uint8_t relayNum)
{
  return relays.get_relay(relayNum).get_pin();
}

/**
 * @brief Returns a bitmask representing all load pins.
 * @return Bitmask with all load pins set.
 */
constexpr uint16_t ALL_LOADS()
{
  uint16_t mask{ 0 };
  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    bit_set(mask, physicalLoadPin[i]);
  }
  return mask;
}

/**
 * @brief Returns a bitmask representing all relay pins.
 * @return Bitmask with all relay pins set, or 0 if RELAY_DIVERSION is disabled.
 */
constexpr uint16_t ALL_RELAYS()
{
  if constexpr (RELAY_DIVERSION)
  {
    uint16_t mask{ 0 };
    for (uint8_t i = 0; i < relays.size(); ++i)
    {
      bit_set(mask, relays.get_relay(i).get_pin());
    }
    return mask;
  }
  else
  {
    return 0;
  }
}

/**
 * @brief Returns a bitmask representing all loads and all relays.
 * @return Bitmask with all load and relay pins set.
 */
constexpr uint16_t ALL_LOADS_AND_RELAYS()
{
  return ALL_LOADS() | ALL_RELAYS();
}

#endif /* UTILS_OVERRIDE_HELPERS_H */
