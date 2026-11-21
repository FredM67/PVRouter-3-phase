/**
 * @file override_masks.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The forced outputs: the override pins, and the scheduled boosts of the dual tariff
 * @version 1.0
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2026-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details Once per second, the firmware computes what is forced ON: the targets of each override
 *          pin held LOW, and the targets of each scheduled boost whose window is open. The
 *          functions here take every input explicitly (the pin states, the time elapsed since
 *          the start of the off-peak period, the temperature, the tables); main.cpp reads
 *          millis(), the pins and the globals, and calls them.
 *
 * @note This header has no Arduino dependency, so it can be exercised by the native test
 *       suite (test/native/test_override_masks).
 *
 * @ingroup GeneralProcessing
 */

#ifndef OVERRIDE_MASKS_H
#define OVERRIDE_MASKS_H

#include <stdint.h>

#include "utils_dualtariff.h"  // DualTariff::Boost, DualTariff::inWindow()

/**
 * @struct OverrideMasks
 * @brief Holds both local and remote override bitmasks.
 *
 * @var OverrideMasks::local
 * Bitmask for local loads and relays (physical pins 2-13).
 * @var OverrideMasks::remote
 * Bitmask for remote loads (bit n = remote load n, n being the remote ordinal).
 */
struct OverrideMasks
{
  uint16_t local;
  uint8_t remote;
};

/** @brief Both masks of @p a and @p b: what either forces. */
constexpr OverrideMasks operator|(const OverrideMasks& a, const OverrideMasks& b)
{
  return { static_cast< uint16_t >(a.local | b.local), static_cast< uint8_t >(a.remote | b.remote) };
}

namespace Forcing
{
/**
 * @brief The targets of the override pins that are active (held LOW).
 *
 * @param pins the override pins and their targets (OverridePins)
 * @param isActive callable taking a pin number, true if the pin is LOW
 * @return the union of the masks of every active pin
 */
template< typename Pins, typename IsActive >
inline OverrideMasks pinForcing(const Pins& pins, IsActive isActive)
{
  OverrideMasks masks{ 0, 0 };

  if (0 == pins.size())
  {
    return masks;
  }

  // a do-while, as before the move: a for loop costs 22 bytes more on the AVR
  uint8_t i{ pins.size() };
  do
  {
    --i;
    if (isActive(pins.getPin(i)))
    {
      masks.local |= pins.getLocalBitmask(i);
      masks.remote |= pins.getRemoteBitmask(i);
    }
  } while (i);
  return masks;
}

/**
 * @brief The targets of the scheduled boosts whose window is open.
 *
 * @details No boost outside the off-peak period, nor when the temperature is above the
 *          threshold. A missing or failed reading (DEVICE_DISCONNECTED_RAW, -7040) counts as cold
 *          on purpose: the boost runs, and the water heater's own thermostat stays the safety
 *          limit; the threshold only stops the boost before the heater's setting.
 *
 * @param bOffPeak true during the off-peak period
 * @param elapsed ms since the start of the off-peak period (millis() - its start)
 * @param temperature_x100 the temperature of the probe checked against the threshold, x100
 * @param threshold_x100 the temperature above which no boost runs, x100
 * @param count the number of boosts
 * @param readBoost callable taking a boost index (below @p count), returning its DualTariff::Boost
 * @return the union of the masks of every boost in its window
 */
template< typename ReadBoost >
inline OverrideMasks boostForcing(const bool bOffPeak, const uint32_t elapsed, const int16_t temperature_x100,
                                  const int16_t threshold_x100, const uint8_t count, ReadBoost readBoost)
{
  if (!bOffPeak)
  {
    return { 0, 0 };
  }

  if (temperature_x100 > threshold_x100)
  {
    return { 0, 0 };
  }

  OverrideMasks forcing{ 0, 0 };
  for (uint8_t i = 0; i != count; ++i)
  {
    const DualTariff::Boost boost{ readBoost(i) };

    if (DualTariff::inWindow(elapsed, boost.start, boost.end))
    {
      forcing.local |= boost.local;
      forcing.remote |= boost.remote;
    }
  }
  return forcing;
}
}  // namespace Forcing

#endif  // OVERRIDE_MASKS_H
