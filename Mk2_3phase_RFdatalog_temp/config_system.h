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

#ifndef CONFIG_SYSTEM_H
#define CONFIG_SYSTEM_H

#include <Arduino.h>

#include "type_traits.hpp"

inline constexpr uint8_t NO_OF_PHASES{ 3 }; /**< number of phases of the main supply. */

//--------------------------------------------------------------------------------------------------
// for users with zero-export profile, this value will be negative
inline constexpr int16_t REQUIRED_EXPORT_IN_WATTS{ 20 }; /**< when set to a negative value, this acts as a PV generator */

//--------------------------------------------------------------------------------------------------
// other system constants, should match most of installations
inline constexpr uint8_t SUPPLY_FREQUENCY{ 50 }; /**< number of cycles/s of the grid power supply */

inline constexpr uint32_t WORKING_ZONE_IN_JOULES{ 3600UL }; /**< number of joule for 1Wh */

inline constexpr uint8_t DATALOG_PERIOD_IN_SECONDS{ 5 }; /**< Period of datalogging in seconds */

inline constexpr typename conditional< DATALOG_PERIOD_IN_SECONDS * SUPPLY_FREQUENCY >= UINT8_MAX, uint16_t, uint8_t >::type DATALOG_PERIOD_IN_MAINS_CYCLES{ DATALOG_PERIOD_IN_SECONDS * SUPPLY_FREQUENCY }; /**< Period of datalogging in cycles */

// Computes inverse value at compile time to use '*' instead of '/'
inline constexpr float invSUPPLY_FREQUENCY{ 1.0F / SUPPLY_FREQUENCY };
inline constexpr float invDATALOG_PERIOD_IN_MAINS_CYCLES{ 1.0F / DATALOG_PERIOD_IN_MAINS_CYCLES };
//--------------------------------------------------------------------------------------------------

#endif  // CONFIG_SYSTEM_H
