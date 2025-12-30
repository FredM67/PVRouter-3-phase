/**
 * @file config_rf.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief RF module configuration for RFM69
 * @version 1.0
 * @date 2025-10-30
 *
 * @copyright Copyright (c) 2025
 *
 * @details Central configuration file for RF communication parameters.
 *          These settings must match between the router and remote load receiver.
 */

#ifndef CONFIG_RF_H
#define CONFIG_RF_H

#include <RFM69.h>

/**
 * @brief RF Module Configuration
 * @details RF Network Topology:
 * 
 *   Router (ID=10) -----> Remote Load Unit (ID=15)  [controls TRIACs]
 *        |      
 *        +-------------> Gateway (ID=1)            [receives telemetry data]
 * 
 *   - NETWORK_ID (210): Same for ALL devices on this RF network
 *   - ROUTER_NODE_ID (10): THIS router's unique address
 *   - REMOTE_NODE_ID (15): Address of the ONE Arduino controlling remote loads
 *   - GATEWAY_ID (1): Address of the telemetry receiver (optional, for data logging)
 * 
 *   Note: Current implementation supports ONE remote load unit.
 *   That unit can control multiple loads (resistors/TRIACs), but all from the same Arduino.
 */
namespace RFConfig
{
// Frequency band - MUST match your hardware and local regulations
// Options: RF69_433MHZ, RF69_868MHZ (Europe), RF69_915MHZ (USA)
inline constexpr uint8_t FREQUENCY{ RF69_868MHZ };

// Network configuration
inline constexpr uint8_t NETWORK_ID{ 210 };    /**< Network ID (1-255, MUST be identical on ALL devices) */
inline constexpr uint8_t ROUTER_NODE_ID{ 10 }; /**< THIS router's unique ID (1-30) */

// Destination node IDs - Addresses of REMOTE devices this router talks to
inline constexpr uint8_t GATEWAY_ID{ 1 };      /**< Gateway for telemetry data (only if RF_LOGGING_PRESENT=true) */
inline constexpr uint8_t REMOTE_NODE_ID{ 15 }; /**< Remote load unit ID (only if REMOTE_LOADS_PRESENT=true) */

// Hardware configuration
inline constexpr bool IS_RFM69HW{ false };  /**< true for RFM69HW/HCW (high power), false for RFM69W/CW */
inline constexpr uint8_t POWER_LEVEL{ 31 }; /**< TX power level: 0-31 (31 = max power) */

// Pin configuration for RFM69 module (standard SPI pins)
inline constexpr uint8_t RF_CS_PIN{ 10 }; /**< SPI Chip Select pin */
inline constexpr uint8_t RF_IRQ_PIN{ 2 }; /**< Interrupt pin (must be 2 or 3 on Arduino UNO) */
}

#endif  // CONFIG_RF_H
