/**
 * @file utils_rf.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief RF module support for RFM69
 * @version 2.0
 * @date 2023-02-09
 *
 * @copyright Copyright (c) 2023
 *
 * @details Provides shared RFM69 radio instance and data logging functionality.
 *          Can be used standalone for data logging or together with remote loads.
 */

#ifndef _UTILS_RF_H
#define _UTILS_RF_H

#if defined(RF_PRESENT) && (defined(ENABLE_RF_DATALOGGING) || defined(ENABLE_REMOTE_LOADS))
#include <RFM69.h>
#include <SPI.h>

inline constexpr bool RF_CHIP_PRESENT{ true };

/**
 * @brief Initialize the shared RF module
 * @return true if initialization successful, false otherwise
 */
inline bool initialize_rf()
{
  if (SharedRF::initialized)
  {
    return true;
  }

  if (!SharedRF::radio.initialize(SharedRF::FREQUENCY,
                                  SharedRF::THIS_NODE_ID,
                                  SharedRF::NETWORK_ID))
  {
    return false;
  }

  if (SharedRF::IS_RFM69HW)
  {
    SharedRF::radio.setHighPower();
  }

  SharedRF::radio.setPowerLevel(SharedRF::POWER_LEVEL);
  SharedRF::initialized = true;

  return true;
}

#ifdef ENABLE_RF_DATALOGGING
/**
 * @brief Send RF data to gateway
 * @param data Reference to the telemetry data to send
 * @details Sends telemetry data to the gateway node for data logging.
 *          Initializes RF module if not already done.
 *          Uses fire-and-forget mode (no ACK) for better performance.
 */
#ifdef TEMP_ENABLED
inline void send_rf_data(const PayloadTx_struct< NO_OF_PHASES, temperatureSensing.size() >& data)
#else
inline void send_rf_data(const PayloadTx_struct< NO_OF_PHASES >& data)
#endif
{
  if (!SharedRF::initialized)
  {
    if (!initialize_rf())
    {
      return;  // Failed to initialize
    }
  }

  // Send data to gateway without ACK (fire and forget for performance)
  SharedRF::radio.send(SharedRF::GATEWAY_ID, (const void*)&data, sizeof(data), false);
}
#endif  // ENABLE_RF_DATALOGGING

#else
inline constexpr bool RF_CHIP_PRESENT{ false };
#endif  // RF_PRESENT && (ENABLE_RF_DATALOGGING || ENABLE_REMOTE_LOADS)

#endif  // _UTILS_RF_H
