/**
 * @file config.h
 * @brief Router configuration: loads, relays, control pins and features
 *
 * Edit it by hand, or create it with the configurator:
 * https://fredm67.github.io/Mk2PVRouter/configurateur/
 *
 * Settings only: no #include line but the one below, nothing computed. The firmware builds
 * what it needs from these settings in config_derived.h.
 *
 * @version 2.0
 * @date 2026-11-21
 */

#pragma once

#include "config_types.h"  // the types and helpers used below (Load::local(), LOAD(), HOURS()...)

// Motherboard: OLD for the former 3-phase board, NEW for the universal 3phaseDiverter board
// (rev. 6.0 and later). A wrong choice gives wrong measurements: recalibrate after a change.
inline constexpr PcbVersion PCB_VERSION{ PcbVersion::NEW };

// Serial output type - IoT, for the mk2Wifi module / Home Assistant (9600 baud, 7E1)
inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::IoT;

// Debug messages (free RAM...) at startup. Whatever this setting, the startup configuration is
// printed in every output (for IoT, at 8N1, before the 7E1 of the frames), and the status
// messages in human-readable output only.
inline constexpr bool ENABLE_DEBUG{ true };

// Calibration mode: the router measures and logs as usual, but never switches any load
// (TRIACs, relays, remote loads, overrides), as with the router OFF. With temperature sensing, it
// also lists the sensors on the bus at startup, with their addresses for this file.
// Set back to false once calibrated!
inline constexpr bool CALIBRATION_MODE{ false };

//--------------------------------------------------------------------------------------------------
// Basic Configuration
//
inline constexpr uint8_t NO_OF_DUMPLOADS{ 3 }; /**< TOTAL number of dump loads (local + remote) */

// ----------- Load map -----------
//
// One entry per dump load, in any order. Each entry says who drives the load:
//   Load::local(pin)           - driven by the router itself, on that digital pin
//   Load::remote(unit)         - driven by remote unit 'unit' (1..3), over RF
//   Load::remote(unit, ledPin) - same, plus a status LED on the router
//
// See the pinout notes further down for which pins are available.
inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS]{
  Load::local(5),
  Load::local(6),
  Load::local(7)
}; /**< the load map: one packed entry per dump load */

// Feature toggles
inline constexpr bool DIVERSION_PIN_PRESENT{ false };                   /**< set it to 'true' if you want to control diversion ON/OFF */
inline constexpr bool ROUTER_OFF_PIN_PRESENT{ false };                  /**< set it to 'true' if you want a pin that switches the router OFF */
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::OFF }; /**< set it to 'OFF/AUTO/PIN' if you want manual/automatic rotation of priorities */
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };                     /**< set it to 'true' if there's a override pin */

inline constexpr bool WATCHDOG_PIN_PRESENT{ false }; /**< set it to 'true' if there's a watch led */
inline constexpr bool RELAY_DIVERSION{ false };      /**< set it to 'true' if a relay is used for diversion */
inline constexpr bool DUAL_TARIFF{ false };          /**< set it to 'true' if there's a dual tariff each day AND the router is connected to the billing meter */
inline constexpr bool TEMP_SENSOR_PRESENT{ false };  /**< set it to 'true' if temperature sensing is needed */
inline constexpr bool RF_LOGGING_PRESENT{ false };   /**< set it to 'true' if RF data logging is needed */

// ----------- Pinout Assignments -----------
//
// ANALOG pins:
// - All analog pins are reserved and hard-wired on the PCB.
//
// DIGITAL pins:
// - D0 & D1: Reserved for the Serial interface.
//
// RFM69 module (RF data logging or remote loads):
// - D2: Interrupt (IRQ).
// - D10: Chip Select (CS).
// - D11: Master Out Slave In (MOSI).
// - D12: Master In Slave Out (MISO).
// - D13: Serial Clock (SCK).
//
// mk2Wifi module (ESP32-C6, Home Assistant):
// - D5 to D9 can be wired to the module, each through a solder jumper (open by default), as
//   inputs driven from Home Assistant: routing stop, router OFF, boost, priority rotation...
// - Only close the jumpers of pins that are free in this configuration: by default, the loads
//   use D5, D6 and D7.

// Load priority order at startup (array index = priority, 0 = highest)
// Load indices refer to physicalLoadPin[] above, local and remote loads alike.
inline constexpr uint8_t loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1, 2 }; /**< load priorities at startup (0=highest) */

