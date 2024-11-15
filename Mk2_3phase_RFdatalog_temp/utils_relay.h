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

#ifndef UTILS_RELAY_H
#define UTILS_RELAY_H

#include "types.h"
#include "type_traits.hpp"
#include "debug.h"

#include "config_system.h"
#include "movingAvg.h"
#include "ewma_avg.hpp"
#include "utils_pins.h"

/**
 * @brief Relay diversion config and engine
 * 
 * @ingroup RelayDiversion
 */
class relayOutput
{
public:
  constexpr relayOutput() = delete;

  /**
   * @brief Construct a new relay Config object with default parameters
   * 
   * @param _relay_pin Control pin for the relay
   */
  explicit constexpr relayOutput(const uint8_t _relay_pin)
    : relay_pin{ _relay_pin }
  {
  }

  /**
   * @brief Construct a new relay Config object with default/custom parameters
   * 
   * @param _relay_pin Control pin for the relay
   * @param _surplusThreshold Surplus threshold to turn relay ON
   * @param _importThreshold Import threshold to turn relay OFF
   */
  constexpr relayOutput(uint8_t _relay_pin, int16_t _surplusThreshold, int16_t _importThreshold)
    : relay_pin{ _relay_pin }, surplusThreshold{ -abs(_surplusThreshold) }, importThreshold{ abs(_importThreshold) }
  {
  }

  /**
   * @brief Construct a new relay Config object with custom parameters
   * 
   * @param _relay_pin Control pin for the relay
   * @param _surplusThreshold Surplus threshold to turn relay ON
   * @param _importThreshold Import threshold to turn relay OFF
   * @param _minON Minimum duration in minutes to leave relay ON
   * @param _minOFF Minimum duration in minutes to leave relay OFF
   */
  constexpr relayOutput(uint8_t _relay_pin, int16_t _surplusThreshold, int16_t _importThreshold, uint16_t _minON, uint16_t _minOFF)
    : relay_pin{ _relay_pin }, surplusThreshold{ -abs(_surplusThreshold) }, importThreshold{ abs(_importThreshold) }, minON{ _minON * 60 }, minOFF{ _minOFF * 60 }
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
   * @return bool True if state has changed
   */
  bool proceed_relay(const int32_t currentAvgPower) const
  {
    // To avoid changing sign, surplus is a negative value
    if (currentAvgPower < surplusThreshold)
    {
      return try_turnON();
    }
    if (currentAvgPower > importThreshold)
    {
      return try_turnOFF();
    }
    return false;
  }

  /**
   * @brief Print the configuration of the current relay-diversion
   * 
   */
  void printRelayConfiguration(uint8_t idx) const
  {
    Serial.print(F("\tRelay configuration: #"));
    Serial.println(idx + 1);

    Serial.print(F("\t\tPin is "));
    Serial.println(get_pin());

    Serial.print(F("\t\tSurplus threshold: "));
    Serial.println(get_surplusThreshold());

    Serial.print(F("\t\tImport threshold: "));
    Serial.println(get_importThreshold());

    Serial.print(F("\t\tMinimum working time in minutes: "));
    Serial.println(get_minON() / 60);

    Serial.print(F("\t\tMinimum stop time in minutes: "));
    Serial.println(get_minOFF() / 60);
  }

private:
  /**
   * @brief Turn ON the relay if the 'time' condition is met
   * 
   * @return bool True if state has changed
   */
  bool try_turnON() const
  {
    if (relayIsON || duration < minOFF)
    {
      return false;
    }

    setPinON(relay_pin);

    DBUGLN(F("Relay turned ON!"));

    relayIsON = true;
    duration = 0;

    return true;
  }

