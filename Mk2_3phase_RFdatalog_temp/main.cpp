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
 * - **Telemetry**: Sends telemetry data in various formats (e.g., IoT, EmonCMS).
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
 * @brief Interrupt Service Routine - Interrupt-Driven Analog Conversion.
 * 
 * @details An Interrupt Service Routine is now defined which instructs the ADC to perform a conversion
 *          for each of the voltage and current sensors in turn.
 *
 *          This Interrupt Service Routine is for use when the ADC is in the free-running mode.
 *          It is executed whenever an ADC conversion has finished, approx every 104 µs. In
 *          free-running mode, the ADC has already started its next conversion by the time that
 *          the ISR is executed. The ISR therefore needs to "look ahead".
 *
 *          At the end of conversion Type N, conversion Type N+1 will start automatically. The ISR
 *          which runs at this point therefore needs to capture the results of conversion Type N,
 *          and set up the conditions for conversion Type N+2, and so on.
 *
 *          By means of various helper functions, all of the time-critical activities are processed
 *          within the ISR.
 *
 *          The main code is notified by means of a flag when fresh copies of loggable data are available.
 *
 *          Keep in mind, when writing an Interrupt Service Routine (ISR):
 *            - Keep it short
 *            - Don't use delay()
 *            - Don't do serial prints
 *            - Make variables shared with the main code volatile
 *            - Variables shared with main code may need to be protected by "critical sections"
 *            - Don't try to turn interrupts off or on
 *
 * @ingroup TimeCritical
 */
ISR(ADC_vect)
{
  static uint8_t sample_index{ 0 };
  int16_t rawSample;

  switch (sample_index)
  {
    case 0:
      rawSample = ADC;                  // store the ADC value (this one is for Voltage L1)
      ADMUX = bit(REFS0) + sensorV[1];  // the conversion for I1 is already under way
      ++sample_index;                   // increment the control flag
      //
      processVoltageRawSample(0, rawSample);
      break;
    case 1:
      rawSample = ADC;                  // store the ADC value (this one is for Current L1)
      ADMUX = bit(REFS0) + sensorI[1];  // the conversion for V2 is already under way
      ++sample_index;                   // increment the control flag
      //
      processCurrentRawSample(0, rawSample);
      break;
    case 2:
      rawSample = ADC;                  // store the ADC value (this one is for Voltage L2)
      ADMUX = bit(REFS0) + sensorV[2];  // the conversion for I2 is already under way
      ++sample_index;                   // increment the control flag
      //
      processVoltageRawSample(1, rawSample);
      break;
    case 3:
      rawSample = ADC;                  // store the ADC value (this one is for Current L2)
      ADMUX = bit(REFS0) + sensorI[2];  // the conversion for V3 is already under way
      ++sample_index;                   // increment the control flag
      //
      processCurrentRawSample(1, rawSample);
      break;
    case 4:
      rawSample = ADC;                  // store the ADC value (this one is for Voltage L3)
      ADMUX = bit(REFS0) + sensorV[0];  // the conversion for I3 is already under way
      ++sample_index;                   // increment the control flag
      //
      processVoltageRawSample(2, rawSample);
      break;
    case 5:
      rawSample = ADC;                  // store the ADC value (this one is for Current L3)
      ADMUX = bit(REFS0) + sensorI[0];  // the conversion for V1 is already under way
      sample_index = 0;                 // reset the control flag
      //
      processCurrentRawSample(2, rawSample);
      break;
    default:
      sample_index = 0;  // to prevent lockup (should never get here)
  }
}  // end of ISR

/**
 * @brief This function set all 3 loads to full power.
 *
 * @return true if loads are forced, false otherwise.
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

    for (auto &bOverrideLoad : b_overrideLoadOn)
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

    b_diversionOff = !pinState;
  }
}

/**
 * @brief Proceed load priority rotation
 * 
 */
void proceedRotation()
{
  b_reOrderLoads = true;

  // waits till the priorities have been rotated from inside the ISR
  do
  {
    delay(10);
  } while (b_reOrderLoads);

  // prints the (new) load priorities
  logLoadPriorities();
}

/**
 * @brief Proceed load priority in combination with dual tariff
 * 
 * @param currentTemperature_x100 current temperature x 100 (default to 0 if deactivated)
 * @return true if high tariff (on-peak period)
 * @return false if low tariff (off-peak period)
 */
