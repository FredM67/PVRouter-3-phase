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
 * Usage example:
 * @code
 * constexpr OverridePins overridePins{
 *     {2, {1, 3}},
 *     {3, {0, 2, 6}},
 *     {4, {5}}
 * };
 * constexpr OverridePins<3, 8> pins{pairs};
 * @endcode
 *
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @version 0.1
 * @date 2025-09-08
 * @copyright Copyright (c) 2025
 */

#ifndef UTILS_OVERRIDE_H
#define UTILS_OVERRIDE_H

#include "config.h"
#include "type_traits.hpp"

/**
 * @brief Returns the pin number for a given load index at compile time.
 * @param loadNum The load index (0-based).
 * @return The pin number for the load (local loads only, remote loads return unused_pin).
 * @note Remote loads (index >= NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS) have no physical pin.
 */
constexpr uint8_t LOAD(uint8_t loadNum)
{
  constexpr uint8_t numLocalLoads = NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS;
  if (loadNum < numLocalLoads)
  {
    return physicalLoadPin[loadNum];
  }
  // Remote loads have no physical pin on the main controller
  return unused_pin;
}

/**
 * @brief Returns the pin number for a given relay index at compile time.
 * @param relayNum The relay index (0-based).
 * @return The pin number for the relay.
 */
constexpr uint8_t RELAY(uint8_t relayNum)
{
  return relays.get_relay(relayNum).get_pin();
}

/**
 * @brief Returns a bitmask representing all load pins.
 *
 * This helper is used to configure an override pin to control all loads at once.
 * Note: Only includes LOCAL loads (physical pins), not remote loads.
 *
 * @return Bitmask with all local load pins set.
 */
constexpr uint16_t ALL_LOADS()
{
  uint16_t mask{ 0 };
  for (uint8_t i = 0; i < (NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS); ++i)
  {
    bit_set(mask, physicalLoadPin[i]);
  }
  return mask;
}

/**
 * @brief Returns a bitmask representing all relay pins.
 *
 * This helper is used to configure an override pin to control all relays at once.
 *
 * @return Bitmask with all relay pins set.
 */
constexpr uint16_t ALL_RELAYS()
{
  uint16_t mask{ 0 };
  for (uint8_t i = 0; i < relays.size(); ++i)
  {
    bit_set(mask, relays.get_relay(i).get_pin());
  }
  return mask;
}

/**
 * @brief Returns a bitmask representing all loads and all relays.
 *
 * This helper is used to configure an override pin to control the entire system (all loads and relays).
 *
 * @return Bitmask with all load and relay pins set.
 */
constexpr uint16_t ALL_LOADS_AND_RELAYS()
{
  return ALL_LOADS() | ALL_RELAYS();
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
  return (bit_read(validPinMask, Pins) && ...);
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
  uint16_t result = 0;
  (bit_set(result, Pins), ...);
  return result;
}

/**
 * @brief Wrapper for a list of pins, constructible from variadic arguments.
 * @tparam MaxPins Maximum number of pins supported.
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
   * @brief Constructor from bitmask. Sets pin numbers from bits set in bitmask.
   * @param bitmask Bitmask value.
   */
  constexpr PinList(uint16_t bitmask)
    : pins{}, count(0)
  {
    for (uint8_t pin = 0; pin < 16 && count < MaxPins; ++pin)
    {
      if (bit_read(bitmask, pin))
      {
        pins[count++] = pin;  // Store the pin number
      }
    }
  }

  /**
   * @brief Variadic constructor. Initializes with provided pins.
   * @param args List of pins.
   */
  template< typename... Args >
  constexpr PinList(Args... args)
    : pins{ static_cast< uint8_t >(args)... }, count(sizeof...(args)) {}

  /**
   * @brief Converts the pin list to a bitmask.
   * @return Bitmask with bits set at the specified pins.
   */
  constexpr uint16_t toBitmask() const
  {
    uint16_t result = 0;
    for (uint8_t i = 0; i < count; ++i)
    {
      bit_set(result, pins[i]);
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
   * @brief Returns the bitmask for the index list.
   * @return Bitmask.
   */
  constexpr uint16_t getBitmask() const
  {
    return indexList.toBitmask();
  }
};

/**
 * @class OverridePins
 * @brief Manages override pins and their associated bitmasks for forced operation.
 *
 * This class provides compile-time mapping between override pins and the loads/relays they control.
 * Each pin can be associated with a set of pins (loads/relays), represented as a bitmask.
 *
 * @tparam N Number of pin-index pairs (entries).
 * @tparam MaxPins Maximum number of pins supported (loads + relays).
 */
template< uint8_t N, uint8_t MaxPins = NO_OF_DUMPLOADS + relays.size() >
class OverridePins
{
private:
  /**
   * @struct Entry
   * @brief Internal structure representing a pin and its associated bitmask.
   *
   * @var Entry::pin
   * Pin number for override control.
   * @var Entry::bitmask
   * Bitmask representing the pins (loads/relays) controlled by this pin.
   */
  struct Entry
  {
    uint8_t pin;      /**< Pin value. */
    uint16_t bitmask; /**< Bitmask for pins. */
  };

  const Entry entries_[N]; /**< Array of entries for all pin-index pairs. */

public:
  /**
   * @brief Constructor. Initializes the override pin mapping from pin-index pairs.
   *
   * @param pairs Array of pin-index pairs, each specifying a pin and its associated pins.
   */
  constexpr OverridePins(const KeyIndexPair< MaxPins > (&pairs)[N])
    : entries_{}  // default initialize
  {
    Entry temp[N]{};
    for (uint8_t i = 0; i < N; ++i)
    {
      temp[i] = { pairs[i].pin, pairs[i].getBitmask() };
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
     * @brief Returns the bitmask for the specified entry index.
     * @param index Index in the entries array.
     * @return Bitmask, or 0 if out of bounds.
     */
  constexpr uint16_t getBitmask(uint8_t index) const
  {
    return index < N ? entries_[index].bitmask : 0;
  }

  /**
     * @brief Finds the bitmask associated with a given pin number.
     * @param pin Pin value to search for.
     * @return Bitmask for the pin, or 0 if not found.
     */
  constexpr uint16_t findBitmask(uint8_t pin) const
  {
    for (uint8_t i = 0; i < N; ++i)
    {
      if (entries_[i].pin == pin)
      {
        return entries_[i].bitmask;
      }
    }
    return 0;
  }

  /**
     * @brief Print the configured override pins and their bitmasks to Serial during startup.
     *
     * This method outputs the pin number and associated bitmask for each entry.
     */
  void printOverrideConfig() const
  {
    Serial.println(F("*** Override Pins Configuration ***"));
    for (uint8_t i = 0; i < N; ++i)
    {
      Serial.print(F("\tPin: "));
      Serial.print(entries_[i].pin);
      Serial.print(F("\tBitmask: 0b"));
      Serial.println(entries_[i].bitmask, BIN);
    }
  }
};

/**
 * @brief Deduction guide for OverridePins template.
 * Allows template argument deduction from constructor arguments.
 */
template< uint8_t MaxPins, uint8_t N >
OverridePins(const KeyIndexPair< MaxPins > (&)[N])
  -> OverridePins< N, MaxPins >;

#endif /* UTILS_OVERRIDE_H */
