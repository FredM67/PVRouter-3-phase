/**
 * @file config_rf.h
 * @brief RF module configuration for Remote Load Receiver
 * @version 1.0
 * @date 2025-10-30
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 *
 * @copyright Copyright (c) 2025
 *
 * @details Central configuration file for RF communication parameters.
 *          These settings MUST match the router configuration.
 */

#ifndef CONFIG_RF_H
#define CONFIG_RF_H

#include <RFM69.h>

/**
 * @brief RF Module Configuration
 * @details RF Network from Remote Load Unit perspective:
 * 
 *   Router (ID=10) -----> THIS Remote Load Unit (ID=15)
 * 
 *   - NETWORK_ID (210): MUST match the router's NETWORK_ID
 *   - ROUTER_NODE_ID (10): Address of the router sending commands (MUST match router's ROUTER_NODE_ID)
 *   - REMOTE_NODE_ID (15): THIS unit's unique address (MUST match router's REMOTE_NODE_ID)
 * 
 *   Example: If you have 2 remote load units:
 *     - Unit #1: REMOTE_NODE_ID = 15 (matches router's first REMOTE_NODE_ID)
 *     - Unit #2: REMOTE_NODE_ID = 16 (different ID, configured in a second router or separate config)
 */
namespace RFConfig
{
// Frequency band - MUST match router and local regulations
// Options: RF69_433MHZ, RF69_868MHZ (Europe), RF69_915MHZ (USA)
inline constexpr uint8_t FREQUENCY{ RF69_868MHZ };

// Network configuration - MUST match router
inline constexpr uint8_t NETWORK_ID{ 210 };        /**< Network ID (MUST match router's NETWORK_ID) */
inline constexpr uint8_t ROUTER_NODE_ID{ 10 };     /**< Router's ID (MUST match router's ROUTER_NODE_ID) */
inline constexpr uint8_t REMOTE_NODE_ID{ 15 };     /**< THIS unit's unique ID (MUST match router's REMOTE_NODE_ID) */

// Hardware configuration
inline constexpr bool IS_RFM69HW{ false }; /**< true for RFM69HW/HCW (high power), false for RFM69W/CW */

// Pin configuration for RFM69 module (standard SPI pins)
inline constexpr uint8_t RF_CS_PIN{ 10 }; /**< SPI Chip Select pin */
inline constexpr uint8_t RF_IRQ_PIN{ 2 }; /**< Interrupt pin (must be 2 or 3 on Arduino UNO) */
}

#endif  // CONFIG_RF_H
