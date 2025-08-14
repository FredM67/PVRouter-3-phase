/**
 * @file main.cpp
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Main code for the PVRouter 3-phase project.
 *
 * This file contains the main logic for the PVRouter, including the setup and loop functions,
 * as well as interrupt service routines (ISRs) and utility functions for managing load priorities,
 * temperature sensing, and telemetry data.
 *
 * @details
 * - **Interrupt-Driven Analog Conversion**: Uses an ISR to process ADC data for voltage and current sensors.
 * - **Load Management**: Includes functions for load priority rotation, forced full power, and dual tariff handling.
 * - **Telemetry**: Sends telemetry data in various formats (e.g., IoT, JSON).
 * - **Temperature Sensing**: Supports DS18B20 sensors for temperature monitoring.
 * - **Watchdog**: Toggles a pin to indicate system activity.
 *
 * @version 0.1
 * @date 2023-02-15
 *
 * @copyright Copyright (c) 2023
 *
 */

static_assert(__cplusplus >= 201703L, "**** Please define 'gnu++17' in 'platform.txt' ! ****");
static_assert(__cplusplus >= 201703L, "See also : https://github.com/FredM67/PVRouter-3-phase/blob/main/Mk2_3phase_RFdatalog_temp/Readme.md");

#include <Arduino.h>  // may not be needed, but it's probably a good idea to include this

#include "config.h"

// In this sketch, the ADC is free-running with a cycle time of ~104uS.

#include "calibration.h"
#include "processing.h"
#include "shared_var.h"
#include "types.h"
#include "utils.h"
#include "utils_relay.h"
#include "validation.h"
#include "main.h"

// --------------  general global variables -----------------
//
// Some of these variables are used in multiple blocks so cannot be static.
// For integer maths, some variables need to be 'int32_t'
//

/**
 * @brief Forces all loads to full power if the override pin is active.
 *
 * This function checks the state of the override pin and forces all loads to full power
 * if the pin is active. It also logs changes in the override state for debugging purposes
 * if enabled.
 *
 * @details
 * - If the override pin is LOW, all loads are forced to full power.
 * - If the override pin is HIGH, normal load behavior is restored.
 * - Debug messages are printed when the override state changes (if debugging is enabled).
 *
 * @return true if loads are forced to full power, false otherwise.
 *
 * @ingroup GeneralProcessing
 */
bool forceFullPower()
{
  if constexpr (OVERRIDE_PIN_PRESENT)
  {
    const auto pinState{ getPinState(forcePin) };

#ifdef ENABLE_DEBUG
    static uint8_t previousState{ HIGH };
    if (previousState != pinState)
    {
      DBUGLN(!pinState ? F("Trigger override!") : F("End override!"));
    }

    previousState = pinState;
#endif

    for (auto &bOverrideLoad : Shared::b_overrideLoadOn)
    {
      bOverrideLoad = !pinState;
    }

    return !pinState;
  }
  else
  {
    return false;
  }
}

/**
 * @brief Checks and updates the diversion state based on the diversion pin.
 *
 * This function monitors the state of the diversion pin and updates the global
 * `b_diversionOff` flag accordingly. It also logs changes in the diversion state
 * for debugging purposes if enabled.
 *
 * @details
 * - If the diversion pin is LOW, the diversion is considered OFF.
 * - If the diversion pin is HIGH, the diversion is considered ON.
 * - Debug messages are printed when the diversion state changes (if debugging is enabled).
 *
 * @ingroup GeneralProcessing
 */
void checkDiversionOnOff()
{
  if constexpr (DIVERSION_PIN_PRESENT)
  {
    const auto pinState{ getPinState(diversionPin) };

#ifdef ENABLE_DEBUG
    static auto previousState{ HIGH };
    if (previousState != pinState)
    {
      DBUGLN(!pinState ? F("Trigger diversion OFF!") : F("End diversion OFF!"));
    }

    previousState = pinState;
#endif

    Shared::b_diversionEnabled = pinState;
  }
}

