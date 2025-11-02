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
 *          - Compact bitmask format (1 bit per remote load per unit)
 *          
 *          Only compiled when ENABLE_REMOTE_LOADS is defined.
 */

#ifndef REMOTE_LOADS_H
#define REMOTE_LOADS_H

#include <Arduino.h>

#include "config_system.h"
#include "types.h"
#include "type_traits.hpp"
#include "config.h"

// Configuration for remote loads
inline constexpr uint8_t REMOTE_REFRESH_CYCLES{ 5 }; /**< send refresh every N mains cycles */

#include "utils_rf.h"  // For shared RF configuration

/**
 * @brief State tracking for a single remote unit
 */
struct RemoteUnitState
{
  uint8_t tx_data{ 0 };                       /**< Transmission payload: bitmask of remote load states (bit 0 = load 0, etc.) */
  uint8_t cyclesSinceLastUpdate{ 0 };         /**< Counter for refresh messages */
  uint8_t previousBitmask{ 0 };               /**< Previous state for change detection */
  volatile bool pendingTransmission{ false }; /**< Flag: RF transmission needed (set in ISR, cleared in main loop) */
};

/**
 * @brief Configuration for a single remote unit
 */
struct RemoteUnit
{
  uint8_t nodeId; /**< RF node ID for this unit */
};

/**
 * @class RemoteLoadManager
 * @brief Manages RF communication for multiple remote load units
 *
 * @tparam N The number of remote units (receivers) to manage
 *
 * @details
 * - Each unit has its own RF node ID and independent bitmask
 * - Supports up to 8 loads per unit
 * - Template parameter allows compile-time optimization for actual number of units
 */
template< uint8_t N >
class RemoteLoadManager
{
private:
  // Helper to extract node IDs using C++17 fold expressions
  template< size_t... Is >
  constexpr RemoteLoadManager(const RemoteUnit (&units)[N], index_sequence< Is... >)
    : nodeIds{ units[Is].nodeId... }
  {
  }

public:
  /**
   * @brief Construct a new Remote Load Manager
   * @param units Array of remote unit configurations
   */
  constexpr RemoteLoadManager(const RemoteUnit (&units)[N])
    : RemoteLoadManager(units, make_index_sequence< N >{})
  {
  }

  /**
   * @brief Update remote load states and mark units for transmission
   * @details Called once per mains cycle from ISR
   * @param loadStates Array of load states (indexed by remote load, not unit)
   */
  void updateLoads(const LoadStates* loadStates)
  {
    if constexpr (!REMOTE_LOADS_PRESENT)
    {
      (void)loadStates;
      return;
    }

    // Build bitmasks per unit by scanning physicalLoadPin array
    uint8_t unitBitmasks[N]{ 0 };
    uint8_t loadCounts[N]{ 0 };

    uint8_t remoteIdx = 0;
    uint8_t idx = NO_OF_DUMPLOADS;
    do
    {
      --idx;
      const uint8_t loadType = (physicalLoadPin[idx] & loadTypeMask) >> loadTypeShift;
      if (loadType == 0 || loadType > N)
        continue;

      const uint8_t unitIdx = loadType - 1;
      if (loadStates[remoteIdx++] == LoadStates::LOAD_ON)
        bit_set(unitBitmasks[unitIdx], loadCounts[unitIdx]);
      loadCounts[unitIdx]++;
    } while (idx);

    // Update each unit and check for changes
    uint8_t i = N;
    do
    {
      --i;
      auto& unit = unitStates[i];
      unit.tx_data = unitBitmasks[i];

      if (unit.tx_data != unit.previousBitmask)
      {
        unit.pendingTransmission = true;
        unit.previousBitmask = unit.tx_data;
        unit.cyclesSinceLastUpdate = 0;
      }
      else if (++unit.cyclesSinceLastUpdate >= REMOTE_REFRESH_CYCLES)
      {
        unit.pendingTransmission = true;
        unit.cyclesSinceLastUpdate = 0;
      }
    } while (i);
  }

  /**
   * @brief Process pending RF transmissions for all units
   * @details Call from main loop to handle RF transmissions outside ISR context
   */
  void processTransmissions() const
  {
    if constexpr (!REMOTE_LOADS_PRESENT)
      return;

    uint8_t i = N;
    do
    {
      --i;
      auto& unit = unitStates[i];
      if (unit.pendingTransmission)
      {
        unit.pendingTransmission = false;
        SharedRF::radio.send(nodeIds[i],
                             &unit.tx_data,
                             sizeof(unit.tx_data),
                             false);
      }
    } while (i);
  }

  /**
   * @brief Get the number of remote units
   * @return constexpr auto Number of units
   */
  constexpr auto size() const
  {
    return N;
  }

private:
  const uint8_t nodeIds[N];                    /**< Array of RF node IDs */
  static inline RemoteUnitState unitStates[N]; /**< State tracking for each unit */
};

#endif  // REMOTE_LOADS_H
