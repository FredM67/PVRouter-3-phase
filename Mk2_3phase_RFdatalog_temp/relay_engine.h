/**
 * @file relay_engine.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Decisions of the relay diversion: which relay switches, and when
 * @version 0.1
 * @date 2026-11-21
 *
 * @details The relay engine without its I/O: the thresholds, the minimum ON and OFF times, the
 *          settle time after any relay change, the order the relays come in and go out, and the
 *          EWMA average they decide on. On each change, the relay calls the switched() of its
 *          Output type, which drives the pin in the firmware (utils_relay.h). Its native tests
 *          are in test/native/test_relay_engine.
 *
 * @note This header has no Arduino dependency (its whole include closure is plain C++).
 *
 * @copyright Copyright (c) 2023-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef RELAY_ENGINE_H
#define RELAY_ENGINE_H

#include <stdint.h>

#include "config_system.h"  // DATALOG_PERIOD_IN_SECONDS
#include "type_traits.hpp"  // make_index_sequence
#include "ewma_avg.hpp"
#include "utils_bits.h"

/**
 * @brief The relay engine without its I/O.
 * @ingroup RelayDiversion
 */
namespace RelayLogic
{
/**
 * @brief The default output of a relay: none, the state only.
 */
struct NoOutput
{
  /**
   * @brief Called on each change, before the state changes
   *
   * @param pin The control pin of the relay
   * @param on The new state
   */
  static void switched(uint8_t /* pin */, bool /* on */)
  {
  }
};

/**
 * @class Relay
 * @brief The state of one relay, and its decisions.
 *
 * @details
 * - **Relay Control**: The relay is turned ON when the surplus threshold is exceeded
 *   and turned OFF when the import threshold is crossed.
 * - **Time Constraints**: Minimum ON and OFF durations ensure stable operation.
 * - **State Management**: The class tracks the relay's current state and the duration
 *   of its current state.
 *
 * On each change, Output::switched(pin, on) drives the pin (relayOutput, utils_relay.h); then
 * the state changes, and the methods return true.
 *
 * @tparam Output A type with a static switched(uint8_t pin, bool on), called on each change.
 */
template< typename Output = NoOutput >
class Relay
{
public:
  constexpr Relay() = delete;

  /**
   * @brief Construct a new relay Config object with default parameters
   *
   * @param _relay_pin Control pin for the relay
   */
  explicit constexpr Relay(const uint8_t _relay_pin)
    : relay_pin{ _relay_pin }
  {
  }

  /**
   * @brief Construct a new relay Config object with default/custom parameters
   *
   * @param _relay_pin Control pin for the relay
   * @param _surplusThreshold Surplus threshold to turn relay ON (positive value, e.g., 1000)
   * @param _importThreshold Import threshold to turn relay OFF
   *        - Positive values: turn OFF when importing > threshold (normal installations)
   *        - Negative values: turn OFF when surplus < abs(threshold) (battery systems)
   *        - Zero: turn OFF when importing > 0W (problematic with batteries)
   */
  constexpr Relay(uint8_t _relay_pin, int16_t _surplusThreshold, int16_t _importThreshold)
    : relay_pin{ _relay_pin }, surplusThreshold{ negativeOf(_surplusThreshold) }, importThreshold{ _importThreshold }
  {
  }

  /**
   * @brief Construct a new relay Config object with custom parameters
   *
   * @param _relay_pin Control pin for the relay
   * @param _surplusThreshold Surplus threshold to turn relay ON (positive value, e.g., 1000)
   * @param _importThreshold Import threshold to turn relay OFF
   *        - Positive values: turn OFF when importing > threshold (normal installations)
   *        - Negative values: turn OFF when surplus < abs(threshold) (battery systems)
   *        - Zero: turn OFF when importing > 0W (problematic with batteries)
   * @param _minON Minimum duration in minutes to leave relay ON
   * @param _minOFF Minimum duration in minutes to leave relay OFF
   */
  constexpr Relay(uint8_t _relay_pin, int16_t _surplusThreshold, int16_t _importThreshold, uint16_t _minON, uint16_t _minOFF)
    : relay_pin{ _relay_pin }, surplusThreshold{ negativeOf(_surplusThreshold) }, importThreshold{ _importThreshold }, minON{ toSeconds(_minON) }, minOFF{ toSeconds(_minOFF) }
  {
  }

  /**
   * @brief Get the control pin of the relay
   *
   * @return constexpr auto
   */
  constexpr auto get_pin() const
  {
    return relay_pin;
  }

  /**
   * @brief Get the surplus threshold which will turns ON the relay
   *
   * @return constexpr auto
   */
  constexpr auto get_surplusThreshold() const
  {
    return -surplusThreshold;
  }

