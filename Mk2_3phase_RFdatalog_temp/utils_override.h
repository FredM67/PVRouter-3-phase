/**
 * @file utils_override.h
 * @brief Compile-time utilities for managing override pins and index-to-bitmask mapping.
 *
 * This header provides types and functions for representing and manipulating sets of override pins
 * and their associated indices, all at compile time. It enables efficient bitmask computation and
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
 */
constexpr uint8_t RELAY(uint8_t relayNum)
{
  static_assert(RELAY_DIVERSION, "RELAY_DIVERSION must be true to use RELAY()");
  return relays.get_relay(relayNum).get_pin();
}

/**
 * @brief Helper to convert indices to a bitmask at compile-time.
 * @tparam Indices List of indices to set in the bitmask.
 * @return Bitmask with bits set at the specified indices.
 */
template< uint8_t... Indices >
constexpr uint16_t indicesToBitmask()
{
  return ((1U << Indices) | ...);
}

/**
 * @brief Wrapper for a list of indices, constructible from variadic arguments.
 * @tparam MaxIndices Maximum number of indices supported.
 */
template< uint8_t MaxIndices >
struct IndexList
{
  static_assert((!RELAY_DIVERSION && MaxIndices <= NO_OF_DUMPLOADS) || RELAY_DIVERSION,
                "You specified too many loads, must be <= NO_OF_DUMPLOADS (relay diversion OFF)");
  static_assert((RELAY_DIVERSION && MaxIndices <= NO_OF_DUMPLOADS + relays.size()) || !RELAY_DIVERSION,
                "You specified too many loads, must be <= NO_OF_DUMPLOADS + relays.size() (relay diversion ON)");

  uint8_t indices[MaxIndices];
  uint8_t count;

  /**
   * @brief Default constructor. Initializes with zero indices.
   */
  constexpr IndexList()
    : indices{}, count(0) {}

  /**
   * @brief Variadic constructor. Initializes with provided indices.
   * @param args List of indices.
   */
  template< typename... Args >
  constexpr IndexList(Args... args)
    : indices{ static_cast< uint8_t >(args)... }, count(sizeof...(args)) {}

  /**
   * @brief Converts the index list to a bitmask.
   * @return Bitmask with bits set at the specified indices.
   */
  constexpr uint16_t toBitmask() const
  {
    uint16_t result = 0;
    for (uint8_t i = 0; i < count; ++i)
    {
      result |= (1U << indices[i]);
    }
    return result;
  }
};

/**
 * @brief Structure holding a pin and its associated index list.
 * @tparam MaxIndices Maximum number of indices supported.
 */
template< uint8_t MaxIndices >
struct KeyIndexPair
{
  uint8_t pin;
  IndexList< MaxIndices > indexList;

  /**
   * @brief Constructor.
   * @param k Pin value.
   * @param list Index list.
   */
  constexpr KeyIndexPair(uint8_t k, const IndexList< MaxIndices >& list)
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
 * @brief Main class for managing override pins and their associated bitmasks.
 * @tparam N Number of pin-index pairs.
 * @tparam MaxIndices Maximum number of indices supported.
 */
template< uint8_t N, uint8_t MaxIndices = NO_OF_DUMPLOADS >
class OverridePins
{
private:
  /**
   * @brief Structure representing a pin and its bitmask.
   */
  struct Entry
  {
    uint8_t pin;      /**< Pin value. */
    uint16_t bitmask; /**< Bitmask for indices. */
  };

  const Entry entries_[N]; /**< Array of entries for all pin-index pairs. */

public:
  /**
     * @brief Constructor. Initializes entries from pin-index pairs.
     * @param pairs Array of pin-index pairs.
     */
  constexpr OverridePins(const KeyIndexPair< MaxIndices > (&pairs)[N])
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
     * @brief Returns the number of pin-index pairs.
     * @return Number of pairs.
     */
  constexpr uint8_t size() const
  {
    return N;
  }

  /**
     * @brief Returns the pin at the specified index.
     * @param index Index in the array.
     * @return Pin value, or 0 if out of bounds.
     */
  constexpr uint8_t getPin(uint8_t index) const
  {
    return index < N ? entries_[index].pin : 0;
  }

  /**
     * @brief Returns the bitmask at the specified index.
     * @param index Index in the array.
     * @return Bitmask, or 0 if out of bounds.
     */
  constexpr uint16_t getBitmask(uint8_t index) const
  {
    return index < N ? entries_[index].bitmask : 0;
  }

  /**
   * @brief Finds the bitmask for a given pin.
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
   */
  void printStatus() const
  {
    Serial.println(F("[OverridePins] Configured pins and bitmasks:"));
    for (uint8_t i = 0; i < N; ++i)
    {
      Serial.print(F("  Pin: "));
      Serial.print(entries_[i].pin);
      Serial.print(F("  Bitmask: 0b"));
      Serial.println(entries_[i].bitmask, BIN);
    }
  }
};

/**
 * @brief Deduction guide for OverridePins template.
 * Allows template argument deduction from constructor arguments.
 */
template< uint8_t MaxIndices, uint8_t N >
OverridePins(const KeyIndexPair< MaxIndices > (&)[N])
  -> OverridePins< N, MaxIndices >;

#endif /* UTILS_OVERRIDE_H */