  /**
   * @brief Turn OFF the relay if the 'time' condition is met
   * 
   * @return bool True if state has changed
   */
  bool try_turnOFF() const
  {
    if (!relayIsON || duration < minON)
    {
      return false;
    }

    setPinOFF(relay_pin);

    DBUGLN(F("Relay turned OFF!"));

    relayIsON = false;
    duration = 0;

    return true;
  }

private:
  const uint8_t relay_pin{ 0xff };         /**< Pin associated with the relay */
  const int16_t surplusThreshold{ -1000 }; /**< Surplus threshold to turn relay ON */
  const int16_t importThreshold{ 200 };    /**< Import threshold to turn relay OFF */
  const uint16_t minON{ 5 * 60 };          /**< Minimum duration in seconds the relay is turned ON */
  const uint16_t minOFF{ 5 * 60 };         /**< Minimum duration in seconds the relay is turned OFF */

  mutable uint16_t duration{ 0 };  /**< Duration of the current state */
  mutable bool relayIsON{ false }; /**< True if the relay is ON */
};

/**
 * @brief This class implements the relay management engine
 * 
 * @tparam D The duration in minutes of the sliding average
 * @tparam N The number of relays to be used. This parameter is deduced automatically.
 * 
 * @ingroup RelayDiversion
 */
template< uint8_t N, uint8_t D = 10 >
class RelayEngine
{
public:
  /**
   * @brief Construct a list of relays
   * 
   */
  explicit constexpr RelayEngine(const relayOutput (&ref)[N])
    : relay(ref)
  {
  }

  /**
   * @brief Construct a list of relays with a custom sliding average
   * 
   */
  constexpr RelayEngine(integral_constant< uint8_t, D > ic, const relayOutput (&ref)[N])
    : relay(ref)
  {
  }

  /**
   * @brief Get the number of relays
   * 
   * @return constexpr auto The number of relays
   */
  constexpr auto get_size() const
  {
    return N;
  }

  /**
   * @brief Get the relay object
   * 
   * @tparam idx The index of the relay
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
    return ewma_average.getAverageS();
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
   * @brief Increment the duration's state of each relay
   * 
   */
#if defined(__DOXYGEN__)
  void inc_duration() const;
#else
  void inc_duration() const __attribute__((optimize("-O3")));
#endif

  /**
   * @brief Proceed all relays in increasing order (surplus) or decreasing order (import)
   * 
   */
  void proceed_relays() const
  {
    if (settle_change != 0)
    {
      // A relay has been toggle less than a minute ago, wait until changes take effect
      return;
    }

    if (ewma_average.getAverageS() > 0)
    {
      // Currently importing, try to turn OFF some relays
      uint8_t idx{ N };
      do
      {
        if (relay[--idx].proceed_relay(ewma_average.getAverageS()))
        {
          settle_change = 60;
          return;
        }
      } while (idx);
    }
    else
    {
      // Remaining surplus, try to turn ON more relays
      uint8_t idx{ 0 };
      do
      {
        if (relay[idx].proceed_relay(ewma_average.getAverageS()))
        {
          settle_change = 60;
          return;
        }
      } while (++idx < N);
    }
  }

  /**
   * @brief Initialize the pins used by the relays
   * 
   */
  void initializePins() const
  {
    uint8_t idx{ N };
    do
    {
      pinMode(relay[--idx].get_pin(), OUTPUT);
      delay(100);
    } while (idx);
  }

  /**
   * @brief Print the configuration of each relay
   * 
   */
  void printConfiguration() const
  {
    Serial.println(F("\t*** Relay(s) configuration ***"));
    Serial.print(F("\t\tSliding average: "));
    Serial.println(D);

    for (uint8_t i = 0; i < N; ++i)
    {
      relay[i].printRelayConfiguration(i);
    }
  }

private:
  const relayOutput relay[N]; /**< Array of relays */

  mutable uint8_t settle_change{ 60 }; /**< Delay in seconds until next change occurs */

  static inline EWMA_average< D * 60 / DATALOG_PERIOD_IN_SECONDS > ewma_average; /**< EWMA average */
};

template< uint8_t N, uint8_t D > void RelayEngine< N, D >::inc_duration() const
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

#endif /* UTILS_RELAY_H */
