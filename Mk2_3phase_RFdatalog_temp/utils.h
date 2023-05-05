/**
 * @file utils.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions
 * @version 0.1
 * @date 2023-02-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef _UTILS_H
#define _UTILS_H

#include "config.h"
#include "constants.h"
#include "dualtariff.h"
#include "processing.h"

#include "utils_rf.h"
#include "utils_temp.h"

/**
 * @brief Print the configuration during start
 *
 */
inline void printConfiguration()
{
#ifndef PROJECT_PATH
#define PROJECT_PATH (__FILE__)
#endif

#ifndef BRANCH_NAME
#define BRANCH_NAME ("N/A")
#endif
#ifndef COMMIT_HASH
#define COMMIT_HASH ("N/A")
#endif

  DBUGLN();
  DBUGLN();
  DBUGLN(F("----------------------------------"));
  DBUG(F("Sketch ID: "));
  DBUGLN(F(PROJECT_PATH));

  DBUG(F("From branch '"));
  DBUG(F(BRANCH_NAME));
  DBUG(F("', commit "));
  DBUGLN(F(COMMIT_HASH));

  DBUG(F("Build on "));
#ifdef CURRENT_TIME
  DBUGLN(F(CURRENT_TIME));
#else
  DBUG(F(__DATE__));
  DBUG(F(" "));
  DBUGLN(F(__TIME__));
#endif
  DBUGLN(F("ADC mode:       free-running"));

  DBUGLN(F("Electrical settings"));
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    DBUG(F("\tf_powerCal for L"));
    DBUG(phase + 1);
    DBUG(F(" =    "));
    DBUGLN(f_powerCal[phase], 5);

    DBUG(F("\tf_voltageCal, for Vrms_L"));
    DBUG(phase + 1);
    DBUG(F(" =    "));
    DBUGLN(f_voltageCal[phase], 5);
  }

  DBUG(F("\tf_phaseCal for all phases =     "));
  DBUGLN(f_phaseCal);

  DBUG(F("\tExport rate (Watts) = "));
  DBUGLN(REQUIRED_EXPORT_IN_WATTS);

  DBUG(F("\tzero-crossing persistence (sample sets) = "));
  DBUGLN(PERSISTENCE_FOR_POLARITY_CHANGE);

  printParamsForSelectedOutputMode();

  DBUG(F("Temperature capability "));
  if constexpr (TEMP_SENSOR_PRESENT)
  {
    DBUGLN(F("is present"));
  }
  else
  {
    DBUGLN(F("is NOT present"));
  }
  DBUG(F("Dual-tariff capability "));
  if constexpr (DUAL_TARIFF)
  {
    DBUGLN(F("is present"));
    printDualTariffConfiguration();
  }
  else
  {
    DBUGLN(F("is NOT present"));
  }
  DBUG(F("Load rotation feature "));
  if constexpr (PRIORITY_ROTATION != RotationModes::OFF)
  {
    DBUGLN(F("is present"));
  }
  else
  {
    DBUGLN(F("is NOT present"));
  }

  DBUG(F("RF capability "));
#ifdef RF_PRESENT
  DBUG(F("IS present, Freq = "));
  if (FREQ == RF12_433MHZ)
    DBUGLN(F("433 MHz"));
  else if (FREQ == RF12_868MHZ)
    DBUGLN(F("868 MHz"));
  rf12_initialize(nodeID, FREQ, networkGroup);  // initialize RF
#else
  DBUGLN(F("is NOT present"));
#endif

  DBUG(F("Datalogging capability "));
#ifdef SERIALPRINT
  DBUGLN(F("is present"));
#else
  DBUGLN(F("is NOT present"));
#endif
}

/**
 * @brief Write on Serial in EmonESP format
 * 
 * @param bOffPeak state of on/off-peak period
 */
inline void printForEmonESP(const bool bOffPeak)
{
  uint8_t idx{ 0 };

  // Total mean power over a data logging period
  Serial.print(F("P:"));
  Serial.print(tx_data.power);

  // Mean power for each phase over a data logging period
  for (idx = 0; idx < NO_OF_PHASES; ++idx)
  {
    Serial.print(F(",P"));
    Serial.print(idx + 1);
    Serial.print(F(":"));
    Serial.print(tx_data.power_L[idx]);
  }
  // Mean power for each load over a data logging period (in %)
  for (idx = 0; idx < NO_OF_DUMPLOADS; ++idx)
  {
    Serial.print(F(",L"));
    Serial.print(idx + 1);
    Serial.print(F(":"));
    Serial.print((copyOf_countLoadON[idx] * 100) / DATALOG_PERIOD_IN_MAINS_CYCLES);
  }

  if constexpr (TEMP_SENSOR_PRESENT)
  {  // Current temperature
    for (idx = 0; idx < size(tx_data.temperature_x100); ++idx)
    {
      if ((OUTOFRANGE_TEMPERATURE == tx_data.temperature_x100[idx])
          || (DEVICE_DISCONNECTED_RAW == tx_data.temperature_x100[idx]))
      {
        continue;
      }

      Serial.print(F(",T"));
      Serial.print(idx + 1);
      Serial.print(F(":"));
      Serial.print((float)tx_data.temperature_x100[idx] / 100);
    }
  }

  if constexpr (DUAL_TARIFF)
  {
    // Current tariff
    Serial.print(F(",T:"));
    Serial.print(bOffPeak ? "low" : "high");
  }
  Serial.println(F(""));
}

