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
 * @brief Calculates the dual tariff forcing bitmask based on current conditions.
 *
 * This function determines which loads should be forced ON during off-peak periods
 * based on elapsed time since off-peak start, configured time windows, and temperature conditions.
 *
 * @param currentTemperature_x100 Current temperature multiplied by 100.
 * @return Bitmask of loads that should be forced ON due to dual tariff conditions, 0 if none.
 *
 * @details
 * - Only applies during off-peak periods (both pins must be LOW).
 * - Each load has its own time window defined in rg_OffsetForce.
 * - Temperature must be at or below the threshold for forcing to activate.
 * - Uses static variables to track dual tariff pin state changes.
 *
 * @ingroup DualTariff
 */
uint16_t getDualTariffForcingBitmask(const int16_t currentTemperature_x100)
{
  if constexpr (!DUAL_TARIFF)
  {
    return 0;
  }

  constexpr int16_t iTemperatureThreshold_x100{ iTemperatureThreshold * 100 };
  static bool pinOffPeakState{ HIGH };
  const auto pinNewState{ getPinState(dualTariffPin) };

  // Return early if we're not in off-peak period
  if (pinOffPeakState || pinNewState)
  {
    return 0;
  }

  // We're in off-peak period - check forcing time windows
  const auto ulElapsedTime{ static_cast< uint32_t >(millis() - ul_TimeOffPeak) };

  uint16_t forcingBitmask = 0;
  uint8_t i{ NO_OF_DUMPLOADS };
  do
  {
    --i;
    // Skip if not within the 'force period'
    if ((ulElapsedTime < rg_OffsetForce[i][0]) || (ulElapsedTime >= rg_OffsetForce[i][1]))
    {
      continue;
    }

    // Force load ON if temperature condition is met
    if (currentTemperature_x100 <= iTemperatureThreshold_x100)
    {
      bit_set(forcingBitmask, physicalLoadPin[i]);
    }
  } while (i);

  return forcingBitmask;
}

/**
 * @brief Gets the combined bitmask of all active override pins and dual tariff forcing.
 *
 * This function calculates the complete override bitmask by combining external override pins
 * with dual tariff automatic forcing. The bitwise OR operation automatically handles precedence
 * where external overrides take priority over dual tariff forcing for the same pins.
 *
 * @param currentTemperature_x100 Current temperature multiplied by 100 (used for dual tariff logic).
 * @return Combined bitmask of all active overrides (external pins + dual tariff), 0 if no overrides are active.
 *
 * @details
 * - First checks all configured external override pins and sets corresponding bits.
 * - Then applies dual tariff forcing using bitwise OR operation.
 * - OR operation ensures external overrides take precedence (1|x = 1).
 * - For pins without external overrides (0|x = x), dual tariff forcing is applied.
 * - Function is called atomically to prevent race conditions with ISR.
 *
 * @ingroup GeneralProcessing
 */
uint16_t getOverrideBitmask(const int16_t currentTemperature_x100)
{
  uint16_t overrideBitmask = 0;

  // Add external override pins
  if constexpr (OVERRIDE_PIN_PRESENT)
  {
    // Check each configured override pin and combine their bitmasks
    uint8_t i{ overridePins.size() };
    do
    {
      --i;
      const uint8_t pin = overridePins.getPin(i);
      const auto pinState = getPinState(pin);

      if (!pinState)  // Pin is LOW (active)
      {
        overrideBitmask |= overridePins.getBitmask(i);
      }
    } while (i);
  }

  // Add dual tariff forcing - OR operation handles precedence automatically
  // If a bit is already set by external override, OR won't change it
  // If a bit is not set, OR will apply dual tariff forcing
  overrideBitmask |= getDualTariffForcingBitmask(currentTemperature_x100);

  return overrideBitmask;
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
 * @brief Handles dual tariff state transitions and priority rotation during off-peak periods.
 *
 * This function manages dual tariff state detection and triggers priority rotation
 * when transitioning to off-peak periods. The actual load forcing is handled in getOverrideBitmask().
 *
 * @return true if the system is in a high tariff (on-peak) period.
 * @return false if the system is in a low tariff (off-peak) period.
 *
 * @details
 * - Detects transitions between off-peak and on-peak periods using the dual tariff pin.
 * - Triggers automatic priority rotation when entering off-peak period.
 * - Logs transitions between tariff periods for debugging purposes.
 * - Load forcing logic (including temperature conditions) is handled atomically in getOverrideBitmask().
 *
 * @ingroup GeneralProcessing
 */
bool proceedDualTariffLogic()
{
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

  // end of off-peak period
  if (!pinOffPeakState && pinNewState)
  {
    DBUGLN(F("Change to peak period!"));
  }

  pinOffPeakState = pinNewState;

  return (LOW == pinOffPeakState);
}

/**
 * @brief Handles load priority rotation and dual tariff state transitions.
 *
 * This function manages load priority rotation behavior and dual tariff state detection
 * based on the system configuration. It supports priority rotation via pin control, 
 * EmonESP control, or automatic rotation. Override logic is handled in getOverrideBitmask().
 *
 * @param currentTemperature_x100 Current temperature multiplied by 100 (default to 0 if deactivated).
 * @return true if the system is in a high tariff (on-peak) period (only for dual tariff mode).
 * @return false if the system is in a low tariff (off-peak) period or not in dual tariff mode.
 *
 * @details
 * - If dual tariff is enabled, it delegates to `proceedDualTariffLogic` for state transitions.
 * - If EmonESP control is enabled, it handles load rotation based on the rotation pin state.
 * - If priority rotation is set to auto, it rotates priorities after a defined period of inactivity.
 * - Override logic (external pins + dual tariff forcing) is handled atomically in getOverrideBitmask().
 *
 * @ingroup GeneralProcessing
 */
bool proceedLoadPriorities(const int16_t &currentTemperature_x100)
{
  // Suppress unused parameter warning when DUAL_TARIFF is disabled
  (void)currentTemperature_x100;

  if constexpr (DUAL_TARIFF)
  {
    return proceedDualTariffLogic();
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
  uint8_t phase{ NO_OF_PHASES };
  do
  {
    --phase;
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
  } while (phase);
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
  uint8_t idx{ temperatureSensing.size() };
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

  // Get complete override bitmask atomically (external pins + dual tariff forcing)
  uint16_t privateOverrideBitmask = getOverrideBitmask(iTemperature_x100);
  
  if constexpr (RELAY_DIVERSION)
  {
    relays.inc_duration();
    // Pass private bitmask to relay engine, it will filter out relay pins that can be controlled
    relays.proceed_relays(privateOverrideBitmask);
  }
  
  // Copy the filtered bitmask (only triac/load pins) to shared version
  Shared::overrideBitmask = privateOverrideBitmask;

  // Only process priority logic if no override pins are active
  if (!Shared::overrideBitmask)
  {
    bOffPeak = proceedLoadPriorities(iTemperature_x100);
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
