/**
 * @file config.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Configuration values to be set by the end-user
 * @version 0.5
 * @date 2023-02-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef _CONFIG_H
#define _CONFIG_H

//--------------------------------------------------------------------------------------------------
//#define TEMP_ENABLED  ///< this line must be commented out if the temperature sensor is not present
//#define RF_PRESENT  ///< this line must be commented out if the RFM12B module is not present

// Output messages
//#define EMONESP  ///< Uncomment if an ESP WiFi module is used

#define ENABLE_DEBUG  ///< enable this line to include debugging print statements
#define SERIALPRINT   ///< include 'human-friendly' print statement for commissioning - comment this line to exclude.
//#define SERIALOUT ///< Uncomment if a wired serial connection is used
//--------------------------------------------------------------------------------------------------

#include "debug.h"
#include "types.h"

//--------------------------------------------------------------------------------------------------
// constants which must be set individually for each system
//
inline constexpr uint8_t NO_OF_PHASES{ 3 };    /**< number of phases of the main supply. */
inline constexpr uint8_t NO_OF_DUMPLOADS{ 3 }; /**< number of dump loads connected to the diverter */

#ifdef EMONESP
inline constexpr bool EMONESP_CONTROL{ true };
inline constexpr bool DIVERSION_PIN_PRESENT{ true }; /**< managed through EmonESP */
inline constexpr bool PRIORITY_ROTATION{ true };     /**< managed through EmonESP */
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };  /**< managed through EmonESP */
#else
inline constexpr bool EMONESP_CONTROL{ false };
inline constexpr bool DIVERSION_PIN_PRESENT{ false }; /**< set it to 'true' if you want to control diversion ON/OFF */
inline constexpr bool PRIORITY_ROTATION{ false };     /**< set it to 'true' if you want automatic rotation of priorities */
inline constexpr bool OVERRIDE_PIN_PRESENT{ false };  /**< set it to 'true' if there's a override pin */
#endif

inline constexpr bool WATCHDOG_PIN_PRESENT{ false }; /**< set it to 'true' if there's a watch led */
inline constexpr bool DUAL_TARIFF{ false };          /**< set it to 'true' if there's a dual tariff each day AND the router is connected to the billing meter */

// ----------- Pinout assignments -----------
//
// ANALOG pins are all reserved and hard-wired on the PCB
//
// DIGITAL pins:
// D0 & D1 are reserved for the Serial i/f
// - RFM12B -------------------------------
// D2 is for the RFM12B if present
// D10 is for the RFM12B if present
// D11 is for the RFM12B if present
// D12 is for the RFM12B if present
// D13 is for the RFM12B if present
// - SPI ----------------------------------
// D10 is SC
// D11 is MOSI
// D12 is MISO
// D13 is SCK

inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS]{ 5, 6, 7 }; /**< for 3-phase PCB, Load #1/#2/#3 (Rev 2 PCB) */
inline uint8_t loadPrioritiesAndState[NO_OF_DUMPLOADS]{ 0, 1, 2 };    /**< load priorities and states at startup */

inline constexpr uint8_t dualTariffPin{ 0xff }; /**< for 3-phase PCB, off-peak trigger */
inline constexpr uint8_t diversionPin{ 0xff };  /**< if LOW, set diversion on standby */
inline constexpr uint8_t rotationPin{ 0xff };   /**< if LOW, trigger a load priority rotation */
inline constexpr uint8_t forcePin{ 0xff };      /**< for 3-phase PCB, force pin */
inline constexpr uint8_t watchDogPin{ 0xff };   /**< watch dog LED */

inline constexpr uint8_t tempSensorPin{ 0xff }; /**< for 3-phase PCB, sensor pin */

inline constexpr uint8_t ul_OFF_PEAK_DURATION{ 8 };                          /**< Duration of the off-peak period in hours */
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { -3, 2 },     /**< force config for load #1 ONLY for dual tariff */
                                                              { -3, 120 },   /**< force config for load #2 ONLY for dual tariff */
                                                              { -180, 2 } }; /**< force config for load #3 ONLY for dual tariff */

inline constexpr int16_t iTemperatureThreshold{ 100 }; /**< the temperature threshold to stop overriding in °C */

inline constexpr DeviceAddress sensorAddrs[]{ { 0x28, 0xBE, 0x41, 0x6B, 0x09, 0x00, 0x00, 0xA4 },
                                              { 0x28, 0xED, 0x5B, 0x6A, 0x09, 0x00, 0x00, 0x9D },
                                              { 0x28, 0xDB, 0x6D, 0x6A, 0x09, 0x00, 0x00, 0xDA },
                                              { 0x28, 0x59, 0x1F, 0x6A, 0x09, 0x00, 0x00, 0xB0 },
                                              { 0x28, 0x1B, 0xD7, 0x6A, 0x09, 0x00, 0x00, 0xB7 } }; /**< list of temperature sensor Addresses */

//--------------------------------------------------------------------------------------------------
// for users with zero-export profile, this value will be negative
inline constexpr int16_t REQUIRED_EXPORT_IN_WATTS{ 5 }; /**< when set to a negative value, this acts as a PV generator */

//--------------------------------------------------------------------------------------------------
// other system constants, should match most of installations
inline constexpr uint8_t SUPPLY_FREQUENCY{ 50 }; /**< number of cycles/s of the grid power supply */

inline constexpr uint8_t DATALOG_PERIOD_IN_SECONDS{ 5 };                                                  /**< Period of datalogging in seconds */
inline constexpr uint16_t DATALOG_PERIOD_IN_MAINS_CYCLES{ DATALOG_PERIOD_IN_SECONDS * SUPPLY_FREQUENCY }; /**< Period of datalogging in cycles */
//--------------------------------------------------------------------------------------------------

inline constexpr uint32_t ROTATION_AFTER_CYCLES{ 8UL * 3600UL * SUPPLY_FREQUENCY }; /**< rotates load priorities after this period of inactivity */

/* --------------------------------------
   RF configuration (for the RFM12B module)
   frequency options are RF12_433MHZ, RF12_868MHZ or RF12_915MHZ
*/
#ifdef RF_PRESENT

#define RF69_COMPAT 0  // for the RFM12B
// #define RF69_COMPAT 1 // for the RF69

#define FREQ RF12_868MHZ

inline constexpr int nodeID{ 10 };        /**<  RFM12B node ID */
inline constexpr int networkGroup{ 210 }; /**< wireless network group - needs to be same for all nodes */
inline constexpr int UNO{ 1 };            /**< for when the processor contains the UNO bootloader. */

#endif  // RF_PRESENT

#endif  // _CONFIG_H
