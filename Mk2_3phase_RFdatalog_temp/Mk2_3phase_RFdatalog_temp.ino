/**
 * @file Mk2_3phase_RFdatalog_temp.ino
 * @author Robin Emley (www.Mk2PVrouter.co.uk)
 * @author Frederic Metrich (frederic.metrich@live.fr)
 * @brief Mk2_3phase_RFdatalog_temp.ino - A photovoltaic energy diverter.
 * @date 2020-01-14
 *
 * @mainpage A 3-phase photovoltaic router/diverter
 *
 * @section description Description
 * Mk2_3phase_RFdatalog_temp.ino - Arduino program that maximizes the use of home photovoltaic production
 * by monitoring energy consumption and diverting power to one or more resistive charge(s) when needed.
 * In the absence of such a system, surplus energy flows away to the grid and is of no benefit to the PV-owner.
 *
 * @section history History
 * __Issue 1 was released in January 2015.__
 *
 * This sketch provides continuous monitoring of real power on three phases.
 * Surplus power is diverted to multiple loads in sequential order. A suitable
 * output-stage is required for each load; this can be either triac-based, or a
 * Solid State Relay.
 *
 * Datalogging of real power and Vrms is provided for each phase.
 * The presence or absence of the RFM12B needs to be set at compile time
 *
 * __January 2016, renamed as Mk2_3phase_RFdatalog_2 with these changes:__
 * - Improved control of multiple loads has been imported from the
 *     equivalent 1-phase sketch, Mk2_multiLoad_wired_6.ino
 * - the ISR has been upgraded to fix a possible timing anomaly
 * - variables to store ADC samples are now declared as "volatile"
 * - for RF69 RF module is now supported
 * - a performance check has been added with the result being sent to the Serial port
 * - control signals for loads are now active-high to suit the latest 3-phase PCB
 *
 * __February 2016, renamed as Mk2_3phase_RFdatalog_3 with these changes:__
 * - improvements to the start-up logic. The start of normal operation is now
 *    synchronized with the start of a new mains cycle.
 * - reduce the amount of feedback in the Low Pass Filter for removing the DC content
 *     from the SampleV stream. This resolves an anomaly which has been present since
 *     the start of this project. Although the amount of feedback has previously been
 *     excessive, this anomaly has had minimal effect on the system's overall behaviour.
 * - The reported power at each of the phases has been inverted. These values are now in
 *     line with the Open Energy Monitor convention, whereby import is positive and
 *     export is negative.
 *
 * __February 2020: updated to Mk2_3phase_RFdatalog_3a with these changes:__
 * - removal of some redundant code in the logic for determining the next load state.
 *
 *      Robin Emley
 *      www.Mk2PVrouter.co.uk
 *
 * __October 2019, renamed as Mk2_3phase_RFdatalog_temp with these changes:__
 * - This sketch has been restructured in order to make better use of the ISR.
 * - All of the time-critical code is now contained within the ISR and its helper functions.
 * - Values for datalogging are transferred to the main code using a flag-based handshake mechanism.
 * - The diversion of surplus power can no longer be affected by slower
 * activities which may be running in the main code such as Serial statements and RF.
 * - Temperature sensing is supported. A pullup resistor (4K7 or similar) is required for the Dallas sensor.
 * - The output mode, i.e. NORMAL or ANTI_FLICKER, is now set at compile time.
 * - Also:
 *   - The ADC is now in free-running mode, at ~104 µs per conversion.
 *   - a persistence check has been added for zero-crossing detection (polarityConfirmed)
 *   - a lowestNoOfSampleSetsPerMainsCycle check has been added, to detect any disturbances
 *   - Vrms has been added to the datalog payload (as Vrms x 100)
 *   - temperature has been added to the datalog payload (as degrees C x 100)
 *   - the phaseCal/f_voltageCal mechanism has been modified to be the same for all phases
 *   - RF capability made switchable so that the code will continue to run
 *     when an RF module is not fitted. Dataloging can then take place via the Serial port.
 *   - temperature capability made switchable so that the code will continue to run w/o sensor.
 *   - priority pin changed to handle peak/off-peak tariff
 *   - add rotating load priorities: this sketch is intended to control a 3-phase
 *     water heater which is composed of 3 independent heating elements wired in WYE
 *     with a neutral wire. The router will control each element in a specific order.
 *     To ensure that in average (over many days/months), each load runs the same time,
 *     each day, the router will rotate the priorities.
 *   - most functions have been splitted in one or more sub-functions. This way, each function
 *     has a specific task and is much smaller than before.
 *   - renaming of most of the variables with a single-letter prefix to identify its type.
 *   - direct port manipulation to save code size and speed-up performance
 *
 * __January 2020, changes:__
 * - This sketch has been again re-engineered. All 'defines' have been removed except
 *   the ones for compile-time optional functionalities.
 * - All constants have been replaced with constexpr initialized at compile-time
 * - all number-types have been replaced with fixed width number types
 * - old fashion enums replaced by scoped enums with fixed types
 * - off-peak tariff made switchable at compile-time
 * - rotation of load priorities made switchable at compile-time
 * - enhanced configuration for override specific loads during off-peak period
 *
 * __April 2020, changes:__
 * - Fix a bug in the load level calculation
 *
 * __May 2020, changes:__
 * - Fix a bug in the initialization of off-peak offsets
 * - added detailed configuration on start-up with build timestamp
 *
 * __June 2020, changes:__
 * - Add force pin for full power through overwrite switch
 * - Add priority rotation for single tariff
 *
 * __October 2020, changes:__
 * - Moving some part around (calibration values toward beginning of the sketch)
 * - renaming some preprocessor defines
 * - system/user specific data moved toward beginning of the sketch
 *
 * __January 2021, changes:__
 * - Further optimization
 * - now it's possible to specify the override period in minutes and hours
 * - initialization of runtime parameters for override period at compile-time
 *
 * __February 2021, changes:__
 * - Added temperature threshold for off-peak period
 *
 * __April 2021, renamed as Mk2_3phase_RFdatalog_temp with these changes:__
 * - since this sketch is under source control, no need to write the version in the name
 * - rename function 'checkLoadPrioritySelection' to function's job
 * - made forcePin presence configurable
 * - added WatchDog LED (blink 1s ON/ 1s OFF)
 * - code enhanced to support 6 loads
 *
 * __November 2021, changes:__
 * - heavy refactoring/restructuring of the sketch
 * - calibration values moved to the dedicated file 'calibration.h'
 * - user-specific values (pins, ...) are now in 'config.h' all other files should/must remain unchanged
 * - added support of temperature sensors (virtually no limit of sensors count)
 * - added support for emonESP (see https://github.com/openenergymonitor/EmonESP)
 *
 * @author Fred Metrich
 * @copyright Copyright (c) 2022
 *
 */
