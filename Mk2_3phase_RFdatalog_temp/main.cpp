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
 * @date 2026-11-23
 *
 * @copyright Copyright (c) 2023-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

static_assert(__cplusplus >= 201703L, "**** Please define 'gnu++17' in 'platform.txt' ! ****");
static_assert(__cplusplus >= 201703L, "See also : https://github.com/FredM67/PVRouter-3-phase/blob/main/Mk2_3phase_RFdatalog_temp/Readme.md");

#include <Arduino.h>  // may not be needed, but it's probably a good idea to include this

#include "config_derived.h"

// In this sketch, the ADC is free-running with a cycle time of ~104uS.

#include "calibration.h"
#include "datalog_math.h"
#include "pin_edges.h"
#include "processing.h"
#include "shared_var.h"
#include "types.h"
#include "utils.h"
#include "utils_relay.h"
#include "validation.h"
#include "main.h"
#include "override_masks.h"
#include "temp_schedule.h"

// --------------  general global variables -----------------
//
// Some of these variables are used in multiple blocks so cannot be static.
// For integer maths, some variables need to be 'int32_t'
//

/** @brief The round of the temperature sensors: one 1-Wire transaction per mains cycle (temp_schedule.h). */
TempSchedule::Scheduler< tempSensors.size(), TempSchedule::conversionCycles(SUPPLY_FREQUENCY) > tempSchedule;

/** @brief The last reading of each sensor, in hundredths of °C, as endRead() returned it (none yet: setup()). */
int16_t temperatureReadings[tempSensors.size()];

/**
 * @brief Calculates the dual tariff forcing bitmasks based on current conditions.
 *
 * This function determines which loads and relays should be forced ON during off-peak periods
 * based on elapsed time since off-peak start, the scheduled boosts, and temperature conditions.
 *
 * @param bOffPeak true if the system is in an off-peak period (as tracked by proceedDualTariffLogic()).
 * @param currentTemperature_x100 Current temperature multiplied by 100.
 * @return Bitmasks of loads and relays that should be forced ON due to dual tariff conditions, 0 if none.
 *
 * @details
 * - The logic is Forcing::boostForcing() (override_masks.h); this wrapper reads millis() and the tables.
 * - Only applies during off-peak periods.
 * - Each scheduled boost has its own window (scheduledBoosts), resolved at compile time.
 * - Temperature must be at or below the threshold for forcing to activate.
 * - Relies on proceedDualTariffLogic() having been called first, so that ul_TimeOffPeak is up to date.
 *
 * @ingroup DualTariff
 */
OverrideMasks getDualTariffForcingBitmask(const bool bOffPeak, const int16_t currentTemperature_x100)
{
  if constexpr (!DUAL_TARIFF)
  {
    return { 0, 0 };
  }

  constexpr int16_t iTemperatureThreshold_x100{ iTemperatureThreshold * 100 };

  return Forcing::boostForcing(bOffPeak, static_cast< uint32_t >(millis() - ul_TimeOffPeak), currentTemperature_x100,
                               iTemperatureThreshold_x100, NO_OF_BOOSTS, readBoost);
}

/**
 * @brief Gets the combined bitmasks of all active override pins and dual tariff forcing.
 *
 * This function calculates the complete override bitmasks (local and remote) by combining
 * external override pins with dual tariff automatic forcing. The bitwise OR operation
 * automatically handles precedence where external overrides take priority over dual tariff
 * forcing for the same pins.
 *
 * @param bOffPeak true if the system is in an off-peak period (used for dual tariff logic).
 * @param currentTemperature_x100 Current temperature multiplied by 100 (used for dual tariff logic).
 * @return OverrideMasks struct containing both local (physical pins) and remote (virtual pins) bitmasks.
 *
 * @details
 * - The logic is Forcing::pinForcing() (override_masks.h); this wrapper reads the pins.
 * - First checks all configured external override pins (active LOW) and sets corresponding bits.
 * - Local bitmask controls physical pins (loads/relays on pins 2-13).
 * - Remote bitmask controls remote loads (bit n = remote load n).
 * - Then applies dual tariff forcing using bitwise OR operation (local and remote).
 * - OR operation ensures external overrides take precedence (1|x = 1).
 * - Function is called atomically to prevent race conditions with ISR.
 *
 * @ingroup GeneralProcessing
 */
