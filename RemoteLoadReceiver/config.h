/**
 * @file config.h
 * @brief Configuration settings for Remote Load Receiver
 * @version 2.0
 * @date 2025-10-26
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 *
 * @copyright Copyright (c) 2025
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <RFM69.h>

// RF Configuration - must match transmitter
#define FREQUENCY RF69_868MHZ  // RF69_433MHZ, RF69_868MHZ, or RF69_915MHZ
#define IS_RFM69HW false       // true for RFM69HW/HCW, false for RFM69W/CW

inline constexpr uint8_t TX_NODE_ID{ 10 };   /**< Node ID of transmitter (SharedRF::THIS_NODE_ID) */
inline constexpr uint8_t MY_NODE_ID{ 15 };   /**< This receiver's node ID (SharedRF::REMOTE_LOAD_ID) */
inline constexpr uint8_t NETWORK_ID{ 210 };  /**< Network ID (1-255, must match transmitter) */

// Load Configuration
inline constexpr uint8_t NO_OF_LOADS{ 2 };                   /**< Number of loads controlled by this unit */
inline constexpr uint8_t loadPins[NO_OF_LOADS]{ 4, 3 };      /**< Output pins for loads (active HIGH) */

// Status LED Configuration
inline constexpr uint8_t GREEN_LED_PIN{ 5 };             /**< Green LED for watchdog (1Hz blink) */
inline constexpr uint8_t RED_LED_PIN{ 7 };               /**< Red LED for RF link lost (fast blink) */
inline constexpr bool STATUS_LEDS_PRESENT{ true };       /**< Enable status LED support */

// Timing Configuration
inline constexpr unsigned long RF_TIMEOUT_MS{ 500 };          /**< Lost RF link after this many milliseconds */
inline constexpr unsigned long WATCHDOG_INTERVAL_MS{ 1000 };  /**< Toggle watchdog this often */

// Pin configuration for RFM69 module
inline constexpr uint8_t RF_CS_PIN{ 10 };  /**< SPI Chip Select pin */
inline constexpr uint8_t RF_IRQ_PIN{ 2 };  /**< Interrupt pin */

// Data structure for received commands (must match transmitter)
struct RemoteLoadPayload
{
  uint8_t loadBitmask;  /**< Bit 0 = Load 0, Bit 1 = Load 1, etc. */
};

// RF Status enumeration
enum RfStatus
{
  RF_OK,    /**< RF link is active */
  RF_LOST   /**< RF link has been lost */
};

#endif  // CONFIG_H
