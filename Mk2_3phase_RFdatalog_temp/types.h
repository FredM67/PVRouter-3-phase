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

// Unified load encoding in physicalLoadPin[] array:
// Bits 6-7: Load type (00=local, 01=remote unit 1, 10=remote unit 2, 11=remote unit 3)
// Bits 0-5: For local loads = physical pin number; for remote loads = optional status LED pin (0 = no LED)
inline constexpr uint8_t loadTypeMask{ 0xC0U }; /**< bits 6-7: load type */
inline constexpr uint8_t loadPinMask{ 0x3FU };  /**< bits 0-5: pin number or LED pin */
inline constexpr uint8_t loadTypeShift{ 6 };    /**< shift amount for load type bits */

/**
 * @brief Helper to create local load entry for physicalLoadPin array
 * @param pin Physical pin number for local TRIAC control
 */
constexpr uint8_t LOCAL_LOAD(uint8_t pin)
{
  return pin & loadPinMask;  // Type = 0 (local), bits 0-5 = pin
}

/**
 * @brief Helper to create remote load entry for physicalLoadPin array
 * @param unit Remote unit number (1-3)
 * @param ledPin Optional status LED pin (0 = no LED)
 */
constexpr uint8_t REMOTE_LOAD(uint8_t unit, uint8_t ledPin = 0)
{
  return ((unit << loadTypeShift) & loadTypeMask) | (ledPin & loadPinMask);
}

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

/**
 * @brief Macro to convert compile-time constant to integral_constant
 * 
 * This allows clean syntax for creating integral_constant from constexpr values
 * Usage: MINUTES(RELAY_FILTER_DELAY_MINUTES) instead of RELAY_FILTER_DELAY_MINUTES"_i"
 */
#define MINUTES(value) \
  integral_constant< uint8_t, (value) > {}

#endif  // TYPES_H
