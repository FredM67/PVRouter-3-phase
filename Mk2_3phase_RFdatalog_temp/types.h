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

#ifndef __TYPES_H__
#define __TYPES_H__

#include <Arduino.h>

#include "constants.h"

// -------------------------------
// definitions of enumerated types

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

inline constexpr uint8_t loadStateOnBit{ 0x80U }; /**< bit mask for load state ON */
inline constexpr uint8_t loadStateMask{ 0x7FU };  /**< bit mask for masking load state */

/** @brief container for datalogging
 *  @details This class is used for datalogging.
 *
 * @tparam N # of phases
 * @tparam S # of temperature sensors
 */
template< uint8_t N = 3, uint8_t S = 0 > class PayloadTx_struct
{
public:
  int16_t power;               /**< main power, import = +ve, to match OEM convention */
  int16_t power_L[N];          /**< power for phase #, import = +ve, to match OEM convention */
  int16_t Vrms_L_x100[N];      /**< average voltage over datalogging period (in 100th of Volt)*/
  int16_t temperature_x100[S]; /**< temperature in 100th of °C */
};

/** @brief Config parameters for overriding a load
 *  @details This class allows the user to define when and how long a load will be forced at
 *           full power during off-peak period.
 *
 *           For each load, the user defines a pair of values: pairForceLoad => { offset, duration }.
 *           The load will be started with full power at ('start_offpeak' + 'offset') for a duration of 'duration'
 *             - all values are in hours (if between -24 and 24) or in minutes.
 *             - if the offset is negative, it's calculated from the end of the off-peak period (ie -3 means 3 hours back from the end).
 *             - to leave the load at full power till the end of the off-peak period, set the duration to 'UINT16_MAX' (somehow infinite time)
 */
class pairForceLoad
{
public:
  constexpr pairForceLoad() = default;
  explicit constexpr pairForceLoad(int16_t _iStartOffset)
    : iStartOffset(_iStartOffset), uiDuration(UINT16_MAX)
  {
  }
  constexpr pairForceLoad(int16_t _iStartOffset, uint16_t _uiDuration)
    : iStartOffset(_iStartOffset), uiDuration(_uiDuration)
  {
  }

  constexpr int16_t getStartOffset() const
  {
    return iStartOffset;
  }
  constexpr uint16_t getDuration() const
  {
    return uiDuration;
  }

private:
  int16_t iStartOffset{ 0 };         /**< the start offset from the off-peak begin in hours or minutes */
  uint16_t uiDuration{ UINT16_MAX }; /**< the duration for overriding the load in hours or minutes */
};

using ScratchPad = uint8_t[9];
using DeviceAddress = uint8_t[8];

template< typename _Tp, size_t _Nm > constexpr size_t size(const _Tp (&/*__array*/)[_Nm]) noexcept
{
  return _Nm;
}

template< typename _Tp > constexpr size_t size(const _Tp (&/*__array*/)[0]) noexcept
{
  return 0;
}

#endif  // __TYPES_H__