/**
 * @brief Proceeds with load priority rotation.
 *
 * This function triggers the rotation of load priorities and waits until the rotation
 * is completed. It ensures that the new load priorities are logged after the rotation.
 *
 * @details
 * - Sets the `b_reOrderLoads` flag to initiate the rotation process.
 * - Waits in a loop until the rotation is completed by the ISR.
 * - Logs the updated load priorities after the rotation.
 *
 * @ingroup GeneralProcessing
 */
void proceedRotation()
{
  Shared::b_reOrderLoads = true;

  // waits till the priorities have been rotated from inside the ISR
  do
  {
    delay(10);
  } while (Shared::b_reOrderLoads);

  // prints the (new) load priorities
  logLoadPriorities();
}

/**
 * @brief Handles load priorities and overriding during dual tariff periods.
 *
 * This function manages load priorities and overriding logic based on the dual tariff state
 * and the current temperature. It ensures proper load behavior during off-peak and on-peak periods.
 *
 * @param currentTemperature_x100 Current temperature multiplied by 100 (default to 0 if deactivated).
 * @return true if the system is in a high tariff (on-peak) period.
 * @return false if the system is in a low tariff (off-peak) period.
 *
 * @details
 * - Detects transitions between off-peak and on-peak periods using the dual tariff pin.
 * - During off-peak periods, manages load priorities and overrides based on elapsed time and temperature.
 * - Logs transitions between tariff periods for debugging purposes.
 *
 * @ingroup GeneralProcessing
 */
bool proceedLoadPrioritiesAndOverridingDualTariff(const int16_t &currentTemperature_x100)
{
  constexpr int16_t iTemperatureThreshold_x100{ iTemperatureThreshold * 100 };
  static bool pinOffPeakState{ HIGH };
  const auto pinNewState{ getPinState(dualTariffPin) };

  if (pinOffPeakState && !pinNewState)
  {
    // we start off-peak period
    DBUGLN(F("Change to off-peak period!"));

    ul_TimeOffPeak = millis();

    if constexpr (PRIORITY_ROTATION == RotationModes::AUTO)
    {
      proceedRotation();
    }
  }
  else
  {
    const auto ulElapsedTime{ static_cast< uint32_t >(millis() - ul_TimeOffPeak) };
    const auto pinState{ OVERRIDE_PIN_PRESENT ? getPinState(forcePin) : HIGH };

    for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
    {
      // for each load, if we're inside off-peak period and within the 'force period', trigger the ISR to turn the load ON
      if (!pinOffPeakState && !pinNewState && (ulElapsedTime >= rg_OffsetForce[i][0]) && (ulElapsedTime < rg_OffsetForce[i][1]))
      {
        Shared::b_overrideLoadOn[i] = !pinState || (currentTemperature_x100 <= iTemperatureThreshold_x100);
      }
      else
      {
        Shared::b_overrideLoadOn[i] = !pinState;
      }
    }
  }
  // end of off-peak period
  if (!pinOffPeakState && pinNewState)
  {
    DBUGLN(F("Change to peak period!"));
  }

  pinOffPeakState = pinNewState;

  return (LOW == pinOffPeakState);
}

/**
 * @brief Handles load priorities and overriding logic.
 *
 * This function manages load priorities and overriding behavior based on the system configuration.
 * It supports dual tariff handling, priority rotation, and manual override functionality.
 *
 * @param currentTemperature_x100 Current temperature multiplied by 100 (default to 0 if deactivated).
 * @return true if the system is in a high tariff (on-peak) period.
 * @return false if the system is in a low tariff (off-peak) period.
 *
 * @details
 * - If dual tariff is enabled, it delegates to `proceedLoadPrioritiesAndOverridingDualTariff`.
 * - If EmonESP control is enabled, it handles load rotation based on the rotation pin state.
 * - If priority rotation is set to auto, it rotates priorities after a defined period of inactivity.
 * - If the override pin is present, it forces all loads to full power when activated.
 *
 * @ingroup GeneralProcessing
 */
