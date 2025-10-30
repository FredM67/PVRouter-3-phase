/**
 * @file remote_loads.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Support for remotely controlled loads via RF using RFM69
 * @version 2.0
 * @date 2025-10-25
 *
 * @copyright Copyright (c) 2025
 *
 * @details This file implements RF communication for controlling remote loads.
 *          Remote loads have lower priority than local loads and are controlled
 *          via a minimal data protocol to reduce transmission overhead.
 *
 *          Uses shared RFM69 instance from utils_rf.h for efficient resource usage.
 *
 *          The system sends:
 *          - State changes immediately (within same mains cycle)
 *          - Refresh messages every 5 mains cycles if state unchanged
 *          - Compact bitmask format (1 bit per remote load)
 *          
 *          Only compiled when ENABLE_REMOTE_LOADS is defined.
 */

#ifndef REMOTE_LOADS_H
#define REMOTE_LOADS_H

#include <Arduino.h>

#include "config_system.h"
#include "types.h"
#include "config.h"

// Configuration for remote loads
// Note: NO_OF_REMOTE_LOADS is defined in config.h by the user
inline constexpr uint8_t REMOTE_REFRESH_CYCLES{ 5 }; /**< send refresh every N mains cycles */

#include "utils_rf.h"  // For shared RF configuration

/**
 * @brief Compact payload structure for remote load control
 * @details Uses a single byte bitmask to control up to 8 remote loads.
 *          Bit 0 = Remote Load 0, Bit 1 = Remote Load 1, etc.
 *          1 = Load ON, 0 = Load OFF
 */
struct RemoteLoadPayload
{
  uint8_t loadBitmask{ 0 }; /**< bitmask of remote load states (bit 0 = load 0, etc.) */
};

/**
 * @brief RF configuration for remote load communication
 * @details Uses shared RF radio instance from utils_rf.h
 */
namespace RemoteLoadRF
{
inline RemoteLoadPayload tx_remote_data;           /**< Transmission payload */
inline uint8_t cyclesSinceLastUpdate{ 0 };         /**< Counter for refresh messages */
inline uint8_t previousBitmask{ 0 };               /**< Previous state for change detection */
inline volatile bool pendingTransmission{ false }; /**< Flag: RF transmission needed (set in ISR, cleared in main loop) */
}

/**
 * @brief Array to track logical state of each remote load
 * @details Each element corresponds to a remote load.
 *          These are managed by the main load priority system.
 */
inline LoadStates remoteLoadState[NO_OF_REMOTE_LOADS];

/**
 * @brief Initialize RF module for remote load communication
 * @details Call this once during setup(), initializes RFM69 module
 * 
 * @return true if initialization successful, false otherwise
 */
inline bool initializeRemoteLoads()
{
  // Initialize all remote loads to OFF
  for (uint8_t i = 0; i < NO_OF_REMOTE_LOADS; ++i)
  {
    remoteLoadState[i] = LoadStates::LOAD_OFF;
  }

  RemoteLoadRF::tx_remote_data.loadBitmask = 0;
  RemoteLoadRF::previousBitmask = 0;
  RemoteLoadRF::cyclesSinceLastUpdate = 0;

  // RF initialization is handled by initialize_rf() in processing.cpp
  return true;
}

/**
 * @brief Transmit remote load data via RF
 * @details Uses shared RFM69 radio instance to send data to receiver.
 *          Non-blocking operation without ACK for performance.
 */
inline void sendRemoteLoadData()
{
  if constexpr( RF_LOGGING_PRESENT || REMOTE_LOADS_PRESENT)
  // Send to remote load receiver using shared radio
  SharedRF::radio.send(SharedRF::REMOTE_LOAD_ID,
                       &RemoteLoadRF::tx_remote_data,
                       sizeof(RemoteLoadRF::tx_remote_data),
                       false);  // false = don't request ACK (faster, less blocking)
}

/**
 * @brief Update remote load states and mark for transmission if necessary
 * @details Should be called once per mains cycle from the ISR.
 *          Sets a flag to transmit, actual RF send happens in main loop.
 * 
 * @note Call this function during the negative half-cycle processing,
 *       after local loads have been updated
 */
inline void updateRemoteLoads()
{
  // Build bitmask from current remote load states
  uint8_t currentBitmask = 0;
  for (uint8_t i = 0; i < NO_OF_REMOTE_LOADS; ++i)
  {
    if (remoteLoadState[i] == LoadStates::LOAD_ON)
    {
      currentBitmask |= (1 << i);
    }
  }

  RemoteLoadRF::tx_remote_data.loadBitmask = currentBitmask;

  // Check if state has changed
  if (currentBitmask != RemoteLoadRF::previousBitmask)
  {
    // State changed - mark for immediate transmission (handled in main loop)
    RemoteLoadRF::pendingTransmission = true;
    RemoteLoadRF::previousBitmask = currentBitmask;
    RemoteLoadRF::cyclesSinceLastUpdate = 0;
  }
  else
  {
    // State unchanged - send refresh periodically
    RemoteLoadRF::cyclesSinceLastUpdate++;
    if (RemoteLoadRF::cyclesSinceLastUpdate >= REMOTE_REFRESH_CYCLES)
    {
      RemoteLoadRF::pendingTransmission = true;
      RemoteLoadRF::cyclesSinceLastUpdate = 0;
    }
  }
}

/**
 * @brief Process pending RF transmissions
 * @details Call this from the main loop to handle RF transmissions outside ISR context.
 *          This prevents blocking the ISR with RF communication delays.
 */
inline void processRemoteLoadTransmissions()
{
  if (RemoteLoadRF::pendingTransmission)
  {
    RemoteLoadRF::pendingTransmission = false;
    sendRemoteLoadData();
  }
}

/**
 * @brief Get the number of remote loads configured
 * @return Number of remote loads
 */
inline constexpr uint8_t getRemoteLoadCount()
{
  return NO_OF_REMOTE_LOADS;
}

/**
 * @brief Check if a remote load is currently ON
 * @param loadIndex Index of the remote load (0 to NO_OF_REMOTE_LOADS-1)
 * @return true if load is ON, false otherwise
 */
inline bool isRemoteLoadOn(uint8_t loadIndex)
{
  if (loadIndex < NO_OF_REMOTE_LOADS)
  {
    return (remoteLoadState[loadIndex] == LoadStates::LOAD_ON);
  }
  return false;
}

#endif  // REMOTE_LOADS_H
