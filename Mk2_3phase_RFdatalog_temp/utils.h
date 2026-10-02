/**
 * @file utils.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions
 * @version 0.1
 * @date 2026-10-02
 *
 * @copyright Copyright (c) 2023-2026
 *
 */

#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

#include "FastDivision.h"

#include "calibration.h"
#include "constants.h"
#include "dualtariff.h"
#include "energy_bucket.h"
#include "processing.h"
#include "serial_output.h"
#include "shared_var.h"
#include "teleinfo.h"

#include "utils_rf.h"
#include "utils_temp.h"

#include "version.h"

/**
 * @brief Print the configuration during startup.
 *
 * This function outputs the system configuration to the Serial output during startup.
 * It includes details about the sketch, build information, electrical settings, and
 * enabled features.
 *
 * @details
 * - Prints the sketch ID, branch name, commit hash, and build date/time.
 * - Outputs electrical settings such as power calibration, voltage calibration, and phase calibration.
 * - Displays enabled features like temperature sensing, dual tariff, load rotation, relay diversion, and RF communication.
 * - Logs the selected datalogging format (Human-readable, IoT, or JSON).
 *
 * @ingroup Initialization
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
#ifndef BUILD_ENV
#define BUILD_ENV ("N/A")
#endif

  INFOLN();
  INFOLN();
  INFOLN(F("----------------------------------"));
  INFO(F("Sketch ID: "));
  INFOLN(F(PROJECT_PATH));

  INFO(F("From branch '"));
  INFO(F(BRANCH_NAME));
  INFO(F("', commit "));
  INFOLN(F(COMMIT_HASH));

  INFO(F("Build environment: "));
  INFOLN(F(BUILD_ENV));

  INFO(F("Build on "));
#ifdef CURRENT_TIME
  INFOLN(F(CURRENT_TIME));
#else
  INFO(F(__DATE__));
  INFO(F(" "));
  INFOLN(F(__TIME__));
#endif
  INFOLN(F("ADC mode:       free-running"));

  if constexpr (CALIBRATION_MODE)
  {
    INFOLN(F("*** CALIBRATION MODE: no load will ever be switched ***"));
  }

  INFOLN(F("Electrical settings"));
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    INFO(F("\tf_powerCal for L"));
    INFO(phase + 1);
    INFO(F(" =    "));
    INFOLN(f_powerCal[phase], 6);

    INFO(F("\tf_voltageCal, for Vrms_L"));
    INFO(phase + 1);
    INFO(F(" =    "));
    INFOLN(f_voltageCal[phase], 5);
  }

  INFO(F("\tf_phaseCal for all phases =     "));
  INFOLN(f_phaseCal);

  INFO(F("\tExport rate (Watts) = "));
  INFOLN(REQUIRED_EXPORT_IN_WATTS);

  INFO(F("\tzero-crossing persistence (sample sets) = "));
  INFOLN(PERSISTENCE_FOR_POLARITY_CHANGE);

  printParamsForSelectedOutputMode();

  INFO(F("Temperature capability "));
  if constexpr (TEMP_SENSOR_PRESENT)
  {
    INFOLN(F("is present"));
  }
  else
  {
    INFOLN(F("is NOT present"));
  }

  INFO(F("Dual-tariff capability "));
  if constexpr (DUAL_TARIFF)
  {
    INFOLN(F("is present"));
    printDualTariffConfiguration();
  }
  else
  {
    INFOLN(F("is NOT present"));
  }

  INFO(F("Load rotation feature "));
  if constexpr (PRIORITY_ROTATION != RotationModes::OFF)
  {
    INFOLN(F("is present"));
  }
  else
  {
    INFOLN(F("is NOT present"));
  }

  INFO(F("Relay diversion feature "));
  if constexpr (RELAY_DIVERSION)
  {
    INFOLN(F("is present"));

    relays.printRelayEngineConfiguration();
  }
  else
  {
    INFOLN(F("is NOT present"));
  }

  INFO(F("Override feature "));
  if constexpr (OVERRIDE_PIN_PRESENT)
  {
    INFOLN(F("is present"));

    overridePins.printOverrideConfig();
  }
  else
  {
    INFOLN(F("is NOT present"));
  }

  INFO(F("RF capability "));
  if constexpr (RF_CHIP_PRESENT)
  {
    INFO(F("IS present, Freq = "));
    if constexpr (SharedRF::FREQUENCY == RF69_433MHZ)
      INFOLN(F("433 MHz"));
    else if constexpr (SharedRF::FREQUENCY == RF69_868MHZ)
      INFOLN(F("868 MHz"));
    else if constexpr (SharedRF::FREQUENCY == RF69_915MHZ)
      INFOLN(F("915 MHz"));

    INFO(F("  Network ID: "));
    INFOLN(SharedRF::NETWORK_ID);
    INFO(F("  Node ID: "));
    INFOLN(SharedRF::ROUTER_NODE_ID);

    if constexpr (RF_LOGGING_PRESENT)
    {
      INFO(F("  Data logging to Gateway ID: "));
      INFOLN(SharedRF::GATEWAY_ID);
    }

    if constexpr (REMOTE_LOADS_PRESENT)
    {
      INFO(F("  Remote loads to Node ID: "));
      for (uint8_t idx = 0; idx != NO_OF_REMOTE_UNITS; ++idx)
      {
        INFO(SharedRF::REMOTE_NODE_ID[idx]);
        INFO(' ');
      }
      INFOLN();
    }
  }
  else
  {
    INFOLN(F("is NOT present"));
  }

  INFO(F("Datalogging capability "));
  if constexpr (SERIAL_OUTPUT_TYPE == SerialOutputType::HumanReadable)
  {
    INFOLN(F("in Human-readable format"));
  }
  else if constexpr (SERIAL_OUTPUT_TYPE == SerialOutputType::IoT)
  {
    INFOLN(F("in IoT format"));
  }
  else if constexpr (SERIAL_OUTPUT_TYPE == SerialOutputType::JSON)
  {
    INFOLN(F("in JSON format"));
  }
  else
  {
    INFOLN(F("is NOT present"));
  }
}

/**
 * @brief Whether a temperature reading is valid, and so worth sending.
 *
 * @param temperature_x100 The reading, in hundredths of °C.
 */