// Rotation groups: the loads that take turns at the top of the priority list, when
// PRIORITY_ROTATION is not OFF. One group per appliance, e.g. the 3 elements of a water heater:
//   { { ALL_LOCAL_LOADS() } }                                     - one heater, every local load
//   { { LOAD(0), LOAD(1), LOAD(2) }, { LOAD(3), LOAD(4), LOAD(5) } } - two heaters
// Each group rotates among the priority places its loads hold; loads in no group never move.
// A group has at least 2 loads, all local or all on the same remote unit, and a load is in one
// group at most. Leave it empty, {}, without rotation.
inline constexpr RotationGroups rotationGroups{}; /**< the loads that take turns, {} without rotation */

// Set the value to 'unused_pin' when the pin is not needed (feature deactivated)
inline constexpr uint8_t dualTariffPin{ unused_pin }; /**< for 3-phase PCB, off-peak trigger */
inline constexpr uint8_t diversionPin{ unused_pin };  /**< if LOW, no surplus diversion: only forced loads/relays are ON */
inline constexpr uint8_t routerOffPin{ unused_pin };  /**< if LOW, router OFF: every load and relay is OFF, forcing included */
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
// 📖 Full guide: docs/Cloud_Pattern_Tuning_Guide.md, at the root of the repository
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

// This is an example of override pin configuration.
// You can modify the pin numbers and associated loads/relays as needed.
// Ensure that the pins used do not conflict with other functionalities in your setup.
//
// Targets, in a list or alone:
//   LOAD(n)           - load n of physicalLoadPin[], local or remote
//   LOCAL_LOAD(n)     - Same as LOAD(n), for a load known to be local
//   REMOTE_LOAD(n)    - the n-th REMOTE load: n counts remote loads only, in
//                       physicalLoadPin[] order
//   RELAY(n)          - relay n of relays
//   ALL_LOCAL_LOADS(), ALL_REMOTE_LOADS(), ALL_LOADS(), ALL_RELAYS(), ALL_LOADS_AND_RELAYS()
//   a number          - that pin
//
// Example configurations:
// inline constexpr OverridePins overridePins{
//   { { 4, ALL_LOADS() },                                  // Control all loads (local + remote)
//     { 5, ALL_LOCAL_LOADS() },                            // Control only local loads
//     { 6, ALL_REMOTE_LOADS() },                           // Control only remote loads
//     { 7, { LOAD(0), REMOTE_LOAD(1) } },                  // Mixed: load 0 + the 2nd remote load
//     { 8, { LOAD(0), LOAD(1), LOAD(2), LOAD(3) } },       // Using LOAD() for any load index
//     { 9, ALL_LOADS_AND_RELAYS() } } };                   // All loads and relays

inline constexpr OverridePins overridePins{ { { 4, ALL_LOADS() } } }; /**< list of override pin/loads-relays pairs */

inline constexpr uint8_t ul_OFF_PEAK_DURATION{ 8 }; /**< Duration of the off-peak period in hours: must match the meter (it places the boosts counted back from the end) */

// Scheduled boosts during the off-peak period (DUAL_TARIFF): when, and what they force.
// Targets as for the override pins; what is not listed is never boosted.
// Each window lies within the period; every boost stops when the meter switches back to peak.
//   FIRST(d)                the first d of the period
//   LAST(d)                 the last d of the period
//   FROM_START(t), FOR(d)   from t after the start, for d (without FOR: until the end)
//   BEFORE_END(t), FOR(d)   from t before the end, for d (without FOR: until the end)
//   t and d: HOURS(n) or MINUTES(n)
// inline constexpr ScheduledBoosts scheduledBoosts{ {
//     { FIRST(HOURS(2)), { LOAD(0), LOAD(2) } },    // loads #1 and #3, the first 2 hours
//     { LAST(MINUTES(90)), { RELAY(0) } } } };      // relay #1, the last 1 h 30
inline constexpr ScheduledBoosts scheduledBoosts{ { { BEFORE_END(HOURS(3)), FOR(HOURS(2)), { LOAD(0) } } } }; /**< load #1, from 3 hours before the end, for 2 hours */

inline constexpr int16_t iTemperatureThreshold{ 100 }; /**< the temperature threshold to stop overriding in °C */
inline constexpr uint8_t BOOST_TEMP_SENSOR{ 0 };       /**< the sensor checked against it: its index in temperatureSensing, from 0 */

inline constexpr TemperatureSensing temperatureSensing{ unused_pin,
                                                        { { 0x28, 0xBE, 0x41, 0x6B, 0x09, 0x00, 0x00, 0xA4 },
                                                          { 0x28, 0x1B, 0xD7, 0x6A, 0x09, 0x00, 0x00, 0xB7 } } }; /**< list of temperature sensor Addresses */

inline constexpr uint32_t ROTATION_AFTER_SECONDS{ 8UL * 3600UL }; /**< rotates load priorities after this period of inactivity */
