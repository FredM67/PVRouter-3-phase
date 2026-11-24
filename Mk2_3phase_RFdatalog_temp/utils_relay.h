/**
 * @file utils_relay.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions for the relay output feature
 * @version 0.1
 * @date 2026-11-21
 *
 * @details The decisions (thresholds, minimum ON/OFF times, settle time, order, EWMA average) are
 *          in relay_engine.h, without Arduino. Here, the relays drive their pins and print their
 *          changes and configuration.
 *
 * @copyright Copyright (c) 2023-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef UTILS_RELAY_H
#define UTILS_RELAY_H

#include "types.h"
#include "type_traits.hpp"
#include "debug.h"

#include "relay_engine.h"
#include "utils_pins.h"

/**
 * @brief The output of a relay in the firmware: its pin, and a status message.
 * @ingroup RelayDiversion
 */
struct RelayPinOutput
{
  /**
   * @brief Called on each change of a relay, before its state changes
   *
   * @param pin The control pin of the relay
   * @param on The new state
   */
  __attribute__((always_inline)) static void switched(const uint8_t pin, const bool on)
  {
    setPinState(pin, on);

    statusln(on ? F("Relay turned ON!") : F("Relay turned OFF!"));
  }
};

/**
 * @class relayOutput
 * @brief Represents a single relay configuration and its behavior.
 *
 * The decisions are those of RelayLogic::Relay (relay_engine.h); on each change, the relay
 * pin follows the new state and the change is printed (RelayPinOutput).
 *
 * @ingroup RelayDiversion
 */
class relayOutput : public RelayLogic::Relay< RelayPinOutput >
{
public:
  using RelayLogic::Relay< RelayPinOutput >::Relay;

  /**
   * @brief Print the configuration of the current relay-diversion
   *
   */
  void printRelayConfiguration(uint8_t idx) const
  {
    info(F("\tRelay configuration: #"));
    infoln(idx + 1);

    info(F("\t\tPin is "));
    infoln(get_pin());

    info(F("\t\tSurplus threshold: "));
    infoln(get_surplusThreshold());

    info(F("\t\tImport threshold: "));
    info(get_importThreshold());
    if (get_importThreshold() >= 0)
    {
      infoln(F(" (import mode)"));
    }
    else
    {
      info(F(" (surplus mode: turn OFF when surplus < "));
      info(-get_importThreshold());
      infoln(F("W)"));
    }

    info(F("\t\tMinimum working time in minutes: "));
    infoln(get_minON() / 60);

    info(F("\t\tMinimum stop time in minutes: "));
    infoln(get_minOFF() / 60);
  }
};

/**
 * @class RelayDiverter
 * @brief Manages a collection of relays and their behavior based on surplus and import thresholds.
 *
 * The decisions are those of RelayLogic::Engine (relay_engine.h), on relays that drive their
 * pins (relayOutput). Here, the constructors, which check the sliding average duration, and the
 * printing of the configuration. config_derived.h builds it from the RelayEngine of config.h.
 *
 * @tparam N The number of relays to be managed.
 * @tparam D The duration in minutes for the sliding average (default is 10 minutes).
 *
 * @ingroup RelayDiversion
 */
template< uint8_t N, uint8_t D = 10 >
class RelayDiverter : public RelayLogic::Engine< relayOutput, N, D >
{
  using Base = RelayLogic::Engine< relayOutput, N, D >;

public:
  /**
   * @brief Construct a list of relays
   *
   */
  explicit constexpr RelayDiverter(const relayOutput (&ref)[N])
    : Base(ref)
  {
  }

  /**
   * @brief Construct a list of relays with a custom sliding average.
   *
   * The first argument, an unnamed integral_constant, only carries the sliding average
   * duration D in its type.
   *
   * @param ref Array of relay configurations.
   */
  constexpr RelayDiverter(integral_constant< uint8_t, D > /* ic */, const relayOutput (&ref)[N])
    : Base(ref)
  {
  }

  /**
   * @brief Construct a list of relays with a custom sliding average, given with MINUTES().
   *
   * @tparam V the sliding average duration in minutes: D, once checked to fit 8 bits
   * @param ref Array of relay configurations.
   */
  template< uint16_t V >
  constexpr RelayDiverter(integral_constant< uint16_t, V > /* ic */, const relayOutput (&ref)[N])
    : Base(ref)
  {
    static_assert(V <= UINT8_MAX, "******** RELAY_FILTER_DELAY cannot exceed 255 minutes ! Please check your config.h ! ********");
    static_assert(V == D, "******** The MINUTES() given to RelayDiverter differs from its template parameter D (RELAY_FILTER_DELAY) ! Please check your config.h ! ********");
  }

  /**
   * @brief The relays of config.h.
   *
   * @param relays The relays, as in config.h.
   */
  explicit constexpr RelayDiverter(const RelayEngine< N, D >& relays)
    : RelayDiverter(relays, make_index_sequence< N >{})
  {
  }

  /**
   * @brief Print the configuration of each relay.
   *
   * @details This method outputs the configuration of all relays, including their pin assignments,
   * thresholds, and minimum ON/OFF durations, to the Serial interface.
   */
  void printRelayEngineConfiguration() const
  {
    infoln(F("*** Relay(s) configuration ***"));
    info(F("\tSliding average: "));
    infoln(D);

    for (uint8_t i = 0; i < N; ++i)
    {
      Base::relay[i].printRelayConfiguration(i);
    }
  }

private:
  template< uint8_t... I >
  constexpr RelayDiverter(const RelayEngine< N, D >& relays, index_sequence< I... > /* is */)
    : Base({ relayOutput{ relays.get_relay(I).pin, relays.get_relay(I).surplusThreshold, relays.get_relay(I).importThreshold,
                          relays.get_relay(I).minON, relays.get_relay(I).minOFF }... })
  {
  }
};

// Deduction guides for automatic template parameter deduction
template< uint8_t N >
RelayDiverter(const relayOutput (&)[N]) -> RelayDiverter< N, 10 >;

template< uint8_t N, uint8_t D >
RelayDiverter(integral_constant< uint8_t, D >, const relayOutput (&)[N]) -> RelayDiverter< N, D >;

template< uint8_t N, uint16_t D >
RelayDiverter(integral_constant< uint16_t, D >, const relayOutput (&)[N]) -> RelayDiverter< N, static_cast< uint8_t >(D) >;

template< uint8_t N, uint8_t D >
RelayDiverter(const RelayEngine< N, D >&) -> RelayDiverter< N, D >;

#endif /* UTILS_RELAY_H */
