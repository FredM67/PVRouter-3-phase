/**
 * @file remote_loads.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Support for remotely controlled loads via RF using RFM69
 * @version 3.0
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2025-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details Radio shell around RemoteLoadCore (remote_loads_core.h). The core decides
 *          what each remote unit should be told and when; this file does the telling,
 *          using the shared RFM69 instance from utils_rf.h.
 *
 *          Up to MAX_REMOTE_UNITS separate units are supported, each with its own node
 *          ID (RFConfig::REMOTE_NODE_ID[]) and its own one-byte payload. A unit is
 *          addressed when at least one entry of the load map names it.
 *
 *          Transmission policy, per unit:
 *          - state changes are sent on the next pass through loop()
 *          - an unchanged payload is refreshed every REMOTE_REFRESH_CYCLES mains cycles
 *          - compact bitmask format, 1 bit per load of that unit
 *
 *          Active when the load map holds at least one remote load; otherwise the whole
 *          RF path, radio instance included, is dropped by the linker.
 */

#ifndef REMOTE_LOADS_H
#define REMOTE_LOADS_H

#include <Arduino.h>

#include "config_system.h"
#include "types.h"
#include "config_derived.h"

#include "remote_loads_core.h"
#include "utils_rf.h"  // For shared RF configuration

/**
 * @brief Process pending RF transmissions
 *
 * @details Call this from the main loop to handle RF transmissions outside ISR context.
 *          This prevents blocking the ISR with RF communication delays.
 *
 *          Nothing is sent while the RF module is not initialized (missing, unpowered or
 *          badly wired at startup): the library would wait up to 1 s for a free channel on
 *          each send, then possibly forever for the "packet sent" flag, which a module that
 *          does not answer never raises. The payloads stay pending, and go out once
 *          sendResults() has brought the module up (it retries once per datalog period).
 *
 * @note Declared here but only usable once @c remoteLoads is declared (end of config.h);
 *       it is a template-free inline function, so that is resolved at the call site.
 */
inline void processRemoteLoadTransmissions()
{
  if constexpr (REMOTE_LOADS_PRESENT)
  {
    if (!SharedRF::initialized)
    {
      return;
    }

    remoteLoads.sendPending(RFConfig::REMOTE_NODE_ID, [](uint8_t nodeId, uint8_t payload)
                            {
                              // fire and forget - no ACK, so the main loop is never blocked waiting on a reply
                              SharedRF::radio().send(nodeId, &payload, sizeof(payload), false);
                            });
  }
}

#endif  // REMOTE_LOADS_H