  /**
   * @brief Get the import threshold which will turns OFF the relay
   *
   * @return constexpr auto
   */
  constexpr auto get_importThreshold() const
  {
    return importThreshold;
  }

  /**
   * @brief Get the minimum ON-time in seconds
   *
   * @return constexpr auto
   */
  constexpr auto get_minON() const
  {
    return minON;
  }

  /**
   * @brief Get the minimum OFF-time in seconds
   *
   * @return constexpr auto
   */
  constexpr auto get_minOFF() const
  {
    return minOFF;
  }

  /**
   * @brief Whether both minimum times fit in 16 bits of seconds (1092 minutes at most).
   *
   * @details validation.h refuses a relay for which it is false.
   */
  constexpr bool hasValidMinimumTimes() const
  {
    return (minON != UINT16_MAX) && (minOFF != UINT16_MAX);
  }

  /**
   * @brief Return the state
   *
   * @return auto
   */
  auto isRelayON() const
  {
    return relayIsON;
  }

  /**
   * @brief Increment the duration of the current state
   * @details This function must be called every second.
   *
   */
  void inc_duration() const
  {
    if (duration < UINT16_MAX)
    {
      ++duration;
    }
  }

  /**
   * @brief Proceed with the relay
   *
   * @param currentAvgPower Current average power
   * @param overrideBitmask Override bitmask (local pins): the relay is forced ON while its bit is set
   * @param onAllowed False to turn the relay OFF only (a forced relay is never turned OFF)
   * @return bool True if state has changed
   */
  bool proceed_relay(const int32_t currentAvgPower, const uint16_t overrideBitmask, const bool onAllowed = true) const
  {
    const bool forced{ isForced(overrideBitmask) };

    // To avoid changing sign, surplus is a negative value
    // Turn ON if surplus threshold is met OR if override is active
    if (onAllowed && (currentAvgPower < surplusThreshold || forced))
    {
      return try_switch(true);
    }

    // Positive threshold: turn OFF when importing > threshold (normal mode).
    // Negative threshold: turn OFF when surplus < abs(threshold) (battery mode).
    if (!forced && currentAvgPower > importThreshold)
    {
      return try_switch(false);
    }

    return false;
  }

  /**
   * @brief Proceed with the relay while diversion is disabled: ON only while forced
   *
   * @param overrideBitmask Override bitmask (local pins)
   * @return bool True if state has changed
   * @details The minimum ON and OFF times still apply.
   */
  bool proceed_forced_only(const uint16_t overrideBitmask) const
  {
    return try_switch(isForced(overrideBitmask));
  }

private:
  /**
   * @brief Minutes to seconds, in 16 bits.
   *
   * @return the seconds, or UINT16_MAX beyond 1092 minutes: no number of minutes gives it (it
   *         is no multiple of 60), so hasValidMinimumTimes() can tell. On AVR, `minutes * 60`
   *         would wrap silently (1093 minutes gave 44 seconds).
   */
  static constexpr uint16_t toSeconds(const uint16_t minutes)
  {
    return minutes * 60UL > UINT16_MAX ? UINT16_MAX : static_cast< uint16_t >(minutes * 60UL);
  }

  /** @brief -abs(value), without the abs() macro of Arduino.h. */
  static constexpr int16_t negativeOf(const int16_t value)
  {
    return value > 0 ? -value : value;
  }

  bool isForced(const uint16_t overrideBitmask) const
  {
    return bit_read(overrideBitmask, relay_pin);
  }

  /**
   * @brief Turn the relay ON or OFF if the 'time' condition is met
   *
   * @param on True to turn the relay ON, false to turn it OFF
   * @return bool True if state has changed
   */
  bool try_switch(const bool on) const
  {
    if ((relayIsON == on) || (duration < (on ? minOFF : minON)))
    {
      return false;
    }

    Output::switched(relay_pin, on);

    relayIsON = on;
    duration = 0;

    return true;
  }

private:
  const uint8_t relay_pin{ 0xff };         /**< Pin associated with the relay (unused_pin) */
  const int16_t surplusThreshold{ -1000 }; /**< Surplus threshold to turn relay ON */
  const int16_t importThreshold{ 200 };    /**< Import threshold to turn relay OFF (positive = import mode, negative = surplus mode) */
  const uint16_t minON{ 5 * 60 };          /**< Minimum duration in seconds the relay is turned ON */
  const uint16_t minOFF{ 5 * 60 };         /**< Minimum duration in seconds the relay is turned OFF */

