/**
 * @file pin_edges.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The edges of the active-LOW input pins: pressed, released, or no change
 * @version 1.0
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2026-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details The diversion, router OFF, dual tariff and rotation pins are read once per second.
 *          Each one keeps its previous level, released at startup: the new level and the
 *          previous one give its edges.
 *
 * @note This header has no Arduino dependency, so it can be exercised by the native test
 *       suite. The pin itself is read in main.cpp (readPinEdges()).
 *
 * @ingroup GeneralProcessing
 */

#ifndef PIN_EDGES_H
#define PIN_EDGES_H

#include <stdint.h>

/**
 * @brief State of an active-LOW input pin, and how it changed since the previous reading.
 */
struct PinEdges
{
  bool active;  /**< the pin is LOW now */
  bool changed; /**< it changed since the previous reading */

  /** @brief it went LOW since the previous reading */
  constexpr bool pressed() const
  {
    return changed && active;
  }

  /** @brief it went HIGH since the previous reading */
  constexpr bool released() const
  {
    return changed && !active;
  }
};

/**
 * @brief The edges of a pin, from its previous level and its current one.
 *
 * @param wasActive the pin was LOW at the previous reading.
 * @param active the pin is LOW now.
 * @return its state and its edges.
 */
constexpr PinEdges pinEdges(const bool wasActive, const bool active)
{
  return { active, active != wasActive };
}

/**
 * @brief Gives the edges of a pin from its current level, and keeps that level for the next call.
 *
 * @tparam pin the input pin: each one keeps its own previous level, released at startup.
 * @param active the pin is LOW now.
 * @return its state and its edges.
 *
 * @details Call it for each pin every second, whether its edges are used or not: a previous
 *          level left stale would turn a press made long ago into an edge now.
 */
template< uint8_t pin >
PinEdges trackPinEdges(const bool active)
{
  static bool wasActive{ false };
  const PinEdges edges{ pinEdges(wasActive, active) };

  wasActive = active;
  return edges;
}

/**
 * @brief Tells whether the rotation pin asks for a rotation now.
 *
 * @param pin the edges of the rotation pin, read every second, forced or not.
 * @param bOverridden true while a load, relay or remote load is forced.
 * @return true on a press while nothing is forced. A press during forcing is dropped, not
 *         carried over to its end: held until then, it is no edge any more.
 */
constexpr bool isRotationRequested(const PinEdges& pin, const bool bOverridden)
{
  return pin.pressed() && !bOverridden;
}

#endif /* PIN_EDGES_H */
