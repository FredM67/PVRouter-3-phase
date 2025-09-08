/**
 * @file utils_override.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-09-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef UTILS_OVERRIDE_H
#define UTILS_OVERRIDE_H

#include "config.h"

// Helper to convert indices to bitmask at compile-time
template< uint8_t... Indices >
constexpr uint16_t indicesToBitmask()
{
  return ((1U << Indices) | ...);
}

// Wrapper for index list that can be constructed from initializer_list syntax
template< uint8_t MaxIndices >
struct IndexList
{
  static_assert(MaxIndices <= NO_OF_DUMPLOADS, "You specified too many loads, must be <= NO_OF_DUMPLOADS");

  uint8_t indices[MaxIndices];
  uint8_t count;

  constexpr IndexList() : indices{}, count(0) {}

  template< typename... Args >
  constexpr IndexList(Args... args) : indices{ static_cast< uint8_t >(args)... }, count(sizeof...(args)) {}

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

// Pair structure that holds key and index list
template< uint8_t MaxIndices >
struct KeyIndexPair
{
  uint8_t key;
  IndexList< MaxIndices > indexList;

  constexpr KeyIndexPair(uint8_t k, const IndexList< MaxIndices >& list)
    : key(k), indexList(list) {}

  constexpr uint16_t getBitmask() const
  {
    return indexList.toBitmask();
  }
};

// Main class
template< uint8_t N, uint8_t MaxIndices = NO_OF_DUMPLOADS >
class OverridePins
{
private:
  struct Entry
  {
    uint8_t key;
    uint16_t bitmask;
  };

  const Entry entries_[N];

public:
  constexpr OverridePins(const KeyIndexPair< MaxIndices > (&pairs)[N])
    : entries_{}  // default initialize
  {
    Entry temp[N]{};
    for (uint8_t i = 0; i < N; ++i)
    {
      temp[i] = { pairs[i].key, pairs[i].getBitmask() };
    }
    for (uint8_t i = 0; i < N; ++i)
    {
      const_cast< Entry& >(entries_[i]) = temp[i];
    }
  }

  constexpr uint8_t size() const
  {
    return N;
  }

  constexpr uint8_t getKey(uint8_t index) const
  {
    return index < N ? entries_[index].key : 0;
  }

  constexpr uint16_t getBitmask(uint8_t index) const
  {
    return index < N ? entries_[index].bitmask : 0;
  }

  constexpr uint16_t findBitmask(uint8_t key) const
  {
    for (uint8_t i = 0; i < N; ++i)
    {
      if (entries_[i].key == key)
      {
        return entries_[i].bitmask;
      }
    }
    return 0;
  }
};

// Deduction guide
template< uint8_t MaxIndices, uint8_t N >
OverridePins(const KeyIndexPair< MaxIndices > (&)[N])
  -> OverridePins< N, MaxIndices >;

#endif /* UTILS_OVERRIDE_H */