inline bool isValidTemperature(int16_t temperature_x100)
{
  return (OUTOFRANGE_TEMPERATURE != temperature_x100) && (DEVICE_DISCONNECTED_RAW != temperature_x100);
}

inline bool datalogOffPeak{ false }; /**< tariff at the datalog being printed, for the JSON format */

/**
 * @brief One step of the datalog in JSON format.
 *
 * @details One field per step (see serial_output.h), which gives:
 *          {"P":-25,"R":0,"P1":399,"P2":399,"P3":-823,"T1":21.37,"TA":"low"}
 * - total mean power over the datalog period, and the relays' average if relay diversion is enabled;
 * - mean power of each phase;
 * - each valid temperature, if temperature sensing is enabled;
 * - the tariff, if dual tariff is enabled.
 *
 * @param out Where to write.
 * @param step Index of the step.
 * @return false once the line is complete.
 *
 * @ingroup Telemetry
 */
inline bool printJsonStep(Print& out, uint8_t step)
{
  if (step == 0)
  {
    out.print(F("{\"P\":"));
    out.print(tx_data.power);

    if constexpr (RELAY_DIVERSION)
    {
      out.print(F(",\"R\":"));
      out.print(relays.get_average());
    }
    return true;
  }
  --step;

  if (step < NO_OF_PHASES)
  {
    out.print(F(",\"P"));
    out.print(step + 1);
    out.print(F("\":"));
    out.print(tx_data.power_L[step]);
    return true;
  }
  step -= NO_OF_PHASES;

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    if (step < temperatureSensing.size())
    {
      if (isValidTemperature(tx_data.temperature_x100[step]))
      {
        out.print(F(",\"T"));
        out.print(step + 1);
        out.print(F("\":"));
        SerialOutput::printHundredths(out, tx_data.temperature_x100[step]);
      }
      return true;
    }
    step -= temperatureSensing.size();
  }

  if constexpr (DUAL_TARIFF)
  {
    if (step == 0)
    {
      out.print(datalogOffPeak ? F(",\"TA\":\"low\"") : F(",\"TA\":\"high\""));
      return true;
    }
  }

  out.println('}');
  return false;
}

/**
 * @brief One step of the datalog in text format.
 *
 * @details One field per step (see serial_output.h):
 * - energy in the bucket, total power (and the relays' average if relay diversion is enabled);
 * - power and RMS voltage of each phase;
 * - each valid temperature, if temperature sensing is enabled;
 * - the number of sample sets, and the absence of diverted energy count if priority rotation is automatic.
 *
 * @param out Where to write.
 * @param step Index of the step.
 * @return false once the line is complete.
 *
 * @ingroup Telemetry
 */
