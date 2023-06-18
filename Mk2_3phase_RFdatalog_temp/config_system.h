/**
 * @file config_system.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Basic configuration values to be set by the end-user
 * @version 0.1
 * @date 2023-05-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef __CONFIG_SYSTEM_H__
#define __CONFIG_SYSTEM_H__

#include <Arduino.h>

//--------------------------------------------------------------------------------------------------
// for users with zero-export profile, this value will be negative
inline constexpr int16_t REQUIRED_EXPORT_IN_WATTS{ 20 }; /**< when set to a negative value, this acts as a PV generator */

//--------------------------------------------------------------------------------------------------
// other system constants, should match most of installations
inline constexpr uint8_t SUPPLY_FREQUENCY{ 50 }; /**< number of cycles/s of the grid power supply */

inline constexpr uint8_t DATALOG_PERIOD_IN_SECONDS{ 5 };                                                  /**< Period of datalogging in seconds */
inline constexpr uint16_t DATALOG_PERIOD_IN_MAINS_CYCLES{ DATALOG_PERIOD_IN_SECONDS * SUPPLY_FREQUENCY }; /**< Period of datalogging in cycles */
//--------------------------------------------------------------------------------------------------

#endif  // __CONFIG_SYSTEM_H__
