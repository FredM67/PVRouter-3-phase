/**
 * @file remote_loads_core.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Unit grouping and payload bookkeeping for remotely controlled loads
 * @version 1.0
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2025-2026
 *
 * @details Given the load map (see load_map.h) and the current state of every dump
 *          load, this builds one RF payload per remote unit and decides when a
 *          transmission is due. It knows nothing about the radio itself: the actual
 *          send lives in remote_loads.h, outside the ISR.
 *
 *          Each unit is sent one byte, one bit per load:
 *          **bit @em b of a unit's payload is the @em b-th load of that unit in
 *          ascending physicalLoadPin[] order.** The receiver must agree.
 *
 *          A payload is transmitted when it changes, and otherwise refreshed every
 *          REMOTE_REFRESH_CYCLES mains cycles so a receiver that missed a packet -
 *          or was powered up late - converges within 100 ms.
 *
 * @note This header has no Arduino dependency (its whole include closure is
 *       @c <stdint.h>, @c utils_bits.h and @c load_map.h) so it can be exercised
 *       by the native test suite.
 */

#ifndef REMOTE_LOADS_CORE_H
#define REMOTE_LOADS_CORE_H

#include <stdint.h>

#include "utils_bits.h"
#include "load_map.h"

inline constexpr uint8_t REMOTE_REFRESH_CYCLES{ 5 }; /**< send a refresh every N mains cycles */

/**
 * @brief Check the node IDs of the first @p numUnits remote units.
 *
 * @details Each must lie in 1..30, differ from the router's own ID, and be unique.
 *
 * @param nodeIds Node ID table, unit 1 first.
 * @param numUnits Number of units actually addressed.
 * @param routerId Node ID of the router itself.
 */
template< uint8_t NumIds >
constexpr bool areValidNodeIds(const uint8_t (&nodeIds)[NumIds], uint8_t numUnits, uint8_t routerId)
{
  if (numUnits > NumIds)
  {
    return false;
  }

  for (uint8_t i = 0; i != numUnits; ++i)
  {
    if ((nodeIds[i] < 1) || (nodeIds[i] > 30) || (nodeIds[i] == routerId))
    {
      return false;
    }

    for (uint8_t j = 0; j < i; ++j)
    {
      if (nodeIds[i] == nodeIds[j])
      {
        return false;
      }
    }
  }
  return true;
}

/**
 * @brief Transmission bookkeeping for one remote unit.
 */
struct RemoteUnitState
{
  uint8_t tx_data{ 0 };                       /**< payload to be sent: bitmask of this unit's loads */
  uint8_t cyclesSinceLastUpdate{ 0 };         /**< mains cycles elapsed since the last transmission */
  uint8_t previousBitmask{ 0 };               /**< last transmitted payload, for change detection */
  volatile bool pendingTransmission{ false }; /**< a send is due; set in the ISR, cleared in loop() */
};

/**
 * @brief Builds and schedules the RF payloads of every remote unit.
 *
 * @tparam NumUnits Number of remote units, 0 to MAX_REMOTE_UNITS.
 *
 * @details The load map is passed in by reference-to-array rather than read from the
 *          @c physicalLoadPin[] global, which is what makes this class testable on the
 *          host. It costs nothing: there is a single call site, the map is a
 *          @c constexpr global, and @c NumLoads is a template argument.
 *
 *          The default constructor is @c constexpr and every member carries a
 *          brace-or-equal initialiser, so an instance at namespace scope is
 *          constant-initialized: it lands in @c .bss with no @c .init_array entry,
 *          and is dropped altogether by @c --gc-sections when unused.
 */
template< uint8_t NumUnits >
class RemoteLoadCore
{
public:
  constexpr RemoteLoadCore() = default;

  /** @brief Number of remote units handled. */
  static constexpr uint8_t size()
  {
    return NumUnits;
  }