OverrideMasks getOverrideBitmask(const bool bOffPeak, const int16_t currentTemperature_x100)
{
  OverrideMasks masks{ 0, 0 };

  // Add external override pins
  if constexpr (OVERRIDE_PIN_PRESENT)
  {
    masks = Forcing::pinForcing(overrideTable, [](const uint8_t pin)
                                {
                                  return !getPinState(pin);  // Pin is LOW (active)
                                });
  }

  // Add dual tariff forcing - OR operation handles precedence automatically
  return masks | getDualTariffForcingBitmask(bOffPeak, currentTemperature_x100);
}

/**
 * @brief Reads an active-LOW input pin, and tells how it changed since the previous reading.
 *
 * @tparam pin the input pin: each one keeps its own previous level, released at startup.
 * @return its state and its edges.
 *
 * @details Read each pin every second, whether its edges are used or not: a previous level
 *          left stale would turn a press made long ago into an edge now.
 *
 * @ingroup GeneralProcessing
 */
template< uint8_t pin >
PinEdges readPinEdges()
{
  return trackPinEdges< pin >(!getPinState(pin));
}

/**
 * @brief Reads the diversion pin.
 *
 * @return true unless the diversion pin is LOW. With diversion OFF, the surplus is not
 *         diverted any more, but forced loads and relays stay ON.
 *
 * @ingroup GeneralProcessing
 */
bool isDiversionEnabled()
{
  if constexpr (DIVERSION_PIN_PRESENT)
  {
    const PinEdges pin{ readPinEdges< diversionPin >() };

    if (pin.changed)
    {
      statusln(pin.active ? F("Trigger diversion OFF!") : F("End diversion OFF!"));
    }

    return !pin.active;
  }
  else
  {
    return true;
  }
}

/**
 * @brief Tells whether the router is OFF.
 *
 * @return true in calibration mode, or while the router OFF pin is LOW. With the router OFF,
 *         every load and relay is OFF, forcing included.
 *
 * @ingroup GeneralProcessing
 */
