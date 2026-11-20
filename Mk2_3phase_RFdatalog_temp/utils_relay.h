/**
 * @file utils_relay.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions for the relay output feature
 * @version 0.1
 * @date 2026-11-20
 *
 * @details The decisions (thresholds, minimum ON/OFF times, settle time, order, EWMA average) are
 *          in relay_engine.h, without Arduino. Here, the relays drive their pins and print their
 *          changes and configuration.
 *
 * @copyright Copyright (c) 2023-2026
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
 * @class RelayEngine
 * @brief Manages a collection of relays and their behavior based on surplus and import thresholds.
 *
 * The decisions are those of RelayLogic::Engine (relay_engine.h), on relays that drive their
 * pins (relayOutput). Here, the constructors, which check the sliding average duration, and the
 * printing of the configuration.
 *
 * @tparam N The number of relays to be managed.
 * @tparam D The duration in minutes for the sliding average (default is 10 minutes).
 *
 * @ingroup RelayDiversion
 */
template< uint8_t N, uint8_t D = 10 >
class RelayEngine : public RelayLogic::Engine< relayOutput, N, D >
{
  using Base = RelayLogic::Engine< relayOutput, N, D >;

public:
  /**
   * @brief Construct a list of relays
   *
   */
  explicit constexpr RelayEngine(const relayOutput (&ref)[N])
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
  constexpr RelayEngine(integral_constant< uint8_t, D > /* ic */, const relayOutput (&ref)[N])
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
  constexpr RelayEngine(integral_constant< uint16_t, V > /* ic */, const relayOutput (&ref)[N])
    : Base(ref)
  {
    static_assert(V <= UINT8_MAX, "******** RELAY_FILTER_DELAY cannot exceed 255 minutes ! Please check your config.h ! ********");
    static_assert(V == D, "******** The MINUTES() given to RelayEngine differs from its template parameter D (RELAY_FILTER_DELAY) ! Please check your config.h ! ********");
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
};

// Deduction guides for automatic template parameter deduction
template< uint8_t N >
RelayEngine(const relayOutput (&)[N]) -> RelayEngine< N, 10 >;

template< uint8_t N, uint8_t D >
RelayEngine(integral_constant< uint8_t, D >, const relayOutput (&)[N]) -> RelayEngine< N, D >;

template< uint8_t N, uint16_t D >
RelayEngine(integral_constant< uint16_t, D >, const relayOutput (&)[N]) -> RelayEngine< N, static_cast< uint8_t >(D) >;

#endif /* UTILS_RELAY_H */
