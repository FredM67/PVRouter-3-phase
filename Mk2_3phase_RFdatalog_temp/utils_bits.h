/**
 * @file utils_bits.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Bit manipulation utility functions
 * @version 0.1
 * @date 2026-01-29
 *
 * @copyright Copyright (c) 2025-2026
 *
 * @note These functions are pure C++ with no Arduino dependencies,
 *       making them usable in both embedded and native test environments.
 */

#ifndef UTILS_BITS_H
#define UTILS_BITS_H

#include <stdint.h>

/**
 * @brief Set the specified bit to 1
 *
 * @tparam T Type of the variable
 * @param _dest Integer variable to modify
 * @param bit Bit to set in _dest
 */
template< typename T >
constexpr void bit_set(T& _dest, const uint8_t bit)
{
  _dest |= static_cast< T >(1) << bit;
}

/**
 * @brief Read the specified bit
 *
 * @tparam T Type of the variable
 * @param _src Integer variable to read
 * @param bit Bit to read in _src
 * @return constexpr uint8_t
 */
template< typename T >
constexpr uint8_t bit_read(const T& _src, const uint8_t bit)
{
  return (_src >> bit) & static_cast< T >(1);
}

/**
 * @brief Clear the specified bit
 *
 * @tparam T Type of the variable
 * @param _dest Integer variable to modify
 * @param bit Bit to clear in _dest
 * @return constexpr uint8_t
 */
template< typename T >
constexpr uint8_t bit_clear(T& _dest, const uint8_t bit)
{
  return _dest &= ~(static_cast< T >(1) << bit);
}

#endif  // UTILS_BITS_H