bool proceedLoadPrioritiesAndOverriding(const int16_t &currentTemperature_x100)
{
  if constexpr (DUAL_TARIFF)
  {
    return proceedLoadPrioritiesAndOverridingDualTariff(currentTemperature_x100);
  }

  if constexpr ((PRIORITY_ROTATION == RotationModes::PIN) || (EMONESP_CONTROL))
  {
    static uint8_t pinRotationState{ HIGH };
    const auto pinNewState{ getPinState(rotationPin) };

    if (pinRotationState && !pinNewState)
    {
      DBUGLN(F("Trigger rotation!"));

      proceedRotation();
    }
    pinRotationState = pinNewState;
  }
  else if constexpr (PRIORITY_ROTATION == RotationModes::AUTO)
  {
    if (ROTATION_AFTER_SECONDS < Shared::absenceOfDivertedEnergyCountInSeconds)
    {
      proceedRotation();

      Shared::absenceOfDivertedEnergyCountInSeconds = 0;
    }
  }

  if constexpr (OVERRIDE_PIN_PRESENT)
  {
    const auto pinState{ getPinState(forcePin) };

    for (auto &bOverrideLoad : Shared::b_overrideLoadOn)
    {
      bOverrideLoad = !pinState;
    }
  }

  return false;
}

/**
 * @brief Called once during startup.
 *
 * This function initializes the system, configures pins, and prints system configuration
 * to the Serial Monitor. It also initializes optional features like temperature sensing
 * and load priorities.
 *
 * @details
 * - Delays startup to allow time to open the Serial Monitor.
 * - Initializes the Serial interface and debug port.
 * - Displays configuration information.
 * - Initializes all loads to OFF at startup.
 * - Logs load priorities and initializes temperature sensors if present.
 * - Prints available free RAM for debugging purposes.
 * 
 * @ingroup Initialization
 */
void setup()
{
  delay(initialDelay);  // allows time to open the Serial Monitor

  DEBUG_PORT.begin(9600);
  Serial.begin(9600, SERIAL_OUTPUT_TYPE == SerialOutputType::IoT ? SERIAL_7E1 : SERIAL_8N1);  // initialize Serial interface, Do NOT set greater than 9600

  // On start, always display config info in the serial monitor
  printConfiguration();

  // initializes all loads to OFF at startup
  initializeProcessing();

  logLoadPriorities();

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    temperatureSensing.initTemperatureSensors();
  }

  DBUG(F(">>free RAM = "));
  DBUGLN(freeRam());  // a useful value to keep an eye on
  DBUGLN(F("----"));
}

/**
 * @brief Updates power and voltage data for all phases.
 *
 * This function calculates the power and voltage for each phase based on the
 * accumulated data during the datalogging period. It also updates the total power.
 *
 * @details
 * - Computes the power for each phase using the accumulated power data and calibration factors.
 * - Calculates the RMS voltage for each phase using the accumulated voltage squared data.
 * - Updates the total power by summing the power of all phases.
 *
 * @ingroup GeneralProcessing
 */
void updatePowerAndVoltageData()
{
  tx_data.power = 0;
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    tx_data.power_L[phase] = Shared::copyOf_sumP_atSupplyPoint[phase] / Shared::copyOf_sampleSetsDuringThisDatalogPeriod * f_powerCal[phase];
    tx_data.power_L[phase] *= -1;

    tx_data.power += tx_data.power_L[phase];

    if constexpr (DATALOG_PERIOD_IN_SECONDS > 10)
    {
      tx_data.Vrms_L_x100[phase] = static_cast< uint32_t >((100U << 2) * f_voltageCal[phase] * sqrt(Shared::copyOf_sum_Vsquared[phase] / Shared::copyOf_sampleSetsDuringThisDatalogPeriod));
    }
    else
    {
      tx_data.Vrms_L_x100[phase] = static_cast< uint32_t >(100U * f_voltageCal[phase] * sqrt(Shared::copyOf_sum_Vsquared[phase] / Shared::copyOf_sampleSetsDuringThisDatalogPeriod));
    }
  }
}