  mutable uint16_t duration{ 0 };  /**< Duration of the current state */
  mutable bool relayIsON{ false }; /**< True if the relay is ON */
};

/**
 * @class Engine
 * @brief A collection of relays, switched one at a time from a sliding average.
 *
 * @tparam R The relay type: a Relay<>, or a type derived from one.
 * @tparam N The number of relays to be managed.
 * @tparam D The duration in minutes for the sliding average.
 *
 * @details
 * - **Relay Management**: Handles the state of multiple relays, turning them ON or OFF based
 *   on surplus and import thresholds.
 * - **Sliding Average**: Uses an Exponentially Weighted Moving Average (EWMA) to calculate
 *   the average power over a configurable duration.
 * - **State Transitions**: Ensures stable operation by enforcing minimum ON/OFF durations
 *   and a delay between state changes.
 */
template< typename R, uint8_t N, uint8_t D >
class Engine
{
public:
  /**
   * @brief Construct a list of relays
   *
   * @param ref Array of relay configurations.
   */
  explicit constexpr Engine(const R (&ref)[N])
    : Engine(ref, make_index_sequence< N >{})
  {
  }

  /**
   * @brief Get the number of relays
   *
   * @return constexpr auto The number of relays
   */
  constexpr auto size() const
  {
    return N;
  }

  /**
   * @brief Get the relay object
   *
   * @param idx The index of the relay
   * @return constexpr const auto& The relay object
   */
  constexpr const auto& get_relay(uint8_t idx) const
  {
    return relay[idx];
  }

  /**
   * @brief Get the current average
   *
   * @return auto The current average
   */
  inline static auto get_average()
  {
    return ewma_average.getAverageT();  // Use TEMA for better cloud immunity
  }

  /**
   * @brief Update the sliding average
   *
   * @param currentPower Current power at the grid
   */
  inline static void update_average(int16_t currentPower)
  {
    ewma_average.addValue(currentPower);
  }

  /**
   * @brief Increment the duration's state of each relay.
   *
   * @details This method updates the duration of the current state for each relay and decreases
   * the delay (`settle_change`) until the next state change is allowed.
   */
#if defined(__DOXYGEN__)
  void inc_duration() const;
#else
  void inc_duration() const __attribute__((optimize("-O3")));
#endif

  /**
   * @brief Proceed all relays: OFF in decreasing order, ON in increasing order.
   *
   * @param overrideBitmask Override bitmask (local pins): a relay is forced ON while its bit is set
   * @param diversionEnabled Whether diversion is enabled - if false, only the forced relays are ON
   *
   * @details This method adjusts the state of the relays based on the current average power.
   * If surplus power is available, it tries to turn ON relays in increasing order. If power
   * is being imported, it tries to turn OFF relays in decreasing order. On a surplus too, a
   * relay in battery mode (negative import threshold) that goes OFF does so in decreasing
   * order, before any relay comes in.
   * When diversion is disabled, the relays that are not forced are turned OFF.
   * The minimum ON and OFF times, and the settle time after any change, always apply.
   */
  void proceed_relays(const uint16_t overrideBitmask, const bool diversionEnabled = true) const
  {
    if (settle_change != 0)
    {
      // A relay has been toggle less than a minute ago, wait until changes take effect
      return;
    }

    if (!diversionEnabled)
    {
      bool anyChanges{ false };
      uint8_t idx{ N };
      do
      {
        anyChanges |= relay[--idx].proceed_forced_only(overrideBitmask);
      } while (idx);

      if (anyChanges)
        settle_change = 60;

      return;
    }

    const auto currentAvgPower{ ewma_average.getAverageT() };

    const bool importing{ currentAvgPower > 0 };

    // First from the last relay down: OFF on an import (where a forced relay comes in too), and
    // on a small surplus for the relays in battery mode. Then, on a surplus only, from the first
    // relay up: ON. One call site, so that the switching code is inlined once.
    for (uint8_t step = 0; step != 2 * N; ++step)
    {
      const bool down{ step < N };
      if (!down && importing)
      {
        return;
      }

      const uint8_t idx{ static_cast< uint8_t >(down ? N - 1 - step : step - N) };
      if (relay[idx].proceed_relay(currentAvgPower, overrideBitmask, importing || !down))
      {
        settle_change = 60;
        return;
      }
    }
  }

protected:
  const R relay[N]; /**< Array of relays */

private:
  /** @brief Copies each relay of @p ref: an array cannot be initialized from another. */
  template< uint8_t... I >
  constexpr Engine(const R (&ref)[N], index_sequence< I... > /* is */)
    : relay{ ref[I]... }
  {
  }

private:
  mutable uint8_t settle_change{ 60 }; /**< Delay in seconds until next change occurs */

  static inline EWMA_average< D * 60 / DATALOG_PERIOD_IN_SECONDS > ewma_average; /**< EWMA average */
};

template< typename R, uint8_t N, uint8_t D > void Engine< R, N, D >::inc_duration() const
{
  uint8_t idx{ N };
  do
  {
    relay[--idx].inc_duration();
  } while (idx);

  if (settle_change)
  {
    --settle_change;
  }
}
}  // namespace RelayLogic

#endif /* RELAY_ENGINE_H */
