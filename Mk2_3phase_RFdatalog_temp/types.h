/**
 * @file types.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some basics classes/types
 * @version 0.1
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2023-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

#include "type_traits.hpp"

#include "config_types.h"  // the enums of config.h (SerialOutputType, PcbVersion, RotationModes)

#include "constants.h"
#include "load_map.h"         // LoadStates and the packed load-map encoding
#include "load_priorities.h"  // loadStateMask, loadStateOnBit

// -------------------------------
// definitions of enumerated types

//--------------------------------------------------------------------------------------------------
/** Polarities */
enum class Polarities : uint8_t
{
  NEGATIVE, /**< polarity is negative */
  POSITIVE  /**< polarity is positive */
};

/** Output modes */
enum class OutputModes : uint8_t
{
  ANTI_FLICKER, /**< Anti-flicker mode */
  NORMAL        /**< Normal mode */
};

// LoadStates lives in load_map.h, so that the load-map logic stays Arduino-free.

/** @brief container for datalogging
 *  @details This class is used for datalogging.
 *
 * @tparam N # of phases
 * @tparam S # of temperature sensors
 */
template< uint8_t N = 3, uint8_t S = 0 > class PayloadTx_struct
{
public:
  int16_t power{ 0 };            /**< main power, import = +ve, to match OEM convention */
  int16_t power_L[N]{};          /**< power for phase #, import = +ve, to match OEM convention */
  uint16_t Vrms_L_x100[N]{};     /**< average voltage over datalogging period (in 100th of Volt)*/
  int16_t temperature_x100[S]{}; /**< temperature in 100th of °C */
};

/**
 * @brief Helper function to retrieve the dimension of a C-array
 *
 * @tparam _Tp elements type
 * @tparam _Nm dimension
 */
template< typename _Tp, size_t _Nm > constexpr size_t size(const _Tp (& /*__array*/)[_Nm]) noexcept
{
  return _Nm;
}

/**
 * @brief Helper function for the special case of a 0-dimension C-array
 *
 * @tparam _Tp elements type
 */
template< typename _Tp > constexpr size_t size(const _Tp (& /*__array*/)[0]) noexcept
{
  return 0;
}

template< class... Ts >
constexpr uint8_t ival(Ts... Vs)
{
  char vals[sizeof...(Vs)]{ Vs... };
  uint8_t result{ 0 };
  for (uint8_t i = 0; i < sizeof...(Vs); i++)
  {
    result *= 10;
    result += vals[i] - '0';
  }
  return result;
}

template< char... Vs >
constexpr integral_constant< uint8_t, ival(Vs...) > operator""_i()
{
  return {};
}

// MINUTES(): in type_traits.hpp, shared with the scheduled boosts

#endif  // TYPES_H