inline bool printTextStep(Print& out, uint8_t step)
{
  if (step == 0)
  {
    out.print(static_cast< float >(Shared::copyOf_energyInBucket_main) * (invSUPPLY_FREQUENCY / (1 << Energy::FRACTION_BITS)));
    return true;
  }
  if (step == 1)
  {
    out.print(F(", P:"));
    out.print(tx_data.power);

    if constexpr (RELAY_DIVERSION)
    {
      out.print(F("/"));
      out.print(relays.get_average());
    }
    return true;
  }
  step -= 2;

  if (step < NO_OF_PHASES)
  {
    out.print(F(", P"));
    out.print(step + 1);
    out.print(F(":"));
    out.print(tx_data.power_L[step]);
    return true;
  }
  step -= NO_OF_PHASES;

  if (step < NO_OF_PHASES)
  {
    out.print(F(", V"));
    out.print(step + 1);
    out.print(F(":"));
    out.print((float)tx_data.Vrms_L_x100[step] * 0.01F);
    return true;
  }
  step -= NO_OF_PHASES;

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    if (step < temperatureSensing.size())
    {
      if (isValidTemperature(tx_data.temperature_x100[step]))
      {
        out.print(F(", T"));
        out.print(step + 1);
        out.print(F(":"));
        out.print((float)tx_data.temperature_x100[step] * 0.01F);
      }
      return true;
    }
    step -= temperatureSensing.size();
  }

  switch (step)
  {
    case 0:
      out.print(F(", (minSampleSets/MC "));
      out.print(Shared::copyOf_lowestNoOfSampleSetsPerMainsCycle);
      return true;
    case 1:
      out.print(F(", #ofSampleSets "));
      out.print(Shared::copyOf_sampleSetsDuringThisDatalogPeriod);
      return true;
    case 2:
      if constexpr (!DUAL_TARIFF && PRIORITY_ROTATION != RotationModes::OFF)
      {
        out.print(F(", NoED "));
        out.print(Shared::absenceOfDivertedEnergyCountInSeconds);
      }
      return true;
    default:
      out.println(F(")"));
      return false;
  }
}

inline TeleInfo teleInfo; /**< telemetry frame, in IoT format */

/**
 * @brief One step of the telemetry frame: its next part.
 *
 * @param out Where to write.
 * @return false once the frame is complete.
 *
 * @ingroup Telemetry
 */
inline bool writeTeleInfoStep(Print& out, uint8_t /*step*/)
{
  return teleInfo.writeNext(out, SerialOutput::MAX_STEP_LENGTH);
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
 * dual tariff information, and different supply frequencies (50 Hz or 60 Hz).
 *
 * @param bOffPeak Indicates whether the system is in an off-peak tariff period.
 *
 * @details
 * - **Power Data**: Sends the total power grid data.
 * - **Relay Data**: If relay diversion is enabled (`RELAY_DIVERSION`), sends the average relay data.
 * - **Voltage Data**: Sends the voltage data for each phase.
 * - **Temperature Data**: If temperature sensing is enabled (`TEMP_SENSOR_PRESENT`), sends valid temperature readings.
 * - **Dual Tariff Data**: If dual tariff is enabled (`DUAL_TARIFF`), sends the current tariff state.
 * - **Absence of Diverted Energy Count**: The amount of seconds without diverting energy.
 *
 * @note The function uses compile-time constants (`constexpr`) to include or exclude specific features.
 *       Invalid temperature readings (e.g., `OUTOFRANGE_TEMPERATURE` or `DEVICE_DISCONNECTED_RAW`) are skipped.
 *
 * @throws static_assert If `SUPPLY_FREQUENCY` is not 50 or 60 Hz.
 */
void sendTelemetryData(const bool bOffPeak)
{
  uint8_t idx{ 0 };

  teleInfo.startFrame();  // Start a new telemetry frame

  teleInfo.send("P", tx_data.power);  // Send power grid data

  if constexpr (NO_OF_PHASES > 1)
  {
    idx = NO_OF_PHASES;
    do
    {
      --idx;
      teleInfo.send("P", tx_data.power_L[idx], idx + 1);      // Send power for each phase
      teleInfo.send("V", tx_data.Vrms_L_x100[idx], idx + 1);  // Send voltage for each phase
    } while (idx);
  }

  if constexpr (RELAY_DIVERSION)
  {
    teleInfo.send("R", static_cast< int16_t >(relays.get_average()));  // Send relay average if diversion is enabled

    idx = relays.size();
    do
    {
      --idx;
      teleInfo.send("R", relays.get_relay(idx).isRelayON());  // Send state of each relay
    } while (idx);
  }

  idx = NO_OF_DUMPLOADS;
  do
  {
    --idx;
    teleInfo.send("D", Shared::copyOf_countLoadON[idx] * 100 * invDATALOG_PERIOD_IN_MAINS_CYCLES, idx + 1);  // Send load ON count for each load
  } while (idx);

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    for (uint8_t idx = 0; idx < temperatureSensing.size(); ++idx)
    {
      if ((OUTOFRANGE_TEMPERATURE == tx_data.temperature_x100[idx])
          || (DEVICE_DISCONNECTED_RAW == tx_data.temperature_x100[idx]))
      {
        continue;  // Skip invalid temperature readings
      }
      teleInfo.send("T", tx_data.temperature_x100[idx], idx + 1);  // Send temperature
    }
  }

  teleInfo.send("N", static_cast< int16_t >(Shared::absenceOfDivertedEnergyCountInSeconds));  // Send absence of diverted energy count for 50Hz

  if constexpr (DUAL_TARIFF)
  {
    teleInfo.send("TA", static_cast< int16_t >(bOffPeak ? 1 : 0));  // Send current tariff state (0=high/on-peak, 1=low/off-peak)
  }

  teleInfo.send("S", Shared::copyOf_sampleSetsDuringThisDatalogPeriod);
  teleInfo.send("S_MC", Shared::copyOf_lowestNoOfSampleSetsPerMainsCycle);

  teleInfo.endFrame();  // Finalize the telemetry frame, written out by writeTeleInfoStep()
}

