/**
 * @file config.h - Basic Three-Phase Configuration Example
 * @brief Standard three-phase PVRouter setup with 2 dump loads
 *
 * This configuration is suitable for:
 * - Standard three-phase electrical installation
 * - Two resistive dump loads (e.g., water heater elements)
 * - Basic monitoring without additional sensors
 * - Human-readable serial output for commissioning
 *
 * Hardware requirements:
 * - 3-phase PVRouter PCB
 * - 3 current transformers (one per phase)
 * - 3 voltage sensing circuits (one per phase)
 * - 2 TRIAC outputs for dump loads
 *
 * @version 1.0
 * @date 2026-02-02
 */

#ifndef CONFIG_H
#define CONFIG_H

//--------------------------------------------------------------------------------------------------
#define ENABLE_DEBUG /**< enable this line to include debugging print statements */
//--------------------------------------------------------------------------------------------------

#include "config_system.h"
#include "debug.h"
#include "types.h"

// Serial output type - Human readable for initial setup and commissioning
inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::HumanReadable;

//--------------------------------------------------------------------------------------------------
// Basic Configuration
//
inline constexpr uint8_t NO_OF_DUMPLOADS{ 3 }; /**< TOTAL number of dump loads (local + remote) */

inline constexpr uint8_t NO_OF_REMOTE_LOADS{ 2 }; /**< number of remote loads controlled via RF (0 = disabled) */

// Feature toggles - Basic setup without advanced features
inline constexpr bool EMONESP_CONTROL{ false };
inline constexpr bool DIVERSION_PIN_PRESENT{ false };                   /**< set it to 'true' if you want to control diversion ON/OFF */
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::OFF }; /**< set it to 'OFF/AUTO/PIN' if you want manual/automatic rotation of priorities */
inline constexpr bool OVERRIDE_PIN_PRESENT{ false };                    /**< set it to 'true' if there's a override pin */

inline constexpr bool WATCHDOG_PIN_PRESENT{ false }; /**< set it to 'true' if there's a watch led */
inline constexpr bool RELAY_DIVERSION{ false };      /**< set it to 'true' if a relay is used for diversion */
inline constexpr bool DUAL_TARIFF{ false };          /**< set it to 'true' if there's a dual tariff each day AND the router is connected to the billing meter */
inline constexpr bool TEMP_SENSOR_PRESENT{ false };  /**< set it to 'true' if temperature sensing is needed */
inline constexpr bool RF_LOGGING_PRESENT{ false };   /**< set it to 'true' if RF data logging is needed */

inline constexpr bool REMOTE_LOADS_PRESENT{ NO_OF_REMOTE_LOADS != 0 }; /**< automatically true if remote loads configured */

#include "utils_dualtariff.h"
#include "utils_relay.h"
#include "remote_loads.h"
#include "utils_temp.h"

// ----------- Pinout Assignments -----------
//
// ANALOG pins:
// - All analog pins are reserved and hard-wired on the PCB.
//
// DIGITAL pins:
// - D0 & D1: Reserved for the Serial interface.
//
// RFM12B Module (if present):
// - D2: Used for the RFM12B module.
// - D10: Used for the RFM12B module.
// - D11: Used for the RFM12B module.
// - D12: Used for the RFM12B module.
// - D13: Used for the RFM12B module.
//
// SPI Interface:
// - D10: Chip Select (CS).
// - D11: Master Out Slave In (MOSI).
// - D12: Master In Slave Out (MISO).
// - D13: Serial Clock (SCK).
//
// Expansion Board:
// Digital Input Pins (D10-D13) are wired to the expansion board and it's intended
// to configure them as digital inputs
// They allow external control from Home Assistant for functions such as:
//   * Forced operation mode
//   * Diversion enable/disable
//   * Priority rotation triggering
//   * Manual load control
//
// D3 is wired to the expansion board too and is intended for taking control of the temperature sensor.
//
// Note: When using these pins for Home Assistant integration, ensure the ESP32
// counterpart is properly configured to send the appropriate signals.

// Physical pin assignments for LOCAL loads only (remote loads are controlled via RF)
inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS]{ 5 }; /**< Pins for local TRIAC outputs */

// Optional status LED pins for REMOTE loads (set to unused_pin if not needed)
// Note: Array size must match NO_OF_REMOTE_LOADS
inline constexpr uint8_t remoteLoadStatusLED[NO_OF_REMOTE_LOADS > 0 ? NO_OF_REMOTE_LOADS : 1]{ unused_pin, unused_pin }; /**< Optional LEDs to show remote load status */

// Load priority order at startup (array index = priority, 0 = highest)
// Load indices: 0 to (NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS - 1) are local loads,
//               (NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS) to (NO_OF_DUMPLOADS - 1) are remote loads
inline constexpr uint8_t loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1, 2 }; /**< load priorities at startup (0=highest) */

