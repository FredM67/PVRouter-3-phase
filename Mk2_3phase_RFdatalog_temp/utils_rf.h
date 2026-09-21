/**
 * @file utils_rf.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief RF module support for RFM69
 * @version 2.0
 * @date 2026-09-21
 *
 * @copyright Copyright (c) 2023-2026
 *
 * @details Provides shared RFM69 radio instance and data logging functionality.
 *          Can be used standalone for data logging or together with remote loads.
 *          RF configuration is centralized in config_rf.h
 */

#ifndef _UTILS_RF_H
#define _UTILS_RF_H

#include "config.h"
#include "config_rf.h"

#include <RFM69.h>
#include <SPI.h>

inline constexpr bool RF_CHIP_PRESENT{ REMOTE_LOADS_PRESENT || RF_LOGGING_PRESENT };

// Shared RFM69 instance and state
namespace SharedRF
{
// Import RF configuration from config_rf.h
using namespace RFConfig;

/**
 * @brief Accessor for the shared RFM69 instance
 * @return Reference to the one and only RFM69 object
 *
 * @details The radio is a function-local static rather than a namespace-scope object
 *          on purpose. A namespace-scope RFM69 would have a dynamic initializer, which
 *          is emitted into `.init_array` whether or not any RF feature is enabled, and
 *          that reference alone drags the whole RFM69 driver (~2.9 kB of flash) into
 *          every build. As a function-local static it is only created if this function
 *          is actually called, so with `RF_CHIP_PRESENT == false` the linker drops it.
 */
inline RFM69& radio()
{
  static RFM69 instance{ RF_CS_PIN, RF_IRQ_PIN, IS_RFM69HW };
  return instance;
}

inline bool initialized{ false }; /**< Track initialization state */
}

/**
 * @brief Initialize the shared RF module
 * @return true if initialization successful, false otherwise
 */
inline bool initialize_rf()
{
  if constexpr (RF_CHIP_PRESENT)
  {
    if (SharedRF::initialized)
    {
      return true;
    }

    if (!SharedRF::radio().initialize(SharedRF::FREQUENCY,
                                      SharedRF::ROUTER_NODE_ID,
                                      SharedRF::NETWORK_ID))
    {
      return false;
    }

    if constexpr (SharedRF::IS_RFM69HW)
    {
      SharedRF::radio().setHighPower();
    }

    SharedRF::radio().setPowerLevel(SharedRF::POWER_LEVEL);
    SharedRF::initialized = true;

    return true;
  }

  return false;
}

/**
 * @brief Send RF data to gateway
 * @param data Reference to the telemetry data to send
 * @details Sends telemetry data to the gateway node for data logging.
 *          Initializes RF module if not already done.
 *          Uses fire-and-forget mode (no ACK) for better performance.
 */
template< typename PayloadType >
inline void send_rf_data(const PayloadType& data)
{
  if constexpr (RF_LOGGING_PRESENT)
  {
    if (!SharedRF::initialized)
    {
      if (!initialize_rf())
      {
        return;  // Failed to initialize
      }
    }

    // Send data to gateway without ACK (fire and forget for performance)
    SharedRF::radio().send(SharedRF::GATEWAY_ID, (const void*)&data, sizeof(data), false);
  }
}

#endif  // _UTILS_RF_H