  /**
   * @brief Rebuild every unit's payload from the current load states.
   *
   * @details Called once per mains cycle, from the ADC ISR. Local entries of the map
   *          are skipped, as is any entry naming a unit beyond @c NumUnits.
   *
   * @param loadMap The packed load map.
   * @param loadStates The current state of each load, in the same index space.
   */
  template< uint8_t NumLoads >
  void updateLoads(const uint8_t (&loadMap)[NumLoads], const LoadStates (&loadStates)[NumLoads])
  {
    if constexpr (NumUnits > 0)
    {
      uint8_t bitmask[NumUnits]{};
      uint8_t nextBit[NumUnits]{};

      for (uint8_t i = 0; i < NumLoads; ++i)
      {
        const uint8_t unit{ Load::unitOf(loadMap[i]) };

        if ((unit == 0) || (unit > NumUnits))
        {
          continue;  // local load, or a unit we do not talk to
        }

        const uint8_t idx{ static_cast< uint8_t >(unit - 1) };

        if (loadStates[i] == LoadStates::LOAD_ON)
        {
          bit_set(bitmask[idx], nextBit[idx]);
        }

        ++nextBit[idx];
      }

      for (uint8_t idx = 0; idx < NumUnits; ++idx)
      {
        schedule(idx, bitmask[idx]);
      }
    }
  }

  /**
   * @brief Claim a pending transmission for one unit.
   *
   * @details Clears the pending flag, so a second call returns false until the state
   *          changes again or the refresh period elapses. Safe to call from loop()
   *          without disabling interrupts: flag and payload are single bytes, and if
   *          the ISR updates them in between, the newer payload is sent and the flag
   *          stays set, so at worst the same byte goes out twice.
   *
   * @param unitIdx Zero-based unit index (unit number - 1).
   * @param payload Receives the bitmask to transmit.
   * @return true if a transmission is due.
   */
  bool takePending(uint8_t unitIdx, uint8_t& payload)
  {
    if ((unitIdx >= NumUnits) || !unitStates[unitIdx].pendingTransmission)
    {
      return false;
    }

    unitStates[unitIdx].pendingTransmission = false;
    payload = unitStates[unitIdx].tx_data;

    return true;
  }

  /**
   * @brief Hand every pending payload to @p send, addressed to its unit's node ID.
   *
   * @details Unit @em n (index @em n-1) goes to @c nodeIds[n-1]. The radio stays out
   *          of this class: the firmware passes a lambda around @c RFM69::send(),
   *          the native tests one that records what would have been sent.
   *
   * @param nodeIds Node ID table, unit 1 first; at least @c NumUnits entries.
   * @param send Callable as @c send(uint8_t nodeId, uint8_t payload).
   */
  template< uint8_t NumIds, typename Send >
  void sendPending(const uint8_t (&nodeIds)[NumIds], Send&& send)
  {
    static_assert(NumIds >= NumUnits, "one node ID per remote unit");

    for (uint8_t idx = 0; idx != NumUnits; ++idx)
    {
      uint8_t payload{ 0 };

      if (takePending(idx, payload))
      {
        send(nodeIds[idx], payload);
      }
    }
  }

  /** @brief Current payload of a unit, whether or not it has been sent. */
  constexpr uint8_t payloadOf(uint8_t unitIdx) const
  {
    return (unitIdx < NumUnits) ? unitStates[unitIdx].tx_data : 0;
  }

  /** @brief True if a transmission is due for that unit. */
  constexpr bool isPending(uint8_t unitIdx) const
  {
    return (unitIdx < NumUnits) && unitStates[unitIdx].pendingTransmission;
  }

  /** @brief Return every unit to its power-on state. */
  void reset()
  {
    for (uint8_t idx = 0; idx < NumUnits; ++idx)
    {
      unitStates[idx] = RemoteUnitState{};
    }
  }

private:
  /**
   * @brief Record a unit's freshly computed payload and decide whether to send it.
   */
  void schedule(uint8_t unitIdx, uint8_t bitmask)
  {
    auto& unit{ unitStates[unitIdx] };

    unit.tx_data = bitmask;

    if (bitmask != unit.previousBitmask)
    {
      // state changed - transmit on the next pass through loop()
      unit.previousBitmask = bitmask;
      unit.cyclesSinceLastUpdate = 0;
      unit.pendingTransmission = true;

      return;
    }

    // state unchanged - refresh periodically
    if (++unit.cyclesSinceLastUpdate >= REMOTE_REFRESH_CYCLES)
    {
      unit.cyclesSinceLastUpdate = 0;
      unit.pendingTransmission = true;
    }
  }

  RemoteUnitState unitStates[NumUnits ? NumUnits : 1]{};
};

#endif  // REMOTE_LOADS_CORE_H
