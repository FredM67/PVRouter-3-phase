/**
 * @file dualtariff.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Classes/types needed for dual-tariff support
 * @version 0.1
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2021-2026
 *
 */

#ifndef DUALTARIFF_H
#define DUALTARIFF_H

#include "config.h"

/**
 * @brief Template class for Load-Overriding
 * @details The array is initialized at compile time so it can be read-only and
 *          the performance and code size are better
 *
 * @tparam N # of loads
 * @tparam D
 *
 * @ingroup DualTariff
 */
template< uint8_t N, uint8_t OffPeakDuration = 8 >
class _rg_OffsetForce
{
public:
  constexpr _rg_OffsetForce()
  {
    // calculates offsets for force start and stop of each load (utils_dualtariff.h)
    for (uint8_t i = 0; i != N; ++i)
    {
      _rg[i][0] = DualTariff::windowStart(rg_ForceLoad[i], OffPeakDuration);
      _rg[i][1] = DualTariff::windowEnd(rg_ForceLoad[i], OffPeakDuration);
    }
  }
  const auto(&operator[](uint8_t i) const)
  {
    return _rg[i];
  }

private:
  uint32_t _rg[N][2]{};
};

inline uint32_t ul_TimeOffPeak; /**< 'timestamp' for start of off-peak period */

inline constexpr auto rg_OffsetForce{ _rg_OffsetForce< NO_OF_DUMPLOADS, ul_OFF_PEAK_DURATION >() }; /**< start & stop offsets for each load */

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

  info(F("\tTemperature threshold is "));
  info(iTemperatureThreshold);
  infoln(F(" C."));  // ASCII: the IoT output is 7-bit

  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    info(F("\tLoad #"));
    info(i + 1);
    infoln(F(":"));

    info(F("\t\tStart "));
    if (rg_ForceLoad[i].getStartOffset() >= 0)
    {
      info(rg_ForceLoad[i].getStartOffset());
      info(F(" hours/minutes after begin of off-peak period "));
    }
    else
    {
      info(-rg_ForceLoad[i].getStartOffset());
      info(F(" hours/minutes before the end of off-peak period "));
    }
    if (rg_ForceLoad[i].getDuration() == UINT16_MAX)
    {
      infoln(F("till the end of the period."));
    }
    else
    {
      info(F("for a duration of "));
      info(rg_ForceLoad[i].getDuration());
      infoln(F(" hour/minute(s)."));
    }
    // milliseconds, printed in seconds with 2 decimals, without the float code of Print
    info(F("\t\tCalculated offset in seconds: "));
    infolnDecimals(static_cast< int32_t >(rg_OffsetForce[i][0] / 10), 2);
    info(F("\t\tCalculated duration in seconds: "));
    if (UINT32_MAX == rg_OffsetForce[i][1])
    {
      infoln(F("until the end of off-peak"));
    }
    else
    {
      infolnDecimals(static_cast< int32_t >(rg_OffsetForce[i][1] / 10), 2);
    }
  }
}

#endif /* DUALTARIFF_H */
