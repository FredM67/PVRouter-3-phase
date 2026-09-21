/**
 * @file config.h
 * @brief Configuration settings for Remote Load Receiver
 * @version 2.0
 * @date 2026-01-30
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 *
 * @copyright Copyright (c) 2025-2026
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "config_rf.h"

// Import RF configuration
using namespace RFConfig;

// Load Configuration
inline constexpr uint8_t NO_OF_LOADS{ 2 };              /**< Number of loads controlled by this unit */
inline constexpr uint8_t loadPins[NO_OF_LOADS]{ 4, 3 }; /**< Output pins for loads (active HIGH) */

// Status LED Configuration
inline constexpr uint8_t GREEN_LED_PIN{ 5 };       /**< Green LED for watchdog (1Hz blink) */
inline constexpr uint8_t RED_LED_PIN{ 7 };         /**< Red LED for RF link lost (fast blink) */
inline constexpr bool STATUS_LEDS_PRESENT{ true }; /**< Enable status LED support */

// Timing Configuration
inline constexpr unsigned long RF_TIMEOUT_MS{ 500 };         /**< Lost RF link after this many milliseconds */
inline constexpr unsigned long WATCHDOG_INTERVAL_MS{ 1000 }; /**< Toggle watchdog this often */

// Red LED: OFF when RF OK, fast blink (~4Hz) when RF lost
inline constexpr unsigned long RED_LED_INTERVAL_MS{ 125 };

// Pin configuration for RFM69 module
inline constexpr uint8_t RF_CS_PIN{ 10 }; /**< SPI Chip Select pin */
inline constexpr uint8_t RF_IRQ_PIN{ 2 }; /**< Interrupt pin */

// Data structure for received commands (must match transmitter)
struct RemoteLoadPayload
{
  uint8_t loadBitmask; /**< Bit 0 = Load 0, Bit 1 = Load 1, etc. */
};

// RF Status enumeration
enum class RfStatus : uint8_t
{
  OK,  /**< RF link is active */
  LOST /**< RF link has been lost */
};

#endif  // CONFIG_H