/**
 * @brief Prints data logs to the Serial output in Json format
 *
 */
inline void printForSerialJson()
{
  uint8_t phase{ 0 };

  Serial.print(copyOf_energyInBucket_main / SUPPLY_FREQUENCY);
  Serial.print(F(", P:"));
  Serial.print(tx_data.power);

  for (phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    Serial.print(F(", P"));
    Serial.print(phase + 1);
    Serial.print(F(":"));
    Serial.print(tx_data.power_L[phase]);
  }
  for (phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    Serial.print(F(", V"));
    Serial.print(phase + 1);
    Serial.print(F(":"));
    Serial.print((float)tx_data.Vrms_L_x100[phase] / 100);
  }

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    for (uint8_t idx = 0; idx < size(tx_data.temperature_x100); ++idx)
    {
      Serial.print(F(", T"));
      Serial.print(idx + 1);
      Serial.print(F(":"));
      Serial.print((float)tx_data.temperature_x100[idx] / 100);
    }
  }

  Serial.println(F(")"));
}

/**
 * @brief Prints data logs to the Serial output in text format
 *
 */
inline void printForSerialText()
{
  uint8_t phase{ 0 };

  Serial.print(copyOf_energyInBucket_main / SUPPLY_FREQUENCY);
  Serial.print(F(", P:"));
  Serial.print(tx_data.power);

  for (phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    Serial.print(F(", P"));
    Serial.print(phase + 1);
    Serial.print(F(":"));
    Serial.print(tx_data.power_L[phase]);
  }
  for (phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    Serial.print(F(", V"));
    Serial.print(phase + 1);
    Serial.print(F(":"));
    Serial.print((float)tx_data.Vrms_L_x100[phase] / 100);
  }

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    for (uint8_t idx = 0; idx < size(tx_data.temperature_x100); ++idx)
    {
      Serial.print(F(", T"));
      Serial.print(idx + 1);
      Serial.print(F(":"));
      Serial.print((float)tx_data.temperature_x100[idx] / 100);
    }
  }

  Serial.print(F(", (minSampleSets/MC "));
  Serial.print(copyOf_lowestNoOfSampleSetsPerMainsCycle);
  Serial.print(F(", #ofSampleSets "));
  Serial.print(copyOf_sampleSetsDuringThisDatalogPeriod);
#ifndef DUAL_TARIFF
#ifdef PRIORITY_ROTATION
  Serial.print(F(", NoED "));
  Serial.print(absenceOfDivertedEnergyCount);
#endif  // PRIORITY_ROTATION
#endif  // DUAL_TARIFF
  Serial.println(F(")"));
}

/**
 * @brief Prints data logs to the Serial output in text or json format
 *
 * @param bOffPeak true if off-peak tariff is active
 */
inline void sendResults(bool bOffPeak)
{
  static bool startup{ true };

  if (startup)
  {
    startup = false;
    return;  // reject the first datalogging which is incomplete !
  }

#ifdef RF_PRESENT
  send_rf_data();  // *SEND RF DATA*
#endif

#if defined SERIALOUT
  printForSerialJson();
#endif  // if defined SERIALOUT

  if constexpr (EMONESP_CONTROL)
    printForEmonESP(bOffPeak);

#if defined SERIALPRINT && !defined EMONESP
  printForSerialText();
#endif  // if defined SERIALPRINT && !defined EMONESP
}

/**
 * @brief Prints the load priorities to the Serial output.
 *
 */
inline void logLoadPriorities()
{
#ifdef ENABLE_DEBUG

  DBUGLN(F("Load Priorities: "));
  for (const auto& loadPrioAndState : loadPrioritiesAndState)
  {
    DBUG(F("\tload "));
    DBUGLN(loadPrioAndState);
  }

#endif
}

/**
 * @brief Get the available RAM during setup
 *
 * @return int The amount of free RAM
 */
inline int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

#endif  // _UTILS_H
