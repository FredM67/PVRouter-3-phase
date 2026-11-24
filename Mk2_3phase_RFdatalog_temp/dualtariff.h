/**
 * @file dualtariff.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The scheduled boosts of the dual tariff, resolved at compile time
 * @version 0.2
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2021-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef DUALTARIFF_H
#define DUALTARIFF_H

#include <avr/pgmspace.h>

#include "config_derived.h"

inline uint32_t ul_TimeOffPeak; /**< 'timestamp' for start of off-peak period */

inline constexpr uint8_t NO_OF_BOOSTS{ boostSchedule.size() }; /**< number of scheduled boosts */

/**
 * @brief The scheduled boosts, resolved for the off-peak duration: window in ms and target masks.
 *
 * @details One entry at least: an array of size 0 is not valid C++. Without any boost, the
 *          entry is never read.
 *
 * @ingroup DualTariff
 */
struct BoostTable
{
  DualTariff::Boost boost[NO_OF_BOOSTS ? NO_OF_BOOSTS : 1];
};

/**
 * @brief Resolves each scheduled boost, at compile time.
 *
 * @ingroup DualTariff
 */
constexpr BoostTable resolveBoosts()
{
  BoostTable table{};
  for (uint8_t i = 0; i != NO_OF_BOOSTS; ++i)
  {
    table.boost[i] = DualTariff::resolve(boostSchedule, i, ul_OFF_PEAK_DURATION);
  }
  return table;
}

inline constexpr BoostTable boostTable PROGMEM{ resolveBoosts() }; /**< the scheduled boosts, in flash */

/**
 * @brief Reads scheduled boost @p i from flash.
 *
 * @param i the boost, below NO_OF_BOOSTS
 * @return its window in ms from the start of the off-peak period, and its target masks
 *
 * @ingroup DualTariff
 */
inline DualTariff::Boost readBoost(const uint8_t i)
{
  DualTariff::Boost boost;
  memcpy_P(&boost, &boostTable.boost[i], sizeof(boost));
  return boost;
}

/**
 * @brief Prints a time in ms from the start of the off-peak period, as <h>h<mm>.
 *
 * @ingroup DualTariff
 */
inline void printOffPeakTime(const uint32_t ms)
{
  const auto minutes{ static_cast< uint16_t >(ms / 60000UL) };
  const uint8_t mm{ static_cast< uint8_t >(minutes % 60) };

  info(minutes / 60);
  info('h');
  if (mm < 10)
  {
    info('0');
  }
  info(mm);
}

/**
 * @brief Print the settings for off-peak period
 *
 * @ingroup DualTariff
 */
inline void printDualTariffConfiguration()
{
  info(F("\tDuration of off-peak period is "));
  info(ul_OFF_PEAK_DURATION);
  infoln(F(" hours."));

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    info(F("\tTemperature threshold is "));
    info(iTemperatureThreshold);
    info(F(" C, on sensor #"));  // ASCII: the IoT output is 7-bit
    info(BOOST_TEMP_SENSOR + 1);
    infoln(F("."));
  }
  else
  {
    infoln(F("\tNo temperature threshold (no temperature sensing)."));
  }

  for (uint8_t i = 0; i != NO_OF_BOOSTS; ++i)
  {
    const auto boost{ readBoost(i) };

    info(F("\tBoost #"));
    info(i + 1);
    info(F(": from "));
    printOffPeakTime(boost.start);
    if (UINT32_MAX == boost.end)
    {
      info(F(" until the end"));
    }
    else
    {
      info(F(" to "));
      printOffPeakTime(boost.end);
    }
    info(F(" of off-peak, forcing "));

    bool first{ true };
    for (uint8_t pin = 0; pin != LOCAL_MASK_BITS; ++pin)
    {
      if (bit_read(boost.local, pin))
      {
        if (!first)
        {
          info(F(", "));
        }
        first = false;
        info('D');
        info(pin);
      }
    }
    for (uint8_t n = 0; n != REMOTE_MASK_BITS; ++n)
    {
      if (bit_read(boost.remote, n))
      {
        if (!first)
        {
          info(F(", "));
        }
        first = false;
        info(F("remote load "));
        info(n + 1);
      }
    }
    if (first)
    {
      info(F("nothing"));
    }
    infoln('.');
  }
}

#endif /* DUALTARIFF_H */
