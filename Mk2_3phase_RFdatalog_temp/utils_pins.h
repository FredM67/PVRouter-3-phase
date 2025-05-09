/**
 * @file utils_pins.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions for pins manipulation
 * @version 0.1
 * @date 2023-05-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef UTILS_PINS_H
#define UTILS_PINS_H

#include <Arduino.h>

inline constexpr uint8_t unused_pin{ 0xff }; /**< unused pin */

#if defined(__DOXYGEN__)
inline constexpr void togglePin(uint8_t pin);

inline constexpr void setPinON(uint8_t pin);
inline void setPinsON(uint16_t pins);

inline constexpr void setPinOFF(uint8_t pin);
inline void setPinsOFF(uint16_t pins);

inline constexpr bool getPinState(uint8_t pin);

inline void setPinsAsOutput(uint16_t pins);
inline void setPinsAsInputPullup(uint16_t pins);
#else
inline constexpr void togglePin(uint8_t pin) __attribute__((always_inline));

inline constexpr void setPinON(uint8_t pin) __attribute__((always_inline));
inline void setPinsON(uint16_t pins) __attribute__((always_inline));

inline constexpr void setPinOFF(uint8_t pin) __attribute__((always_inline));
inline void setPinsOFF(uint16_t pins) __attribute__((always_inline));

inline constexpr bool getPinState(uint8_t pin) __attribute__((always_inline));

inline void setPinsAsOutput(uint16_t pins) __attribute__((always_inline));
inline void setPinsAsInputPullup(uint16_t pins) __attribute__((always_inline));
#endif

/**
 * @brief Set the specified bit to 1
 * 
 * @tparam T Type of the variable
 * @param _dest Integer variable to modify
 * @param bit Bit to set in _dest
 */
template< typename T > constexpr void bit_set(T& _dest, const uint8_t bit)
{
  _dest |= (T)0x01 << bit;
}

/**
 * @brief Read the specified bit
 * 
 * @tparam T Type of the variable
 * @param _src Integer variable to read
 * @param bit Bit to read in _src
 * @return constexpr uint8_t 
 */
template< typename T > constexpr uint8_t bit_read(const T& _src, const uint8_t bit)
{
  return (_src >> bit) & (T)0x01;
}

/**
 * @brief Clear the specified bit
 * 
 * @tparam T Type of the variable
 * @param _dest Integer variable to modify
 * @param bit Bit to clear in _src
 * @return constexpr uint8_t 
 */
template< typename T > constexpr uint8_t bit_clear(T& _dest, const uint8_t bit)
{
  return _dest &= ~((T)0x01 << bit);
}

/**
 * @brief Toggle the specified pin
 *
 */
void constexpr togglePin(const uint8_t pin)
{
  if (pin < 8)
  {
    bit_set(PIND, pin);
  }
  else if (pin < 14)
  {
    bit_set(PINB, pin - 8);
  }
  else
  {
    bit_set(PINC, pin - 14);
  }
}

/**
 * @brief Set the Pin state for the specified pin
 *
 * @param pin pin to change [2..13]
 * @param bState state to be set
 */
inline constexpr void setPinState(const uint8_t pin, const bool bState)
{
  if (bState)
  {
    setPinON(pin);
  }
  else
  {
    setPinOFF(pin);
  }
}

/**
 * @brief Set the Pin state to ON for the specified pin
 *
 * @param pin pin to change [2..13]
 */
inline constexpr void setPinON(const uint8_t pin)
{
  if (pin < 8)
  {
    bit_set(PORTD, pin);
  }
  else if (pin < 14)
  {
    bit_set(PORTB, pin - 8);
  }
  else
  {
    bit_set(PORTC, pin - 14);
  }
}

/**
 * @brief Set the Pins state to ON
 *
 * @param pins The pins to change
 */
inline void setPinsON(const uint16_t pins)
{
  PORTD |= lowByte(pins);
  PORTB |= highByte(pins);
}

/**
 * @brief Set the Pin state to OFF for the specified pin
 *
 * @param pin pin to change [2..13]
 */
inline constexpr void setPinOFF(const uint8_t pin)
{
  if (pin < 8)
  {
    bit_clear(PORTD, pin);
  }
  else if (pin < 14)
  {
    bit_clear(PORTB, pin - 8);
  }
  else
  {
    bit_clear(PORTC, pin - 14);
  }
}

/**
 * @brief Set the Pins state to OFF
 *
 * @param pins The pins to change
 */
inline void setPinsOFF(const uint16_t pins)
{
  PORTD &= ~lowByte(pins);
  PORTB &= ~highByte(pins);
}

/**
 * @brief Get the Pin State
 *
 * @param pin The pin to read
 * @return true if HIGH
 * @return false if LOW
 */
inline constexpr bool getPinState(const uint8_t pin)
{
  if (pin < 8)
  {
    return bit_read(PIND, pin);
  }
  else if (pin < 14)
  {
    return bit_read(PINB, pin - 8);
  }
  else
  {
    return bit_read(PINC, pin - 14);
  }
}

/**
 * @brief Set the pins as OUTPUT
 *
 * @param pins The pins to set as OUTPUT
 */
inline void setPinsAsOutput(const uint16_t pins)
{
  DDRD |= lowByte(pins);
  DDRB |= highByte(pins);
}

/**
 * @brief Set the pins as INPUT_PULLUP
 *
 * @param pins The pins to set as INPUT_PULLUP
 */
inline void setPinsAsInputPullup(const uint16_t pins)
{
  // Set pins as input
  DDRD &= ~lowByte(pins);
  DDRB &= ~highByte(pins);

  // Enable pull-up resistors
  PORTD |= lowByte(pins);
  PORTB |= highByte(pins);
}

#endif /* UTILS_PINS_H */
