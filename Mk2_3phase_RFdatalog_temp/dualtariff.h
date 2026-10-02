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

      if (UINT8_MAX == rg_ForceLoad[i].getDuration())
      {
        _rg[i][1] = rg_ForceLoad[i].getDuration();
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
  INFO(F("\tDuration of off-peak period is "));
  INFO(ul_OFF_PEAK_DURATION);
  INFOLN(F(" hours."));

  INFO(F("\tTemperature threshold is "));
  INFO(iTemperatureThreshold);
  INFOLN(F("°C."));

  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    INFO(F("\tLoad #"));
    INFO(i + 1);
    INFOLN(F(":"));

    INFO(F("\t\tStart "));
    if (rg_ForceLoad[i].getStartOffset() >= 0)
    {
      INFO(rg_ForceLoad[i].getStartOffset());
      INFO(F(" hours/minutes after begin of off-peak period "));
    }
    else
    {
      INFO(-rg_ForceLoad[i].getStartOffset());
      INFO(F(" hours/minutes before the end of off-peak period "));
    }
    if (rg_ForceLoad[i].getDuration() == UINT16_MAX)
    {
      INFOLN(F("till the end of the period."));
    }
    else
    {
      INFO(F("for a duration of "));
      INFO(rg_ForceLoad[i].getDuration());
      INFOLN(F(" hour/minute(s)."));
    }
    INFO(F("\t\tCalculated offset in seconds: "));
    INFOLN(rg_OffsetForce[i][0] * 0.001F);
    INFO(F("\t\tCalculated duration in seconds: "));
    INFOLN(rg_OffsetForce[i][1] * 0.001F);
  }
}

#endif /* DUALTARIFF_H */
