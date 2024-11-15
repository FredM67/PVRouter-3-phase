/**
 * @file dualtariff.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Classes/types needed for dual-tariff support
 * @version 0.1
 * @date 2021-10-04
 *
 * @copyright Copyright (c) 2021
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
  const auto (&operator[](uint8_t i) const)
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
  Serial.print(F("\tDuration of off-peak period is "));
  Serial.print(ul_OFF_PEAK_DURATION);
  Serial.println(F(" hours."));

  Serial.print(F("\tTemperature threshold is "));
  Serial.print(iTemperatureThreshold);
  Serial.println(F("°C."));

  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    Serial.print(F("\tLoad #"));
    Serial.print(i + 1);
    Serial.println(F(":"));

    Serial.print(F("\t\tStart "));
    if (rg_ForceLoad[i].getStartOffset() >= 0)
    {
      Serial.print(rg_ForceLoad[i].getStartOffset());
      Serial.print(F(" hours/minutes after begin of off-peak period "));
    }
    else
    {
      Serial.print(-rg_ForceLoad[i].getStartOffset());
      Serial.print(F(" hours/minutes before the end of off-peak period "));
    }
    if (rg_ForceLoad[i].getDuration() == UINT16_MAX)
    {
      Serial.println(F("till the end of the period."));
    }
    else
    {
      Serial.print(F("for a duration of "));
      Serial.print(rg_ForceLoad[i].getDuration());
      Serial.println(F(" hour/minute(s)."));
    }
    Serial.print(F("\t\tCalculated offset in seconds: "));
    Serial.println(rg_OffsetForce[i][0] * 0.001F);
    Serial.print(F("\t\tCalculated duration in seconds: "));
    Serial.println(rg_OffsetForce[i][1] * 0.001F);
  }
}

#endif /* DUALTARIFF_H */
