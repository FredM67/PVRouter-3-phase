/**
 * @file utils_pins.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions for pins manipulation
 * @version 0.1
 * @date 2026-11-24
 *
 * @copyright Copyright (c) 2023-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef UTILS_PINS_H
#define UTILS_PINS_H

#include <Arduino.h>

#include "utils_bits.h"

#if defined(__DOXYGEN__)
inline constexpr void togglePin(uint8_t pin);

inline constexpr void setPinON(uint8_t pin);

inline constexpr void setPinOFF(uint8_t pin);
template< uint16_t usedPins >
inline void setPinsState(uint16_t pinsON, uint16_t pinsOFF);

inline constexpr bool getPinState(uint8_t pin);

template< uint16_t pins >
inline void setPinsAsOutput();
template< uint16_t pins >
inline void setPinsAsInputPullup();
#else
inline constexpr void togglePin(uint8_t pin) __attribute__((always_inline));

inline constexpr void setPinON(uint8_t pin) __attribute__((always_inline));

inline constexpr void setPinOFF(uint8_t pin) __attribute__((always_inline));
template< uint16_t usedPins >
inline void setPinsState(uint16_t pinsON, uint16_t pinsOFF) __attribute__((always_inline));

inline constexpr bool getPinState(uint8_t pin) __attribute__((always_inline));

template< uint16_t pins >
inline void setPinsAsOutput() __attribute__((always_inline));
template< uint16_t pins >
inline void setPinsAsInputPullup() __attribute__((always_inline));
#endif

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
 *
 * @details For a pin only known at run time (a relay). The ADC ISR writes the load pins,
 *          possibly on the same port, so the port must not be read-modified-written here:
 *          the pin is toggled through PINx instead, where writing a 1 toggles that bit of
 *          PORTx and writing a 0 leaves the others alone. Only the caller changes this pin,
 *          so reading its state first is safe.
 */
inline void setPinState(const uint8_t pin, const bool bState)
{
  const volatile uint8_t &port{ (pin < 8) ? PORTD : ((pin < 14) ? PORTB : PORTC) };
  volatile uint8_t &toggle{ (pin < 8) ? PIND : ((pin < 14) ? PINB : PINC) };
  const uint8_t mask{ static_cast< uint8_t >(1U << ((pin < 8) ? pin : ((pin < 14) ? pin - 8 : pin - 14))) };

  if (static_cast< bool >(port & mask) != bState)
  {
    toggle = mask;
  }
}

/**
 * @brief Set the Pin state to ON for the specified pin
 *
 * @param pin pin to change [2..13]
 *
 * @warning Atomic (a single sbi/cbi) only for a pin known at compile time. For a pin known at
 *          run time it is a read-modify-write of the port: from loop(), use setPinState().
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
 * @brief Set the Pin state to OFF for the specified pin
 *
 * @param pin pin to change [2..13]
 *
 * @warning Atomic (a single sbi/cbi) only for a pin known at compile time. For a pin known at
 *          run time it is a read-modify-write of the port: from loop(), use setPinState().
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
 * @brief Switch pins ON and OFF, with one write per port
 *
 * @tparam usedPins All the pins this call may ever change, known at compile time
 * @param pinsON The pins to switch ON
 * @param pinsOFF The pins to switch OFF (none of them in pinsON)
 *
 * @details The ports are volatile: each `PORTD |= …` is its own read and write, even when
 *          another one follows or the mask is 0. So each port is read and written once, and a
 *          port none of @p usedPins is on (PORTB with every pin on 2..7) is not touched at all.
 *
 * @warning Read-modify-write of PORTD and PORTB: only safe from the ADC ISR (its sole caller),
 *          which cannot be interrupted. From loop(), use setPinState() or togglePin(): an ISR
 *          landing in the middle would have its load pin changes written back to the old state.
 */
template< uint16_t usedPins >
inline void setPinsState(const uint16_t pinsON, const uint16_t pinsOFF)
{
  if constexpr (lowByte(usedPins) != 0)
  {
    PORTD = static_cast< uint8_t >((PORTD & ~lowByte(pinsOFF)) | lowByte(pinsON));
  }
  if constexpr (highByte(usedPins) != 0)
  {
    PORTB = static_cast< uint8_t >((PORTB & ~highByte(pinsOFF)) | highByte(pinsON));
  }
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
 * @tparam pins The pins to set as OUTPUT, known at compile time
 *
 * @details The registers are volatile: `DDRB |= 0` is still a read and a write. A port none of
 *          @p pins is on is not touched at all.
 */
template< uint16_t pins >
inline void setPinsAsOutput()
{
  if constexpr (lowByte(pins) != 0)
  {
    DDRD |= lowByte(pins);
  }
  if constexpr (highByte(pins) != 0)
  {
    DDRB |= highByte(pins);
  }
}

/**
 * @brief Set the pins as INPUT_PULLUP
 *
 * @tparam pins The pins to set as INPUT_PULLUP, known at compile time
 *
 * @details A port none of @p pins is on is not touched at all (see setPinsAsOutput()).
 */
template< uint16_t pins >
inline void setPinsAsInputPullup()
{
  if constexpr (lowByte(pins) != 0)
  {
    DDRD &= static_cast< uint8_t >(~lowByte(pins));  // input
    PORTD |= lowByte(pins);                          // pull-up on
  }
  if constexpr (highByte(pins) != 0)
  {
    DDRB &= static_cast< uint8_t >(~highByte(pins));
    PORTB |= highByte(pins);
  }
}

#endif /* UTILS_PINS_H */
