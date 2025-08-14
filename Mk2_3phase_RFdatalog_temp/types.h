/**
 * @file types.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some basics classes/types
 * @version 0.1
 * @date 2023-02-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

#include "type_traits.hpp"

#include "constants.h"

// -------------------------------
// definitions of enumerated types

//--------------------------------------------------------------------------------------------------
/** Enum for serial output types */
enum class SerialOutputType
{
  HumanReadable, /**< Human-readable output for commissioning */
  IoT,           /**< Output for HomeAssistant or similar */
  JSON           /**< Output in JSON format */
};

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

/** Load state (for use if loads are active high (Rev 2 PCB)) */
enum class LoadStates : uint8_t
{
  LOAD_OFF, /**< load is OFF */
  LOAD_ON   /**< load is ON */
};
// enum loadStates {LOAD_ON, LOAD_OFF}; /**< for use if loads are active low (original PCB) */

inline constexpr uint8_t loadStateMask{ 0x7FU };                      /**< bit mask for masking load state */
inline constexpr uint8_t loadStateOnBit{ (uint8_t)(~loadStateMask) }; /**< bit mask for load state ON */

/** Rotation modes */
enum class RotationModes : uint8_t
{
  OFF,  /**< Off */
  AUTO, /**< Once a day */
  PIN   /**< Pin triggered */
};

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
template< typename _Tp, size_t _Nm > constexpr size_t size(const _Tp (&/*__array*/)[_Nm]) noexcept
{
  return _Nm;
}

/**
 * @brief Helper function for the special case of a 0-dimension C-array
 * 
 * @tparam _Tp elements type
 */
template< typename _Tp > constexpr size_t size(const _Tp (&/*__array*/)[0]) noexcept
{
  return 0;
}

template< class... Ts >
constexpr uint8_t ival(Ts... Vs)
{
  char vals[sizeof...(Vs)] = { Vs... };
  uint8_t result = 0;
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

#endif  // TYPES_H