static_assert(__cplusplus >= 201703L, "**** Please define 'gnu++17' in 'platform.txt' ! ****");

#include <Arduino.h> // may not be needed, but it's probably a good idea to include this

#include "config.h"

// In this sketch, the ADC is free-running with a cycle time of ~104uS.

//--------------------------------------------------------------------------------------------------
#ifdef EMONESP
#undef SERIALPRINT // Must not corrupt serial output to emonHub with 'human-friendly' printout
#undef SERIALOUT
#endif

#ifdef SERIALOUT
#undef EMONESP
#undef SERIALPRINT // Must not corrupt serial output to emonHub with 'human-friendly' printout
#undef ENABLE_DEBUG
#endif
//--------------------------------------------------------------------------------------------------

#include "calibration.h"
#include "processing.h"
#include "types.h"
#include "utils.h"

// --------------  general global variables -----------------
//
// Some of these variables are used in multiple blocks so cannot be static.
// For integer maths, some variables need to be 'int32_t'
//

/**
 * @brief Interrupt Service Routine - Interrupt-Driven Analog Conversion.
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
 *            - Don't use delay ()
 *            - Don't do serial prints
 *            - Make variables shared with the main code volatile
 *            - Variables shared with main code may need to be protected by "critical sections"
 *            - Don't try to turn interrupts off or on
 *
 */