/**
 * @brief Prints or sends telemetry data logs based on the selected output format.
 *
 * This function handles the transmission of telemetry data in various formats, such as
 * human-readable text, IoT telemetry, or JSON format. It also ensures that the first
 * incomplete datalogging event is skipped during startup.
 *
 * @param bOffPeak Indicates whether the system is in an off-peak tariff period.
 *
 * @details
 * - If RF communication is enabled, it sends RF data.
 * - Depending on the `SERIAL_OUTPUT_TYPE`, it prints data in text format, sends telemetry
 *   data, or outputs data in JSON format.
 * - Skips the first datalogging event during startup to avoid incomplete data.
 *
 * @ingroup GeneralProcessing
 */
inline void sendResults(bool bOffPeak)
{
  static bool startup{ true };

  if (startup)
  {
    startup = false;
    return;  // reject the first datalogging which is incomplete !
  }

  if constexpr (RF_LOGGING_PRESENT)
  {
    send_rf_data(tx_data);  // *SEND RF DATA*
  }

  // The output is only started here, and written by SerialOutput::poll() from the
  // main loop. If the previous one is still in progress (it normally takes a small
  // part of a datalog period), this one is skipped.
  if (SerialOutput::busy())
  {
    return;
  }

  if constexpr (SERIAL_OUTPUT_TYPE == SerialOutputType::HumanReadable)
  {
    SerialOutput::start(printTextStep);
  }
  else if constexpr (SERIAL_OUTPUT_TYPE == SerialOutputType::IoT)
  {
    sendTelemetryData(bOffPeak);
    SerialOutput::start(writeTeleInfoStep);
  }
  else if constexpr (SERIAL_OUTPUT_TYPE == SerialOutputType::JSON)
  {
    datalogOffPeak = bOffPeak;
    SerialOutput::start(printJsonStep);
  }
}

/**
 * @brief Prints the load priorities to the Serial output.
 *
 * This function logs the current load priorities and states to the Serial output
 * at startup and after each priority rotation. It provides a detailed view of the load
 * configuration and their respective priorities.
 *
 * @details
 * - Each load's priority and state are printed in a human-readable format.
 * - Like every text message, it is only printed in human-readable output mode.
 *
 * @ingroup GeneralProcessing
 */
inline void logLoadPriorities()
{
  INFOLN(F("Load Priorities: "));
  for (const auto& loadPrioAndState : loadPrioritiesAndState)
  {
    INFO(F("\tload "));
    INFOLN(loadPrioAndState);
  }
}

/**
 * @brief Get the available RAM during setup.
 *
 * This function calculates the amount of free RAM available in the system.
 * It is useful for debugging and ensuring that the system has sufficient memory
 * for proper operation.
 *
 * @return int The amount of free RAM in bytes.
 *
 * @ingroup Debugging
 */
inline int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

#endif  // UTILS_H