// Set the value to 'unused_pin' when the pin is not needed (feature deactivated)
inline constexpr uint8_t dualTariffPin{ unused_pin }; /**< for 3-phase PCB, off-peak trigger */
inline constexpr uint8_t diversionPin{ unused_pin };  /**< if LOW, set diversion on standby */
inline constexpr uint8_t rotationPin{ unused_pin };   /**< if LOW, trigger a load priority rotation */
inline constexpr uint8_t watchDogPin{ unused_pin };   /**< watch dog LED */

//--------------------------------------------------------------------------------------------------
// EWMA Filter Tuning for Cloud Immunity
//
// The RELAY_FILTER_DELAY parameter controls how aggressively the EWMA filter smooths
// power measurements before making relay decisions. This directly affects cloud immunity vs responsiveness.
//
// 🌤️ Quick Reference by Climate:
// - Clear sky regions (desert, dry):     1 minute  (fast response, minimal clouds)
// - Mixed conditions (most locations):   2 minutes (recommended default)
// - Frequently cloudy (coastal):         3 minutes (enhanced stability)
// - Very cloudy (mountain, tropical):    4 minutes (maximum stability)
//
// 🧪 Scientific Tuning:
// Run cloud pattern analysis tests to determine optimal setting for your climate:
//   cd Mk2_3phase_RFdatalog_temp && pio test -e native --filter="test_cloud_patterns" -v
//
// 📖 Full guide: docs/Cloud_Pattern_Tuning_Guide.md
//--------------------------------------------------------------------------------------------------
inline constexpr uint8_t RELAY_FILTER_DELAY{ 2 }; /**< EWMA filter delay in minutes for relay control */

// Relay configuration with tunable EWMA filter
// For battery systems, use negative import threshold (turn OFF when surplus < abs(threshold))
// Examples:
//   Normal installation:  { pin, 1000, 200, 5, 5 }   // Turn OFF when importing > 200W
//   Battery installation: { pin, 1000, -50, 5, 5 }   // Turn OFF when surplus < 50W
inline constexpr RelayEngine relays{ MINUTES(RELAY_FILTER_DELAY),
                                     { { unused_pin, 100, 200, 1, 1 },
                                       { unused_pin, 300, 400, 1, 1 },
                                       { unused_pin, 500, 600, 1, 1 } } }; /**< config for relay diversion with optimized EWMA filtering */

#include "utils_override_helpers.h"  // Provides LOAD(), RELAY(), ALL_LOADS(), ALL_RELAYS(), ALL_LOADS_AND_RELAYS()

// This is an example of override pin configuration.
// You can modify the pin numbers and associated loads/relays as needed.
// Ensure that the pins used do not conflict with other functionalities in your setup.
//
// Helper functions available:
//   LOCAL_LOAD(n)     - Returns physical pin for local load n
//   REMOTE_LOAD(n)    - Returns virtual pin for remote load n (>= 128)
//   LOAD(n)           - Returns pin for any load (physical for local, virtual for remote)
//   ALL_LOCAL_LOADS() - uint32_t bitmask, lower 16 bits for local load pins
//   ALL_REMOTE_LOADS()- uint32_t bitmask, upper 16 bits for remote loads (bit 16 = remote 0)
//   ALL_LOADS()       - uint32_t combining local (lower 16 bits) and remote (upper 16 bits)
//   RELAY(n)          - Returns pin for relay n
//   ALL_RELAYS()      - uint32_t bitmask, lower 16 bits for relay pins
//   ALL_LOADS_AND_RELAYS() - uint32_t combining all loads and relays
//
// Example configurations:
// inline constexpr OverridePins overridePins{
//   { { 4, ALL_LOADS() },                                  // Control all loads (local + remote)
//     { 5, ALL_LOCAL_LOADS() },                            // Control only local loads
//     { 6, ALL_REMOTE_LOADS() },                           // Control only remote loads
//     { 7, { LOCAL_LOAD(0), REMOTE_LOAD(1) } },            // Mixed: local load 0 + remote load 1
//     { 8, { LOAD(0), LOAD(1), LOAD(2), LOAD(3) } },       // Using LOAD() for any load index
//     { 9, ALL_LOADS_AND_RELAYS() } } };                   // All loads and relays

inline constexpr OverridePins overridePins{ { { 3, ALL_LOADS() },
                                              { 4, ALL_REMOTE_LOADS() } } }; /**< list of override pin/loads-relays pairs */

inline constexpr uint8_t ul_OFF_PEAK_DURATION{ 8 };             /**< Duration of the off-peak period in hours */
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{}; /**< force config for each load for dual tariff */

inline constexpr int16_t iTemperatureThreshold{ 100 }; /**< the temperature threshold to stop overriding in °C */

inline constexpr TemperatureSensing temperatureSensing{ unused_pin,
                                                        { { 0x28, 0xBE, 0x41, 0x6B, 0x09, 0x00, 0x00, 0xA4 },
                                                          { 0x28, 0x1B, 0xD7, 0x6A, 0x09, 0x00, 0x00, 0xB7 } } }; /**< list of temperature sensor Addresses */

inline constexpr uint32_t ROTATION_AFTER_SECONDS{ 8UL * 3600UL }; /**< rotates load priorities after this period of inactivity */

#endif  // CONFIG_H
