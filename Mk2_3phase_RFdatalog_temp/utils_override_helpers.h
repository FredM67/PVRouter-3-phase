/**
 * @file utils_override_helpers.h
 * @brief Config-dependent helper functions for override pin configuration
 *
 * This file provides convenience functions like LOAD(), RELAY(), ALL_LOADS(), etc.
 * that depend on configuration values defined in config.h.
 *
 * IMPORTANT: This file must be included AFTER physicalLoadPin[], relays,
 * NO_OF_DUMPLOADS, NO_OF_REMOTE_LOADS, and RELAY_DIVERSION are defined in config.h.
 *
 * @author Frederic Metrich (frederic.metrich@live.fr)
 * @version 0.2
 * @date 2026-01-29
 * @copyright Copyright (c) 2025-2026
 */

#ifndef UTILS_OVERRIDE_HELPERS_H
#define UTILS_OVERRIDE_HELPERS_H

#include "utils_override.h"

/**
 * @brief Returns the physical pin number for a LOCAL load.
 * @param loadNum The local load index (0-based).
 * @return The pin number for the local load.
 */
constexpr uint8_t LOCAL_LOAD(uint8_t loadNum)
{
  return physicalLoadPin[loadNum];
}

/**
 * @brief Returns the virtual pin number for a REMOTE load.
 * @param loadNum The remote load index (0-based).
 * @return The virtual pin number (>= REMOTE_PIN_BASE).
 */
constexpr uint8_t REMOTE_LOAD(uint8_t loadNum)
{
  return REMOTE_PIN_BASE + loadNum;
}

/**
 * @brief Returns the pin number for any load (local or remote).
 * @param loadNum The load index (0-based, local loads first, then remote).
 * @return Physical pin for local loads, virtual pin for remote loads.
 */
constexpr uint8_t LOAD(uint8_t loadNum)
{
  constexpr uint8_t numLocalLoads = NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS;
  if (loadNum < numLocalLoads)
  {
    return physicalLoadPin[loadNum];
  }
  else
  {
    return REMOTE_PIN_BASE + (loadNum - numLocalLoads);
  }
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
 * @brief Returns a bitmask representing all LOCAL load pins.
 * @return uint32_t with lower 16 bits set for local load pins.
 */
constexpr uint32_t ALL_LOCAL_LOADS()
{
  uint32_t mask{ 0 };
  for (uint8_t i = 0; i < (NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS); ++i)
  {
    bit_set(mask, physicalLoadPin[i]);
  }
  return mask;
}

/**
 * @brief Returns a bitmask representing all REMOTE loads.
 * @return uint32_t with upper 16 bits set for remote loads (bit 16 = remote 0).
 */
constexpr uint32_t ALL_REMOTE_LOADS()
{
  uint32_t mask{ 0 };
  for (uint8_t i = 0; i < NO_OF_REMOTE_LOADS; ++i)
  {
    bit_set(mask, 16 + i);  // Set bit 16+i for remote load i
  }
  return mask;
}

/**
 * @brief Returns a bitmask representing all loads (local + remote).
 * @return uint32_t with local pins in lower 16 bits, remote in upper 16 bits.
 */
constexpr uint32_t ALL_LOADS()
{
  return ALL_LOCAL_LOADS() | ALL_REMOTE_LOADS();
}

/**
 * @brief Returns a bitmask representing all relay pins.
 * @return uint32_t with relay pins set in lower 16 bits, or 0 if disabled.
 */
constexpr uint32_t ALL_RELAYS()
{
  if constexpr (RELAY_DIVERSION)
  {
    uint32_t mask{ 0 };
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
 * @return uint32_t with all load and relay bits set.
 */
constexpr uint32_t ALL_LOADS_AND_RELAYS()
{
  return ALL_LOADS() | ALL_RELAYS();
}

#endif /* UTILS_OVERRIDE_HELPERS_H */
