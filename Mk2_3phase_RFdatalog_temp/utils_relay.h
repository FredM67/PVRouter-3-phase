/**
 * @file utils_relay.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief 
 * @version 0.1
 * @date 2023-03-31
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UTILS_RELAY_H
#define _UTILS_RELAY_H

#include "types.h"

/**
 * @brief Config parameters for relay diversion
 * 
 */
template< uint8_t T = 1 > class relayConfig
{
public:
  constexpr relayConfig() = default;

  /**
   * @brief Construct a new relay Config object
   * 
   * @param _surplusThreshold Surplus threshold to turn relay ON
   * @param _importThreshold Import threshold to turn relay OFF
   */
  constexpr relayConfig(int16_t _surplusThreshold, int16_t _importThreshold)
    : surplusThreshold(abs(_surplusThreshold)), importThreshold(abs(_importThreshold))
  {
  }

  /**
   * @brief Construct a new relay Config object
   * 
   * @param _surplusThreshold Surplus threshold to turn relay ON
   * @param _importThreshold Import threshold to turn relay OFF
   * @param _minON Minimum duration in minutes to leave relay ON
   * @param _minOFF Minimum duration in minutes to leave relay OFF
   */
  constexpr relayConfig(int16_t _surplusThreshold, int16_t _importThreshold, uint16_t _minON, uint16_t _minOFF)
    : surplusThreshold(abs(_surplusThreshold)), importThreshold(abs(_importThreshold)), minON(_minON * 60), minOFF(_minOFF * 60)
  {
  }

  /**
   * @brief Get the surplus threshold which will turns ON the relay
   * 
   * @return constexpr auto 
   */
  constexpr auto get_surplusThreshold() const
  {
    return surplusThreshold;
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
   * 
   */
  void inc_duration()
  {
    ++duration;
  }

  /**
   * @brief Proceed with the relay
   * 
   * @param currentAvgPower Current sliding average power
   */
  void proceed_relay(int16_t currentAvgPower)
  {
    if (currentAvgPower > surplusThreshold)
    {
      try_turnON();
    }
    else if (-currentAvgPower > importThreshold)
    {
      try_turnOFF();
    }
  }

private:
  /**
   * @brief Turn ON the relay if the 'time' condition is met
   * 
   */
  void try_turnON()
  {
    if (relayState)
    {
      return;
    }
    if (duration > minOFF)
    {
      relayState = true;
      duration = 0;
    }
  }

  /**
   * @brief Turn OFF the relay if the 'time' condition is met
   * 
   */
  void try_turnOFF()
  {
    if (!relayState)
    {
      return;
    }
    if (duration > minON)
    {
      relayState = false;
      duration = 0;
    }
  }

private:
  int16_t surplusThreshold{ 1000 }; /**< Surplus threshold to turn relay ON */
  int16_t importThreshold{ 200 };   /**< Import threshold to turn relay OFF */
  uint16_t minON{ 5 * 60 };         /**< Minimum duration in seconds the relay is turned ON */
  uint16_t minOFF{ 5 * 60 };        /**< Minimum duration in seconds the relay is turned OFF */

  uint16_t duration{ 0 };           /**< Duration of the current state */
  bool relayState{ false };         /**< State of the relay */
};

#endif  // _UTILS_RELAY_H