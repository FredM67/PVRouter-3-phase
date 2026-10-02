/**
 * @file dualtariff.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Classes/types needed for dual-tariff support
 * @version 0.1
 * @date 2026-10-02
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
    constexpr uint16_t uiPeakDurationInSec{ OffPeakDuration * 3600 };
    // calculates offsets for force start and stop of each load
    for (uint8_t i = 0; i != N; ++i)
    {
      const bool bOffsetInMinutes{ rg_ForceLoad[i].getStartOffset() > 24 || rg_ForceLoad[i].getStartOffset() < -24 };
      const bool bDurationInMinutes{ rg_ForceLoad[i].getDuration() > 24 && UINT16_MAX != rg_ForceLoad[i].getDuration() };

      _rg[i][0] = ((rg_ForceLoad[i].getStartOffset() >= 0) ? 0 : uiPeakDurationInSec) + rg_ForceLoad[i].getStartOffset() * (bOffsetInMinutes ? 60ul : 3600ul);
      _rg[i][0] *= 1000ul;  // convert in milli-seconds

      if (UINT16_MAX == rg_ForceLoad[i].getDuration())
      {
        // until the end of the off-peak period, which stops any forcing anyway
        _rg[i][1] = UINT32_MAX;
      }
      else
      {
        _rg[i][1] = _rg[i][0] + rg_ForceLoad[i].getDuration() * (bDurationInMinutes ? 60ul : 3600ul) * 1000ul;
      }
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
  infoln(F("°C."));

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
