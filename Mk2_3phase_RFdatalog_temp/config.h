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

#ifndef CONFIG_H
#define CONFIG_H

//--------------------------------------------------------------------------------------------------
//#define TEMP_ENABLED  /**< this line must be commented out if the temperature sensor is not present */
//#define RF_PRESENT  /**< this line must be commented out if the RFM12B module is not present */#define ENABLE_DEBUG /**< enable this line to include debugging print statements */
#define ENABLE_DEBUG /**< enable this line to include debugging print statements */
//--------------------------------------------------------------------------------------------------

#include "config_system.h"
#include "debug.h"
#include "types.h"

#include "utils_dualtariff.h"
#include "utils_relay.h"
#include "utils_temp.h"

// constexpr variable to set the serial output type
inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::HumanReadable;

//--------------------------------------------------------------------------------------------------
// constants which must be set individually for each system
//
inline constexpr uint8_t NO_OF_DUMPLOADS{ 2 }; /**< number of dump loads connected to the diverter */

#ifdef EMONESP
inline constexpr bool EMONESP_CONTROL{ true };
inline constexpr bool DIVERSION_PIN_PRESENT{ true };                    /**< managed through EmonESP */
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::PIN }; /**< managed through EmonESP */
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };                     /**< managed through EmonESP */
#else
inline constexpr bool EMONESP_CONTROL{ false };
inline constexpr bool DIVERSION_PIN_PRESENT{ false };                   /**< set it to 'true' if you want to control diversion ON/OFF */
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::OFF }; /**< set it to 'OFF/AUTO/PIN' if you want manual/automatic rotation of priorities */
inline constexpr bool OVERRIDE_PIN_PRESENT{ false };                    /**< set it to 'true' if there's a override pin */
#endif

inline constexpr bool WATCHDOG_PIN_PRESENT{ false }; /**< set it to 'true' if there's a watch led */
inline constexpr bool RELAY_DIVERSION{ false };      /**< set it to 'true' if a relay is used for diversion */
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

inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS]{ 5, 7 };         /**< for 3-phase PCB, Load #1/#2/#3 (Rev 2 PCB) */
inline constexpr uint8_t loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1 }; /**< load priorities and states at startup */

// Set the value to 0xff when the pin is not needed (feature deactivated)
inline constexpr uint8_t dualTariffPin{ 0xff }; /**< for 3-phase PCB, off-peak trigger */
inline constexpr uint8_t diversionPin{ 0xff };  /**< if LOW, set diversion on standby */
inline constexpr uint8_t rotationPin{ 0xff };   /**< if LOW, trigger a load priority rotation */
inline constexpr uint8_t forcePin{ 0xff };      /**< for 3-phase PCB, force pin */
inline constexpr uint8_t watchDogPin{ 0xff };   /**< watch dog LED */

inline constexpr RelayEngine relays{ { { 0xff, 1000, 200, 1, 1 } } }; /**< config for relay diversion, see class definition for defaults and advanced options */

inline constexpr uint8_t ul_OFF_PEAK_DURATION{ 8 };                        /**< Duration of the off-peak period in hours */
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { -3, 2 } }; /**< force config for load #1 ONLY for dual tariff */

inline constexpr int16_t iTemperatureThreshold{ 100 }; /**< the temperature threshold to stop overriding in °C */

inline constexpr TemperatureSensing temperatureSensing{ 0xff,
                                                        { { 0x28, 0xBE, 0x41, 0x6B, 0x09, 0x00, 0x00, 0xA4 },
                                                          { 0x28, 0xED, 0x5B, 0x6A, 0x09, 0x00, 0x00, 0x9D },
                                                          { 0x28, 0xDB, 0x6D, 0x6A, 0x09, 0x00, 0x00, 0xDA },
                                                          { 0x28, 0x59, 0x1F, 0x6A, 0x09, 0x00, 0x00, 0xB0 },
                                                          { 0x28, 0x1B, 0xD7, 0x6A, 0x09, 0x00, 0x00, 0xB7 } } }; /**< list of temperature sensor Addresses */

inline constexpr uint32_t ROTATION_AFTER_SECONDS{ 8UL * 3600UL }; /**< rotates load priorities after this period of inactivity */

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

#endif  // CONFIG_H