bool isRouterOff()
{
  if constexpr (CALIBRATION_MODE)
  {
    return true;
  }
  else if constexpr (ROUTER_OFF_PIN_PRESENT)
  {
    const PinEdges pin{ readPinEdges< routerOffPin >() };

    if (pin.changed)
    {
      statusln(pin.active ? F("Router OFF!") : F("Router ON!"));
    }

    return pin.active;
  }
  else
  {
    return false;
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
 * @return true if the system is in a low tariff (off-peak) period.
 * @return false if the system is in a high tariff (on-peak) period.
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
  const PinEdges pin{ readPinEdges< dualTariffPin >() };

  if (pin.pressed())
  {
    // we start off-peak period
    statusln(F("Change to off-peak period!"));

    ul_TimeOffPeak = millis();

    if constexpr (PRIORITY_ROTATION == RotationModes::AUTO)
    {
      proceedRotation();
    }
  }

  // end of off-peak period
  if (pin.released())
  {
    statusln(F("Change to peak period!"));
  }

  return pin.active;
}

/**
 * @brief Handles load priority rotation.
 *
 * This function manages load priority rotation behavior based on the system configuration.
 * It supports priority rotation via pin control or automatic rotation.
 * Override logic is handled in getOverrideBitmask().
 *
 * @param bOverridden true while a load, relay or remote load is forced: no rotation then.
 *
 * @details
 * - The rotation pin works with or without dual tariff.
 * - Automatic rotation: after a defined period without diversion; with dual tariff,
 *   `proceedDualTariffLogic` rotates when off-peak starts instead.
 * - The rotation pin is read every second, forced or not: a press during forcing is dropped,
 *   not carried over to its end.
 * - Override logic (external pins + dual tariff forcing) is handled atomically in getOverrideBitmask().
 *
 * @ingroup GeneralProcessing
 */
void proceedLoadPriorities(const bool bOverridden)
{
  if constexpr (PRIORITY_ROTATION == RotationModes::PIN)
  {
    const PinEdges pin{ readPinEdges< rotationPin >() };

    if (isRotationRequested(pin, bOverridden))
    {
      statusln(F("Trigger rotation!"));

      proceedRotation();
    }
  }
  else if constexpr (PRIORITY_ROTATION == RotationModes::AUTO && !DUAL_TARIFF)
  {
    if (!bOverridden && ROTATION_AFTER_SECONDS < Shared::absenceOfDivertedEnergyCountInSeconds)
    {
      proceedRotation();

      Shared::absenceOfDivertedEnergyCountInSeconds = 0;
    }
  }
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
 * - Initializes the Serial interface.
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

  // 8N1 even for the IoT output, so the startup configuration shows on a serial monitor with its
  // default settings: 7E1 comes at the end of setup().
  SerialOutput::beginStartupText();

  // On start, display config info in the serial monitor (human-readable and IoT outputs)
  printConfiguration();

  // initializes all loads to OFF at startup
  initializeProcessing();

  // The control inputs are then read once per second: read them now, before the first load decision
  Shared::b_diversionEnabled = !isRouterOff() && isDiversionEnabled();

  logLoadPriorities();

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    tempSensors.printSensorsOnBus();  // empty unless CALIBRATION_MODE
    tempSensors.initTemperatureSensors();
    for (auto &reading : temperatureReadings)
    {
      reading = DEVICE_DISCONNECTED_RAW;  // not sent until read
    }
  }

  debug(F(">>free RAM = "));
  debugln(freeRam());  // a useful value to keep an eye on
  infoln(F("----"));

  SerialOutput::endStartupText< SERIAL_OUTPUT_TYPE == SerialOutputType::IoT >();
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
 * - Integer arithmetic only (datalog_math.h): the calibration factors are fixed-point constants.
 * - Updates the total power by summing the power of all phases.
 *
 * @ingroup GeneralProcessing
 */
void updatePowerAndVoltageData()
{
  // the calibration constants in fixed point, converted at compile time: no float code (#120)
  static constexpr auto powerCal PROGMEM{ Datalog::powerCalibration(f_powerCal) };
  static constexpr auto voltageCal PROGMEM{ Datalog::voltageCalibration(Datalog::vrmsFactor(DATALOG_PERIOD_IN_SECONDS), f_voltageCal) };

  tx_data.power = 0;
  uint8_t phase{ NO_OF_PHASES };
  do
  {
    --phase;
    const uint16_t sampleSets{ Shared::copyOf_sampleSetsDuringThisDatalogPeriod };

    // import = +ve: the sum of the ISR is negative when importing
    tx_data.power_L[phase] = Datalog::watts(-Shared::copyOf_sumP_atSupplyPoint[phase] / sampleSets,
                                            pgm_read_word(&powerCal.value[phase]), powerCal.shift);

    tx_data.power += tx_data.power_L[phase];

    tx_data.Vrms_L_x100[phase] = Datalog::vrms(Shared::copyOf_sum_Vsquared[phase] / sampleSets,
                                               pgm_read_word(&voltageCal.value[phase]), voltageCal.shift);
  } while (phase);
}

/**
 * @brief Runs the 1-Wire transaction of this mains cycle, if any.
 *
 * @details Called at the top of the loop pass that sees the new mains cycle, before any other
 *          work: the cycle's work in the ADC interrupt (processStartNewCycle()) is done by then
 *          (Shared::b_newMainsCycle), and the long slots of the transaction (its resets and 0s
 *          written, which keep interrupts off for 65 to 70 µs) end long before the next one,
 *          a mains cycle later (temp_schedule.h).
 *
 * @param onTime Whether the loop was idle when the cycle began: the flag is fresh
 *
 * @ingroup TemperatureSensing
 */
void runTemperatureTransaction(const bool onTime)
{
  const TempSchedule::Action action{ tempSchedule.next(onTime) };
  switch (action.step)
  {
    case TempSchedule::Step::CONVERT:
      tempSensors.requestTemperatures();
      break;
    case TempSchedule::Step::ADDRESS:
      if (!tempSensors.startRead(action.sensor))
      {
        temperatureReadings[action.sensor] = DEVICE_DISCONNECTED_RAW;  // no answer to the reset
        tempSchedule.skipRestOf(action.sensor);
      }
      break;
    case TempSchedule::Step::READ:
      tempSensors.readScratchpad(action.sensor);
      break;
    case TempSchedule::Step::CLOSE:
      temperatureReadings[action.sensor] = tempSensors.endRead();
      break;
    default:
      break;
  }
}

/**
 * @brief Processes temperature data from DS18B20 sensors.
 *
 * @details At the end of each datalog period: the readings of the period's round (runTemperatureTransaction()),
 *          converted 750 ms or more before they were read, filtered and copied for the datalog output. Invalid
 *          readings are identified as 85.00°C (encoded as 8500) with a delta greater than 5.00°C (encoded as 500)
 *          from the previous reading. The next round starts at once if the loop handles the end of the
 *          period on time, so that its conversion runs before the datalog output (the UART interrupts would add
 *          to the delay of the ADC interrupt after a 1-Wire slot); otherwise with the next mains cycle.
 *
 * @note This function assumes that temperature values are stored as integers multiplied by 100 for precision.
 *
 * @pre The `temperatureSensing` object must be initialized and configured with the connected DS18B20 sensors.
 *
 * @post The `tx_data.temperature_x100` array is updated with the latest valid temperature readings.
 *
 * @param bStartNow Whether the end of the period is handled on time, soon after its mains cycle was flagged
 *
 * @ingroup TemperatureSensing
 */
void processTemperatureData(const bool bStartNow)
{
  // 85.00 °C more than 5.00 °C away from the last valid reading: the power-on value, skipped
  static DS18B20::PowerOnFilter powerOnFilter[tempSensors.size()];

  uint8_t idx{ tempSensors.size() };
  do
  {
    --idx;
    tx_data.temperature_x100[idx] = powerOnFilter[idx](temperatureReadings[idx]);
  } while (idx);

  // the next round: its conversion now, in the cycle that ends the period (which had no transaction,
  // the round being over), or with the next cycle
  if (tempSchedule.newPeriod() && bStartNow)
  {
    runTemperatureTransaction(true);
  }
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
 * @note Override pins are checked at 1 Hz (once per second) for debouncing and to minimize
 *       ISR communication overhead. This means override pin changes have a latency of 0-1 second
 *       before taking effect. This is acceptable since override pins are typically used for
 *       manual control or scheduled operations (e.g., dual tariff forcing) rather than
 *       time-critical automatic control.
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

  // Tariff transitions must be tracked every second, whatever the override state:
  // dual tariff forcing depends on the off-peak start timestamp set here.
  if constexpr (DUAL_TARIFF)
  {
    bOffPeak = proceedDualTariffLogic();
  }

  // Router OFF: no forcing and no diversion, so every load and relay is OFF
  const bool bRouterOff{ isRouterOff() };
  const bool bDiversionEnabled{ !bRouterOff && isDiversionEnabled() };

  // Get complete override bitmasks atomically (external pins + dual tariff forcing)
  const OverrideMasks privateOverrideMasks{ bRouterOff ? OverrideMasks{ 0, 0 } : getOverrideBitmask(bOffPeak, iTemperature_x100) };

  if constexpr (RELAY_DIVERSION)
  {
    relayDiverter.inc_duration();
    relayDiverter.proceed_relays(privateOverrideMasks.local, bDiversionEnabled);
  }

  // Copy to the shared versions for ISR access
  Shared::b_diversionEnabled = bDiversionEnabled;
  Shared::overrideBitmask = privateOverrideMasks.local;
  Shared::remoteOverrideBitmask = privateOverrideMasks.remote;

  // no rotation while anything is forced (local or remote)
  proceedLoadPriorities(privateOverrideMasks.local || privateOverrideMasks.remote);
}

/**
 * @brief Main processor loop.
 *
 * This function handles non-time-critical tasks such as load management, telemetry updates,
 * and temperature sensing. It processes ADC data through flags set by the ISR and ensures
 * proper system operation.
 *
 * @details
 * - Executes tasks triggered by the `b_newMainsCycle` flag, which is set once per mains cycle; with temperature
 *   sensing, the cycle's 1-Wire transaction first, before any other work.
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

  const bool bNewMainsCycle{ Shared::b_newMainsCycle };  // set once per mains cycle, by the ADC interrupt
  if (bNewMainsCycle)
  {
    Shared::b_newMainsCycle = false;  // reset the flag
  }

  [[maybe_unused]] uint32_t cycleSeen_us{ 0 };  // temperature sensing: when this pass saw the new cycle
  [[maybe_unused]] bool bCycleOnTime{ false };  // ... and whether on time

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    // The flag was set at most one loop pass ago: on time if the last pass was short (the loop
    // idle when the cycle began), late after a long one (per-second work, datalog, RF frame).
    constexpr uint32_t ON_TIME_US{ 1000 };
    static uint32_t passStart_us{ 0 };
    const uint32_t now_us{ micros() };
    const bool bOnTime{ (now_us - passStart_us) < ON_TIME_US };
    passStart_us = now_us;

    if (bNewMainsCycle)
    {
      cycleSeen_us = now_us;
      bCycleOnTime = bOnTime;
      runTemperatureTransaction(bOnTime);  // first: its long slots must end before the next cycle's work
    }
  }

  // Write the datalog output in progress, as far as the serial buffer allows (never waits)
  SerialOutput::poll();

  // Process any pending RF transmissions (called outside ISR to avoid blocking)
  if constexpr (REMOTE_LOADS_PRESENT)
  {
    processRemoteLoadTransmissions();
  }

  if (bNewMainsCycle)
  {
    ++perSecondTimer;

    if (perSecondTimer >= SUPPLY_FREQUENCY)
    {
      perSecondTimer = 0;
      handlePerSecondTasks(bOffPeak, iTemperature_x100);
    }
  }

  // With temperature sensing, the end of the period waits for the pass that sees the new cycle, so
  // that the next round's conversion starts after the cycle's work and before the datalog output.
  // The timing, a sample set being 6 conversions (624 µs, whatever the mains frequency):
  // - L1's positive crossing: the period's datalog copies (processDataLogging(), from
  //   processPlusHalfCycle()), then b_datalogEventPending;
  // - 2 sample sets later (1248 µs): the cycle's work, processStartNewCycle(), the heaviest call
  //   (its ADMUX written ~660 to 780 cycles in with 4 loads: sim/README.md, Temperature with remote
  //   loads, RF datalog and relays), then b_newMainsCycle;
  // - the next such call, a mains cycle later.
  // The conversion (reset, Skip ROM, Convert T: ~2.1 ms of bus I/O) cannot fit between the event
  // and processStartNewCycle(); started after it, its slots fall among the ordinary calls.
  if (Shared::b_datalogEventPending && (bNewMainsCycle || !TEMP_SENSOR_PRESENT))
  {
    Shared::b_datalogEventPending = false;

    updatePowerAndVoltageData();

    if constexpr (RELAY_DIVERSION)
    {
      relayDiverter.update_average(tx_data.power);
    }

    if constexpr (TEMP_SENSOR_PRESENT)
    {
      // On time if this pass saw the new cycle on time and is still within 3 ms of it (the per-second
      // work may run in between): the conversion's slots then end within ~6 ms of the cycle's work
      constexpr uint32_t PERIOD_END_ON_TIME_US{ 3000 };
      processTemperatureData(bCycleOnTime && (micros() - cycleSeen_us) < PERIOD_END_ON_TIME_US);

      // the sensor checked against the dual tariff temperature threshold
      iTemperature_x100 = tx_data.temperature_x100[BOOST_TEMP_SENSOR];
    }

    sendResults(bOffPeak);
  }
}  // end of loop()