/**
 * @brief Processes temperature data from DS18B20 sensors.
 * 
 * @details This function reads temperature values from all connected DS18B20 sensors, filters out invalid readings, 
 *          and updates the telemetry data structure with valid temperature values. Invalid readings are identified 
 *          as 85.00°C (encoded as 8500) with a delta greater than 5.00°C (encoded as 500) from the previous reading. 
 *          After processing, it requests new temperature measurements for the next cycle.
 * 
 * @note This function assumes that temperature values are stored as integers multiplied by 100 for precision.
 * 
 * @pre The `temperatureSensing` object must be initialized and configured with the connected DS18B20 sensors.
 * 
 * @post The `tx_data.temperature_x100` array is updated with the latest valid temperature readings.
 * 
 * @ingroup TemperatureProcessing
 */
void processTemperatureData()
{
  uint8_t idx{ temperatureSensing.get_size() };
  do
  {
    auto tmp = temperatureSensing.readTemperature(--idx);

    // if read temperature is 85 and the delta with previous is greater than 5, skip the value
    if (8500 == tmp && (abs(tmp - tx_data.temperature_x100[idx]) > 500))
    {
      tmp = DEVICE_DISCONNECTED_RAW;
    }

    tx_data.temperature_x100[idx] = tmp;
  } while (idx);

  temperatureSensing.requestTemperatures();  // for use next time around
}

/**
 * @brief Handles tasks that need to be executed every second.
 *
 * This function performs various tasks that are triggered once per second, ensuring
 * proper system operation and load management.
 *
 * @details
 * - Increments the absence of diverted energy count if no energy is being diverted.
 * - Toggles the watchdog pin if the feature is enabled.
 * - Checks and updates the diversion state.
 * - Manages load priorities and overriding based on the current temperature.
 * - Updates relay durations and proceeds with relay state transitions if relay diversion is enabled.
 *
 * @param bOffPeak Reference to the off-peak state flag.
 * @param iTemperature_x100 Current temperature multiplied by 100 (default to 0 if temperature sensing is disabled).
 *
 * @ingroup GeneralProcessing
 */
void handlePerSecondTasks(bool &bOffPeak, int16_t &iTemperature_x100)
{
  if constexpr (WATCHDOG_PIN_PRESENT)
  {
    togglePin(watchDogPin);
  }

  checkDiversionOnOff();

  if (!forceFullPower())
  {
    bOffPeak = proceedLoadPrioritiesAndOverriding(iTemperature_x100);  // called every second
  }

  if constexpr (RELAY_DIVERSION)
  {
    relays.inc_duration();
    relays.proceed_relays();
  }
}

/**
 * @brief Main processor loop.
 *
 * This function handles non-time-critical tasks such as load management, telemetry updates,
 * and temperature sensing. It processes ADC data through flags set by the ISR and ensures
 * proper system operation.
 *
 * @details
 * - Executes tasks triggered by the `b_newMainsCycle` flag, which is set after every pair of ADC conversions.
 * - Handles per-second tasks such as load priority management and diversion state updates.
 * - Processes data logging events and updates power, voltage, and temperature data.
 * - Sends telemetry results and updates relay states if relay diversion is enabled.
 *
 * @ingroup GeneralProcessing
 */
void loop()
{
  static uint8_t perSecondTimer{ 0 };
  static bool bOffPeak{ false };
  static int16_t iTemperature_x100{ 0 };

  if (Shared::b_newMainsCycle)  // flag is set after every pair of ADC conversions
  {
    Shared::b_newMainsCycle = false;  // reset the flag
    ++perSecondTimer;

    if (perSecondTimer >= SUPPLY_FREQUENCY)
    {
      perSecondTimer = 0;
      handlePerSecondTasks(bOffPeak, iTemperature_x100);
    }
  }

  if (Shared::b_datalogEventPending)
  {
    Shared::b_datalogEventPending = false;

    updatePowerAndVoltageData();

    if constexpr (RELAY_DIVERSION)
    {
      relays.update_average(tx_data.power);
    }

    if constexpr (TEMP_SENSOR_PRESENT)
    {
      processTemperatureData();
    }

    sendResults(bOffPeak);
  }
}  // end of loop()
