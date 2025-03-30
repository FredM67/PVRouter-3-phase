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

#include "FastDivision.h"

#include "calibration.h"
#include "constants.h"
#include "dualtariff.h"
#include "processing.h"
#include "teleinfo.h"

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
    DBUGLN(f_powerCal[phase], 6);

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

  DBUG(F("Relay diversion feature "));
  if constexpr (RELAY_DIVERSION)
  {
    DBUGLN(F("is present"));

    relays.printConfiguration();
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
  if constexpr(SERIAL_OUTPUT_TYPE == SerialOutputType::HumanReadable)
  {
    DBUGLN(F("in Human-readable format"));
  }
  else if constexpr(SERIAL_OUTPUT_TYPE == SerialOutputType::IoT)
  {
    DBUGLN(F("in IoT format"));
  }
  else if constexpr(SERIAL_OUTPUT_TYPE == SerialOutputType::EmonCMS)
  {
    DBUGLN(F("in EmonCMS format"));
  }
  else
  {
    DBUGLN(F("is NOT present"));
  }
}

/**
 * @brief Write on Serial in EmonESP format
 * 
 * @param bOffPeak state of on/off-peak period
 */
inline void printForEmonCMS(const bool bOffPeak)
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
    Serial.print(copyOf_countLoadON[idx] * 100 * invDATALOG_PERIOD_IN_MAINS_CYCLES);
  }

  if constexpr (TEMP_SENSOR_PRESENT)
  {  // Current temperature
    for (idx = 0; idx < temperatureSensing.get_size(); ++idx)
    {
      if ((OUTOFRANGE_TEMPERATURE == tx_data.temperature_x100[idx])
          || (DEVICE_DISCONNECTED_RAW == tx_data.temperature_x100[idx]))
      {
        continue;
      }

      Serial.print(F(",T"));
      Serial.print(idx + 1);
      Serial.print(F(":"));
      Serial.print(tx_data.temperature_x100[idx] * 0.01F);
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
 * @brief Prints data logs to the Serial output in text format
 *
 */
inline void printForSerialText()
{
  uint8_t phase{ 0 };

  Serial.print(copyOf_energyInBucket_main * invSUPPLY_FREQUENCY);
  Serial.print(F(", P:"));
  Serial.print(tx_data.power);

  if constexpr (RELAY_DIVERSION)
  {
    Serial.print(F("/"));
    Serial.print(relays.get_average());
  }

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
    Serial.print((float)tx_data.Vrms_L_x100[phase] * 0.01F);
  }

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    for (uint8_t idx = 0; idx < temperatureSensing.get_size(); ++idx)
    {
      if ((OUTOFRANGE_TEMPERATURE == tx_data.temperature_x100[idx])
          || (DEVICE_DISCONNECTED_RAW == tx_data.temperature_x100[idx]))
      {
        continue;
      }

      Serial.print(F(", T"));
      Serial.print(idx + 1);
      Serial.print(F(":"));
      Serial.print((float)tx_data.temperature_x100[idx] * 0.01F);
    }
  }

  Serial.print(F(", (minSampleSets/MC "));
  Serial.print(copyOf_lowestNoOfSampleSetsPerMainsCycle);
  Serial.print(F(", #ofSampleSets "));
  Serial.print(copyOf_sampleSetsDuringThisDatalogPeriod);
#ifndef DUAL_TARIFF
  if constexpr (PRIORITY_ROTATION != RotationModes::OFF)
  {
    Serial.print(F(", NoED "));
    Serial.print(absenceOfDivertedEnergyCount);
  }
#endif  // DUAL_TARIFF
  Serial.println(F(")"));
}

/**
 * @brief Sends telemetry data using the TeleInfo class.
 *
 * This function collects various telemetry data (e.g., power, voltage, temperature, etc.)
 * and sends it in a structured format using the `TeleInfo` class. The data is sent as a
 * telemetry frame, which starts with a frame initialization, includes multiple data points,
 * and ends with a frame finalization.
 *
 * The function supports conditional features such as relay diversion, temperature sensing,
 * and different supply frequencies (50 Hz or 60 Hz).
 *
 * @details
 * - **Power Data**: Sends the total power grid data.
 * - **Relay Data**: If relay diversion is enabled (`RELAY_DIVERSION`), sends the average relay data.
 * - **Voltage Data**: Sends the voltage data for each phase.
 * - **Temperature Data**: If temperature sensing is enabled (`TEMP_SENSOR_PRESENT`), sends valid temperature readings.
 * - **Absence of Diverted Energy Count**: The amount of seconds without diverting energy.
 *
 * @note The function uses compile-time constants (`constexpr`) to include or exclude specific features.
 *       Invalid temperature readings (e.g., `OUTOFRANGE_TEMPERATURE` or `DEVICE_DISCONNECTED_RAW`) are skipped.
 *
 * @throws static_assert If `SUPPLY_FREQUENCY` is not 50 or 60 Hz.
 */
void sendTelemetryData()
{
  static TeleInfo teleInfo;
  uint8_t idx = 0;

  teleInfo.startFrame();  // Start a new telemetry frame

  teleInfo.send("P", tx_data.power);  // Send power grid data

  if constexpr (RELAY_DIVERSION)
  {
    teleInfo.send("R", static_cast< int16_t >(relays.get_average()));  // Send relay average if diversion is enabled

    idx = 0;
    do
    {
      teleInfo.send("R", relays.get_relay(idx).isRelayON());  // Send diverted energy count for each relay
    } while (++idx < relays.get_size());
  }

  idx = 0;
  do
  {
    teleInfo.send("V", tx_data.Vrms_L_x100[idx], idx + 1);  // Send voltage for each phase
  } while (++idx < NO_OF_PHASES);

  idx = 0;
  do
  {
    teleInfo.send("L", copyOf_countLoadON[idx] * 100 * invDATALOG_PERIOD_IN_MAINS_CYCLES, idx + 1);  // Send load ON count for each load
  } while (++idx < NO_OF_DUMPLOADS);
  
  if constexpr (TEMP_SENSOR_PRESENT)
  {
    for (uint8_t idx = 0; idx < temperatureSensing.get_size(); ++idx)
    {
      if ((OUTOFRANGE_TEMPERATURE == tx_data.temperature_x100[idx])
          || (DEVICE_DISCONNECTED_RAW == tx_data.temperature_x100[idx]))
      {
        continue;  // Skip invalid temperature readings
      }
      teleInfo.send("T", tx_data.temperature_x100[idx], idx + 1);  // Send temperature
    }
  }

  teleInfo.send("N", static_cast< int16_t >(absenceOfDivertedEnergyCount));  // Send absence of diverted energy count for 50Hz

  teleInfo.endFrame();  // Finalize and send the telemetry frame
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

  if constexpr (SERIAL_OUTPUT_TYPE == SerialOutputType::HumanReadable)
  {
    printForSerialText();
  }
  else if constexpr (SERIAL_OUTPUT_TYPE == SerialOutputType::IoT)
  {
    sendTelemetryData();
  }
  else if constexpr (SERIAL_OUTPUT_TYPE == SerialOutputType::EmonCMS)
  {
    printForEmonCMS(bOffPeak);
  }
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

#endif  // UTILS_H