bool proceedLoadPrioritiesAndOverridingDualTariff(const int16_t& currentTemperature_x100)
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
    const auto pinState{ getPinState(forcePin) };

    for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
    {
      // for each load, if we're inside off-peak period and within the 'force period', trigger the ISR to turn the load ON
      if (!pinOffPeakState && !pinNewState && (ulElapsedTime >= rg_OffsetForce[i][0]) && (ulElapsedTime < rg_OffsetForce[i][1]))
      {
        b_overrideLoadOn[i] = !pinState || (currentTemperature_x100 <= iTemperatureThreshold_x100);
      }
      else
      {
        b_overrideLoadOn[i] = !pinState;
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
 * @brief This function changes the value of the load priorities.
 * @details Since we don't have access to a clock, we detect the offPeak start from the main energy meter.
 *          Additionally, when off-peak period starts, we rotate the load priorities for the next day.
 *
 * @param currentTemperature_x100 current temperature x 100 (default to 0 if deactivated)
 * @return true if off-peak tariff is active
 * @return false if on-peak tariff is active
 */
bool proceedLoadPrioritiesAndOverriding(const int16_t& currentTemperature_x100)
{
  if constexpr (DUAL_TARIFF)
  {
    return proceedLoadPrioritiesAndOverridingDualTariff(currentTemperature_x100);
  }

  if constexpr (EMONESP_CONTROL)
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
    if (ROTATION_AFTER_SECONDS < absenceOfDivertedEnergyCount)
    {
      proceedRotation();

      absenceOfDivertedEnergyCount = 0;
    }
  }

  if constexpr (OVERRIDE_PIN_PRESENT)
  {
    const auto pinState{ getPinState(forcePin) };

    for (auto &bOverrideLoad : b_overrideLoadOn)
    {
      bOverrideLoad = !pinState;
    }
  }

  return false;
}

/**
 * @brief Called once during startup.
 * @details Initializes variables, configures pins, and prints system configuration to the Serial Monitor.
 *          Also initializes optional features like temperature sensing and load priorities.
 */
void setup()
{
  delay(initialDelay);  // allows time to open the Serial Monitor

  DEBUG_PORT.begin(9600);
  Serial.begin(9600);  // initialize Serial interface, Do NOT set greater than 9600

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
 * @details This function calculates the power and voltage for each phase based on the
 *          accumulated data during the datalogging period. It also updates the total power.
 */
void updatePowerAndVoltageData()
{
  tx_data.power = 0;
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    tx_data.power_L[phase] = copyOf_sumP_atSupplyPoint[phase] / copyOf_sampleSetsDuringThisDatalogPeriod * f_powerCal[phase];
    tx_data.power_L[phase] *= -1;

    tx_data.power += tx_data.power_L[phase];

    if constexpr (DATALOG_PERIOD_IN_SECONDS > 10)
    {
      tx_data.Vrms_L_x100[phase] = static_cast< int32_t >((100 << 2) * f_voltageCal[phase] * sqrt(copyOf_sum_Vsquared[phase] / copyOf_sampleSetsDuringThisDatalogPeriod));
    }
    else
    {
      tx_data.Vrms_L_x100[phase] = static_cast< int32_t >(100 * f_voltageCal[phase] * sqrt(copyOf_sum_Vsquared[phase] / copyOf_sampleSetsDuringThisDatalogPeriod));
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
 * @details This function performs various tasks that are triggered once per second, including:
 * - Incrementing the absence of diverted energy count if no energy is being diverted.
 * - Toggling the watchdog pin if the feature is enabled.
 * - Checking and updating the diversion state.
 * - Managing load priorities and overriding based on the current temperature.
 * - Updating relay durations and proceeding with relay state transitions if relay diversion is enabled.
 *
 * @param bOffPeak Reference to the off-peak state flag.
 * @param iTemperature_x100 Current temperature multiplied by 100 (default to 0 if temperature sensing is disabled).
 */
void handlePerSecondTasks(bool &bOffPeak, int16_t &iTemperature_x100)
{
  if (EDD_isIdle)
  {
    ++absenceOfDivertedEnergyCount;
  }

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
 * @brief Main processor.
 * @details Handles non-time-critical tasks such as load management, telemetry updates, and temperature sensing.
 *          Processes ADC data through flags set by the ISR.
 */
void loop()
{
  static uint8_t perSecondTimer{ 0 };
  static bool bOffPeak{ false };
  static int16_t iTemperature_x100{ 0 };

  if (b_newMainsCycle)  // flag is set after every pair of ADC conversions
  {
    b_newMainsCycle = false;  // reset the flag
    ++perSecondTimer;

    if (perSecondTimer >= SUPPLY_FREQUENCY)
    {
      perSecondTimer = 0;
      handlePerSecondTasks(bOffPeak, iTemperature_x100);
    }
  }

  if (b_datalogEventPending)
  {
    b_datalogEventPending = false;

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