ISR(ADC_vect)
{
  static uint8_t sample_index{0};
  int16_t rawSample;

  switch (sample_index)
  {
  case 0:
    rawSample = ADC;                 // store the ADC value (this one is for Current L1)
    ADMUX = bit(REFS0) + sensorI[1]; // the conversion for V1 is already under way
    ++sample_index;                  // increment the control flag
    //
    processCurrentRawSample(0, rawSample);
    break;
  case 1:
    rawSample = ADC;                 // store the ADC value (this one is for Voltage L1)
    ADMUX = bit(REFS0) + sensorV[1]; // the conversion for I2 is already under way
    ++sample_index;                  // increment the control flag
    //
    processVoltageRawSample(0, rawSample);
    break;
  case 2:
    rawSample = ADC;                 // store the ADC value (this one is for Current L2)
    ADMUX = bit(REFS0) + sensorI[2]; // the conversion for V2 is already under way
    ++sample_index;                  // increment the control flag
    //
    processCurrentRawSample(1, rawSample);
    break;
  case 3:
    rawSample = ADC;                 // store the ADC value (this one is for Voltage L2)
    ADMUX = bit(REFS0) + sensorV[2]; // the conversion for I3 is already under way
    ++sample_index;                  // increment the control flag
    //
    processVoltageRawSample(1, rawSample);
    break;
  case 4:
    rawSample = ADC;                 // store the ADC value (this one is for Current L3)
    ADMUX = bit(REFS0) + sensorI[0]; // the conversion for V3 is already under way
    ++sample_index;                  // increment the control flag
    //
    processCurrentRawSample(2, rawSample);
    break;
  case 5:
    rawSample = ADC;                 // store the ADC value (this one is for Voltage L3)
    ADMUX = bit(REFS0) + sensorV[0]; // the conversion for I1 is already under way
    sample_index = 0;                // reset the control flag
    //
    processVoltageRawSample(2, rawSample);
    break;
  default:
    sample_index = 0; // to prevent lockup (should never get here)
  }
} // end of ISR

/**
 * @brief This function set all 3 loads to full power.
 *
 * @return true if loads are forced
 * @return false
 */
