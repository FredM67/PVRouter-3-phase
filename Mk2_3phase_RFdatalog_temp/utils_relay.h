/**
 * @file utils_relay.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions for the relay output feature
 * @version 0.1
 * @date 2023-03-31
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UTILS_RELAY_H
#define _UTILS_RELAY_H

#include "types.h"

#include "config_system.h"
#include "movingAvg.h"
#include "utils_pins.h"

/**
 * @brief Relay diversion config and engine
 * @details By default, the sliding average is calculated over 1 minute.
 *          If the user wants to calculate over a longer period,
 *          decare the variable like this:
 *          relayOutput<2> relay_Output{ relayPin, 1000, 200, 1, 1 }
 * 
 * @tparam T Duration in minutes of the sliding average
 */
template< uint8_t T = 1 > class relayOutput
{
public:
  constexpr relayOutput() = delete;

  /**
   * @brief Construct a new relay Config object
   * 
   * @param _relay_pin Control pin for the relay
   */
  explicit constexpr relayOutput(const uint8_t _relay_pin)
    : relay_pin(_relay_pin)
  {
  }

  /**
   * @brief Construct a new relay Config object
   * 
   * @param _relay_pin Control pin for the relay
   * @param _surplusThreshold Surplus threshold to turn relay ON
   * @param _importThreshold Import threshold to turn relay OFF
   */
  constexpr relayOutput(uint8_t _relay_pin, int16_t _surplusThreshold, int16_t _importThreshold)
    : relay_pin(_relay_pin), surplusThreshold(-abs(_surplusThreshold)), importThreshold(abs(_importThreshold))
  {
  }

  /**
   * @brief Construct a new relay Config object
   * 
   * @param _relay_pin Control pin for the relay
   * @param _surplusThreshold Surplus threshold to turn relay ON
   * @param _importThreshold Import threshold to turn relay OFF
   * @param _minON Minimum duration in minutes to leave relay ON
   * @param _minOFF Minimum duration in minutes to leave relay OFF
   */
  constexpr relayOutput(uint8_t _relay_pin, int16_t _surplusThreshold, int16_t _importThreshold, uint16_t _minON, uint16_t _minOFF)
    : relay_pin(_relay_pin), surplusThreshold(-abs(_surplusThreshold)), importThreshold(abs(_importThreshold)), minON(_minON * 60), minOFF(_minOFF * 60)
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
   * @brief Get the minimum ON-time
   * 
   * @return constexpr auto 
   */
  constexpr auto get_minON() const
  {
    return minON;
  }

  /**
   * @brief Get the minimum OFF-time
   * 
   * @return constexpr auto 
   */
  constexpr auto get_minOFF() const
  {
    return minOFF;
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
   */
  void proceed_relay() const
  {
    const auto currentAvgPower{ sliding_Average.getAverage() };

    // To avoid changing sign, surplus is a negative value
    if (currentAvgPower < surplusThreshold)
    {
      try_turnON();
    }
    else if (currentAvgPower > importThreshold)
    {
      try_turnOFF();
    }
  }

  inline static auto get_average()
  {
    return sliding_Average.getAverage();
  }

  inline static void update_average(int16_t currentPower)
  {
    sliding_Average.addValue(currentPower);
  }

  /**
   * @brief Print the configuration of the current relay-diversion
   * 
   */
  static void printRelayConfiguration(const relayOutput& relay)
  {
    Serial.println(F("\tRelay configuration:"));

    Serial.print(F("\t\tPin is "));
    Serial.println(relay.get_pin());

    Serial.print(F("\t\tSurplus threshold: "));
    Serial.println(relay.get_surplusThreshold());

    Serial.print(F("\t\tImport threshold: "));
    Serial.println(relay.get_importThreshold());

    Serial.print(F("\t\tMinimum working time in minutes: "));
    Serial.println(relay.get_minON() / 60);

    Serial.print(F("\t\tMinimum stop time in minutes: "));
    Serial.println(relay.get_minOFF() / 60);
  }

private:
  /**
   * @brief Turn ON the relay if the 'time' condition is met
   * 
   */
  void try_turnON()
  {
    if (relayIsON || duration < minOFF)
    {
      return;
    }

    setPinON(relay_pin);

    DBUGLN(F("Relay turned ON!"));

    relayIsON = true;
    duration = 0;
  }

  /**
   * @brief Turn OFF the relay if the 'time' condition is met
   * 
   */
  void try_turnOFF()
  {
    if (!relayIsON || duration < minON)
    {
      return;
    }

    setPinOFF(relay_pin);

    DBUGLN(F("Relay turned OFF!"));

    relayIsON = false;
    duration = 0;
  }

private:
  const uint8_t relay_pin{ 0xff };         /**< Pin associated with the relay */
  const int16_t surplusThreshold{ -1000 }; /**< Surplus threshold to turn relay ON */
  const int16_t importThreshold{ 200 };    /**< Import threshold to turn relay OFF */
  const uint16_t minON{ 5 * 60 };          /**< Minimum duration in seconds the relay is turned ON */
  const uint16_t minOFF{ 5 * 60 };         /**< Minimum duration in seconds the relay is turned OFF */

  mutable uint16_t duration{ 0 };  /**< Duration of the current state */
  mutable bool relayIsON{ false }; /**< True if the relay is ON */

  static inline movingAvg< int16_t, T * 60 / DATALOG_PERIOD_IN_SECONDS > sliding_Average;
};

#endif  // _UTILS_RELAY_H