bool forceFullPower()
{
  if constexpr (OVERRIDE_PIN_PRESENT)
  {
    const uint8_t pinState{getPinState(forcePin)};

#ifdef ENABLE_DEBUG
    static uint8_t previousState{1};
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

void checkDiversionOnOff()
{
  if constexpr (DIVERSION_PIN_PRESENT)
  {
    const uint8_t pinState{getPinState(diversionPin)};

#ifdef ENABLE_DEBUG
    static uint8_t previousState{1};
    if (previousState != pinState)
    {
      DBUGLN(!pinState ? F("Trigger diversion OFF!") : F("End diversion OFF!"));
    }

    previousState = pinState;
#endif

    b_diversionOff = !pinState;
  }
}

void proceedRotation()
{
  b_reOrderLoads = true;

  // waits till the priorities have been rotated from inside the ISR
  while (b_reOrderLoads)
  {
    delay(10);
  }

  // prints the (new) load priorities
  logLoadPriorities();
}

bool proceedLoadPrioritiesAndOverridingDualTariff(const int16_t currentTemperature_x100)
{
  uint8_t i;
  static constexpr int16_t iTemperatureThreshold_x100{iTemperatureThreshold * 100};
  static uint8_t pinOffPeakState{HIGH};
  const uint8_t pinNewState{getPinState(offPeakForcePin)};

  if (pinOffPeakState && !pinNewState)
  {
    // we start off-peak period
    DBUGLN(F("Change to off-peak period!"));

    ul_TimeOffPeak = millis();

    if constexpr (PRIORITY_ROTATION)
    {
      proceedRotation();
    }
  }
  else
  {
    const auto ulElapsedTime{(uint32_t)(millis() - ul_TimeOffPeak)};
    const uint8_t pinState{getPinState(forcePin)};

    for (i = 0; i < NO_OF_DUMPLOADS; ++i)
    {
      // for each load, if we're inside off-peak period and within the 'force period', trigger the ISR to turn the load ON
      if (!pinOffPeakState && !pinNewState &&
          (ulElapsedTime >= rg_OffsetForce[i][0]) && (ulElapsedTime < rg_OffsetForce[i][1]))
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
bool proceedLoadPrioritiesAndOverriding(const int16_t currentTemperature_x100)
{
  if constexpr (DUAL_TARIFF)
  {
    return proceedLoadPrioritiesAndOverridingDualTariff(currentTemperature_x100);
  }
  else if constexpr (EMONESP_CONTROL)
  {
    static uint8_t pinRotationState{HIGH};
    const uint8_t pinNewState{getPinState(rotationPin)};

    if (pinRotationState && !pinNewState)
    {
      DBUGLN(F("Trigger rotation!"));

      proceedRotation();
    }
    pinRotationState = pinNewState;
  }
  else if constexpr (PRIORITY_ROTATION)
  {
    if (ROTATION_AFTER_CYCLES < absenceOfDivertedEnergyCount)
    {
      proceedRotation();

      absenceOfDivertedEnergyCount = 0;
    }
  }

  if constexpr (OVERRIDE_PIN_PRESENT)
  {
    const uint8_t pinState{getPinState(forcePin)};

    for (auto &bOverrideLoad : b_overrideLoadOn)
    {
      bOverrideLoad = !pinState;
    }
  }

  return false;
}

/**
 * @brief Called once during startup.
 * @details This function initializes a couple of variables we cannot init at compile time and
 *          sets a couple of parameters for runtime.
 *
 */
void setup()
{
  delay(initialDelay); // allows time to open the Serial Monitor

  DEBUG_PORT.begin(9600);
  Serial.begin(9600); // initialize Serial interface, Do NOT set greater than 9600

  // On start, always display config info in the serial monitor
  printConfiguration();

  // initializes all loads to OFF at startup
  initializeProcessing();

  initializeOptionalPins();

  logLoadPriorities();

#ifdef TEMP_SENSOR_PRESENT
  initTemperatureSensors();
#endif

  DBUG(F(">>free RAM = "));
  DBUGLN(freeRam()); // a useful value to keep an eye on
  DBUGLN(F("----"));
}

/**
 * @brief Main processor.
 * @details None of the workload in loop() is time-critical.
 *          All the processing of ADC data is done within the ISR.
 *
 */
void loop()
{
  static uint8_t perSecondTimer{0};
  static bool bOffPeak{false};
  static int16_t iTemperature_x100{0};

  if (b_newMainsCycle) // flag is set after every pair of ADC conversions
  {
    b_newMainsCycle = false; // reset the flag
    ++perSecondTimer;

    if (perSecondTimer >= SUPPLY_FREQUENCY)
    {
      perSecondTimer = 0;

      togglePin(watchDogPin);

      checkDiversionOnOff();

      if (!forceFullPower())
      {
        bOffPeak = proceedLoadPrioritiesAndOverriding(iTemperature_x100); // called every second
      }
    }
  }

  if (b_datalogEventPending)
  {
    b_datalogEventPending = false;

    tx_data.power = 0;
    for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase)
    {
      tx_data.power_L[phase] = copyOf_sumP_atSupplyPoint[phase] / copyOf_sampleSetsDuringThisDatalogPeriod * f_powerCal[phase];
      tx_data.power_L[phase] *= -1;

      tx_data.power += tx_data.power_L[phase];

      tx_data.Vrms_L_x100[phase] = (int32_t)(100 * f_voltageCal[phase] * sqrt(copyOf_sum_Vsquared[phase] / copyOf_sampleSetsDuringThisDatalogPeriod));
    }

#ifdef TEMP_SENSOR_PRESENT
    for (uint8_t idx = 0; idx < size(tx_data.temperature_x100); ++idx)
    {
      static int16_t tmp;
      tmp = readTemperature(sensorAddrs[idx]);

      // if read temperature is 85 and the delta with previous is greater than 5, skip the value
      if (8500 == tmp && abs(tmp - tx_data.temperature_x100[idx] > 500))
      {
        tmp = DEVICE_DISCONNECTED_RAW;
      }

      tx_data.temperature_x100[idx] = tmp;
    }
    requestTemperatures(); // for use next time around
#endif

    sendResults(bOffPeak);
  }
} // end of loop()
