/**
 * @file processing.cpp
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Implements the processing engine
 * @version 0.1
 * @date 2021-10-04
 *
 * @copyright Copyright (c) 2021
 *
 */

#include <Arduino.h>

#include "calibration.h"
#include "dualtariff.h"
#include "processing.h"
#include "utils_pins.h"

// Define operating limits for the LP filters which identify DC offset in the voltage
// sample streams. By limiting the output range, these filters always should start up
// correctly.
constexpr int32_t l_DCoffset_V_min{ (512L - 100L) * 256L }; /**< mid-point of ADC minus a working margin */
constexpr int32_t l_DCoffset_V_max{ (512L + 100L) * 256L }; /**< mid-point of ADC plus a working margin */
constexpr int16_t i_DCoffset_I_nom{ 512L };                 /**< nominal mid-point value of ADC @ x1 scale */

int32_t l_DCoffset_V[NO_OF_PHASES]; /**< <--- for LPF */

/**< main energy bucket for 3-phase use, with units of Joules * SUPPLY_FREQUENCY */
constexpr float f_capacityOfEnergyBucket_main{ static_cast< float >(WORKING_ZONE_IN_JOULES * SUPPLY_FREQUENCY) };
/**< for resetting flexible thresholds */
constexpr float f_midPointOfEnergyBucket_main{ f_capacityOfEnergyBucket_main * 0.5F };
/**< threshold in anti-flicker mode - must not exceed 0.4 */
constexpr float f_offsetOfEnergyThresholdsInAFmode{ 0.1F };

constexpr OutputModes outputMode{ OutputModes::NORMAL }; /**< Output mode to be used */

/**
 * @brief set default threshold at compile time so the variable can be read-only
 *
 * @param lower True to set the lower threshold, false for higher
 * @return the corresponding threshold
 */
constexpr auto initThreshold(const bool lower)
{
  return lower
           ? f_capacityOfEnergyBucket_main * (0.5F - ((OutputModes::ANTI_FLICKER == outputMode) ? f_offsetOfEnergyThresholdsInAFmode : 0.0F))
           : f_capacityOfEnergyBucket_main * (0.5F + ((OutputModes::ANTI_FLICKER == outputMode) ? f_offsetOfEnergyThresholdsInAFmode : 0.0F));
}

constexpr float f_lowerThreshold_default{ initThreshold(true) };  /**< lower default threshold set accordingly to the output mode */
constexpr float f_upperThreshold_default{ initThreshold(false) }; /**< upper default threshold set accordingly to the output mode */

float f_energyInBucket_main{ 0.0F }; /**< main energy bucket (over all phases) */
float f_lowerEnergyThreshold;        /**< dynamic lower threshold */
float f_upperEnergyThreshold;        /**< dynamic upper threshold */

// for improved control of multiple loads
bool b_recentTransition{ false };                 /**< a load state has been recently toggled */
uint8_t postTransitionCount;                      /**< counts the number of cycle since last transition */
constexpr uint8_t POST_TRANSITION_MAX_COUNT{ 3 }; /**< allows each transition to take effect */
// constexpr uint8_t POST_TRANSITION_MAX_COUNT{50}; /**< for testing only */
uint8_t activeLoad{ NO_OF_DUMPLOADS }; /**< current active load */

int32_t l_sumP[NO_OF_PHASES];                /**< cumulative power per phase */
int32_t l_sampleVminusDC[NO_OF_PHASES];      /**< current raw voltage sample filtered */
int32_t l_cumVdeltasThisCycle[NO_OF_PHASES]; /**< for the LPF which determines DC offset (voltage) */
int32_t l_sumP_atSupplyPoint[NO_OF_PHASES];  /**< for summation of 'real power' values during datalog period */
int32_t l_sum_Vsquared[NO_OF_PHASES];        /**< for summation of V^2 values during datalog period */

uint8_t n_samplesDuringThisMainsCycle[NO_OF_PHASES]; /**< number of sample sets for each phase during each mains cycle */
uint16_t i_sampleSetsDuringThisDatalogPeriod;        /**< number of sample sets during each datalogging period */

remove_cv< remove_reference< decltype(DATALOG_PERIOD_IN_MAINS_CYCLES) >::type >::type n_cycleCountForDatalogging{ 0 }; /**< for counting how often datalog is updated */

uint8_t n_lowestNoOfSampleSetsPerMainsCycle; /**< For a mechanism to check the integrity of this code structure */

// For an enhanced polarity detection mechanism, which includes a persistence check
Polarities polarityOfMostRecentSampleV[NO_OF_PHASES];    /**< for zero-crossing detection */
Polarities polarityConfirmed[NO_OF_PHASES];              /**< for zero-crossing detection */
Polarities polarityConfirmedOfLastSampleV[NO_OF_PHASES]; /**< for zero-crossing detection */

LoadStates physicalLoadState[NO_OF_DUMPLOADS]; /**< Physical state of the loads */
uint16_t countLoadON[NO_OF_DUMPLOADS];         /**< Number of cycle the load was ON (over 1 datalog period) */

bool beyondStartUpPeriod{ false }; /**< start-up delay, allows things to settle */

/**
 * @brief Initializes the ports and load states for processing
 *
 */
void initializeProcessing()
{
  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    loadPrioritiesAndState[i] = loadPrioritiesAtStartup[i];
    pinMode(physicalLoadPin[i], OUTPUT);  // driver pin for Load #n
    loadPrioritiesAndState[i] &= loadStateMask;
  }
  updatePhysicalLoadStates();  // allows the logical-to-physical mapping to be changed

  updatePortsStates();  // updates output pin states

  for (auto &DCoffset_V : l_DCoffset_V)
  {
    DCoffset_V = 512L * 256L;  // nominal mid-point value of ADC @ x256 scale
  }

  for (auto &bOverrideLoad : b_overrideLoadOn)
  {
    bOverrideLoad = false;
  }

  // First stop the ADC
  bit_clear(ADCSRA, ADEN);

  // Activate free-running mode
  ADCSRB = 0x00;

  // Set up the ADC to be free-running
  bit_set(ADCSRA, ADPS0);  // Set the ADC's clock to system clock / 128
  bit_set(ADCSRA, ADPS1);
  bit_set(ADCSRA, ADPS2);

  bit_set(ADCSRA, ADATE);  // set the Auto Trigger Enable bit in the ADCSRA register. Because
  // bits ADTS0-2 have not been set (i.e. they are all zero), the
  // ADC's trigger source is set to "free running mode".

  bit_set(ADCSRA, ADIE);  // set the ADC interrupt enable bit. When this bit is written
  // to one and the I-bit in SREG is set, the
  // ADC Conversion Complete Interrupt is activated.

  bit_set(ADCSRA, ADEN);  // Enable the ADC

  bit_set(ADCSRA, ADSC);  // start ADC manually first time

  sei();  // Enable Global Interrupts
}

/**
 * @brief Initializes the optional pins
 *
 */
void initializeOptionalPins()
{
  if constexpr (DUAL_TARIFF)
  {
    pinMode(dualTariffPin, INPUT_PULLUP);  // set as input & enable the internal pullup resistor
    delay(100);                            // allow time to settle

    ul_TimeOffPeak = millis();
  }

  if constexpr (OVERRIDE_PIN_PRESENT)
  {
    pinMode(forcePin, INPUT_PULLUP);  // set as input & enable the internal pullup resistor
    delay(100);                       // allow time to settle
  }

  if constexpr (PRIORITY_ROTATION == RotationModes::PIN)
  {
    pinMode(rotationPin, INPUT_PULLUP);  // set as input & enable the internal pullup resistor
    delay(100);                          // allow time to settle
  }

  if constexpr (DIVERSION_PIN_PRESENT)
  {
    pinMode(diversionPin, INPUT_PULLUP);  // set as input & enable the internal pullup resistor
    delay(100);                           // allow time to settle
  }

  if constexpr (RELAY_DIVERSION)
  {
    relays.initializePins();
  }

  if constexpr (WATCHDOG_PIN_PRESENT)
  {
    pinMode(watchDogPin, OUTPUT);  // set as output
    setPinOFF(watchDogPin);        // set to off
  }
}

#if !defined(__DOXYGEN__)
void updatePortsStates() __attribute__((optimize("-O3")));
#endif
/**
 * @brief update the control ports for each of the physical loads
 *
 */
void updatePortsStates()
{
  uint16_t pinsON{ 0 };
  uint16_t pinsOFF{ 0 };

  uint8_t i{ NO_OF_DUMPLOADS };

  do
  {
    --i;
    // update the local load's state.
    if (LoadStates::LOAD_OFF == physicalLoadState[i])
    {
      // setPinOFF(physicalLoadPin[i]);
      pinsOFF |= bit(physicalLoadPin[i]);
    }
    else
    {
      ++countLoadON[i];
      // setPinON(physicalLoadPin[i]);
      pinsON |= bit(physicalLoadPin[i]);
    }
  } while (i);

  setPinsOFF(pinsOFF);
  setPinsON(pinsON);
}

/**
 * @brief This function provides the link between the logical and physical loads.
 * @details The array, logicalLoadState[], contains the on/off state of all logical loads, with
 *          element 0 being for the one with the highest priority. The array,
 *          physicalLoadState[], contains the on/off state of all physical loads.
 *
 *          The lowest 7 bits of element is the load number as defined in 'physicalLoadState'.
 *          The highest bit of each 'loadPrioritiesAndState' determines if the load is ON or OFF.
 *          The order of each element in 'loadPrioritiesAndState' determines the load priority.
 *          'loadPrioritiesAndState[i] & loadStateMask' will extract the load number at position 'i'
 *          'loadPrioritiesAndState[i] & loadStateOnBit' will extract the load state at position 'i'
 *
 *          Any other mapping relationships could be configured here.
 *
 * @ingroup TimeCritical
 */
void updatePhysicalLoadStates()
{
  if constexpr (PRIORITY_ROTATION != RotationModes::OFF)
  {
    if (b_reOrderLoads)
    {
      uint8_t i{ NO_OF_DUMPLOADS - 1 };
      const auto temp{ loadPrioritiesAndState[i] };
      do
      {
        loadPrioritiesAndState[i] = loadPrioritiesAndState[i - 1];
        --i;
      } while (i);
      loadPrioritiesAndState[0] = temp;

      b_reOrderLoads = false;
    }

    if constexpr (!DUAL_TARIFF)
    {
      if (0x00 == (loadPrioritiesAndState[0] & loadStateOnBit))
      {
        EDD_isIdle = true;
      }
      else
      {
        EDD_isIdle = false;
        absenceOfDivertedEnergyCount = 0;
      }
    }
  }

  const bool bDiversionOff{ b_diversionOff };
  uint8_t idx{ NO_OF_DUMPLOADS };
  do
  {
    --idx;
    const auto iLoad{ loadPrioritiesAndState[idx] & loadStateMask };
    physicalLoadState[iLoad] = !bDiversionOff && (b_overrideLoadOn[iLoad] || (loadPrioritiesAndState[idx] & loadStateOnBit)) ? LoadStates::LOAD_ON : LoadStates::LOAD_OFF;
  } while (idx);
}

/**
 * @brief Process with the polarity for the actual voltage sample for the specific phase
 *
 * @param phase the phase number [0..NO_OF_PHASES[
 * @param rawSample the current sample for the specified phase
 *
 * @ingroup TimeCritical
 */
void processPolarity(const uint8_t phase, const int16_t rawSample)
{
  // remove DC offset from each raw voltage sample by subtracting the accurate value
  // as determined by its associated LP filter.
  l_sampleVminusDC[phase] = (static_cast< int32_t >(rawSample) << 8) - l_DCoffset_V[phase];
  polarityOfMostRecentSampleV[phase] = (l_sampleVminusDC[phase] > 0) ? Polarities::POSITIVE : Polarities::NEGATIVE;
}

/**
 * @brief Process the calculation for the actual current raw sample for the specific phase
 *
 * @param phase the phase number [0..NO_OF_PHASES[
 * @param rawSample the current sample for the specified phase
 *
 * @ingroup TimeCritical
 */
void processCurrentRawSample(const uint8_t phase, const int16_t rawSample)
{
  // extra items for an LPF to improve the processing of data samples from CT1
  static int32_t lpf_long[NO_OF_PHASES]{};  // new LPF, for offsetting the behaviour of CTx as a HPF

  // remove most of the DC offset from the current sample (the precise value does not matter)
  int32_t sampleIminusDC = (static_cast< int32_t >(rawSample - i_DCoffset_I_nom)) << 8;

  // extra filtering to offset the HPF effect of CTx
  const int32_t last_lpf_long{ lpf_long[phase] };
  lpf_long[phase] += alpha * (sampleIminusDC - last_lpf_long);
  sampleIminusDC += (lpf_gain * lpf_long[phase]);

  // calculate the "real power" in this sample pair and add to the accumulated sum
  const int32_t filtV_div4 = l_sampleVminusDC[phase] >> 2;  // reduce to 16-bits (now x64, or 2^6)
  const int32_t filtI_div4 = sampleIminusDC >> 2;           // reduce to 16-bits (now x64, or 2^6)
  int32_t instP = filtV_div4 * filtI_div4;                  // 32-bits (now x4096, or 2^12)
  instP >>= 12;                                             // scaling is now x1, as for Mk2 (V_ADC x I_ADC)

  l_sumP[phase] += instP;                // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
  l_sumP_atSupplyPoint[phase] += instP;  // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
}

/**
 * @brief This routine prevents a zero-crossing point from being declared until a certain number
 *        of consecutive samples in the 'other' half of the waveform have been encountered.
 *
 * @param phase the phase number [0..NO_OF_PHASES[
 *
 * @ingroup TimeCritical
 */
void confirmPolarity(const uint8_t phase)
{
  static uint8_t count[NO_OF_PHASES]{};

  if (polarityOfMostRecentSampleV[phase] == polarityConfirmedOfLastSampleV[phase])
  {
    count[phase] = 0;
    return;
  }

  if (++count[phase] > PERSISTENCE_FOR_POLARITY_CHANGE)
  {
    count[phase] = 0;
    polarityConfirmed[phase] = polarityOfMostRecentSampleV[phase];
  }
}

/**
 * @brief Process the calculation for the current voltage sample for the specific phase
 *
 * @param phase the phase number [0..NO_OF_PHASES[
 *
 * @ingroup TimeCritical
 */
void processVoltage(const uint8_t phase)
{
  // for the Vrms calculation (for datalogging only)
  const int32_t filtV_div4{ l_sampleVminusDC[phase] >> 2 };  // reduce to 16-bits (now x64, or 2^6)
  int32_t inst_Vsquared{ filtV_div4 * filtV_div4 };          // 32-bits (now x4096, or 2^12)

  if constexpr (DATALOG_PERIOD_IN_SECONDS > 10)
  {
    inst_Vsquared >>= 16;  // scaling is now x1/16 (V_ADC x I_ADC)
  }
  else
  {
    inst_Vsquared >>= 12;  // scaling is now x1 (V_ADC x I_ADC)
  }

  l_sum_Vsquared[phase] += inst_Vsquared;  // cumulative V^2 (V_ADC x I_ADC)
  //
  // store items for use during next loop
  l_cumVdeltasThisCycle[phase] += l_sampleVminusDC[phase];           // for use with LP filter
  polarityConfirmedOfLastSampleV[phase] = polarityConfirmed[phase];  // for identification of half cycle boundaries
  ++n_samplesDuringThisMainsCycle[phase];                            // for real power calculations
}

/**
 * @brief Process the startup period for the router.
 *
 * @param phase the phase number [0..NO_OF_PHASES[
 *
 * @ingroup TimeCritical
 */
void processStartUp(const uint8_t phase)
{
  // wait until the DC-blocking filters have had time to settle
  if (millis() <= (initialDelay + startUpPeriod))
  {
    return;  // still settling, do nothing
  }

  // the DC-blocking filters have had time to settle
  beyondStartUpPeriod = true;
  l_sumP[phase] = 0;
  l_sumP_atSupplyPoint[phase] = 0;
  n_samplesDuringThisMainsCycle[phase] = 0;
  i_sampleSetsDuringThisDatalogPeriod = 0;

  n_lowestNoOfSampleSetsPerMainsCycle = UINT8_MAX;
  // can't say "Go!" here 'cos we're in an ISR!
}

/**
 * @brief Process the case of high energy level, some action may be required.
 *
 * @ingroup TimeCritical
 */
void proceedHighEnergyLevel()
{
  bool bOK_toAddLoad{ true };
  const auto tempLoad{ nextLogicalLoadToBeAdded() };

  if (tempLoad >= NO_OF_DUMPLOADS)
  {
    return;
  }

  // a load which is now OFF has been identified for potentially being switched ON
  if (b_recentTransition)
  {
    // During the post-transition period, any increase in the energy level is noted.
    f_upperEnergyThreshold = f_energyInBucket_main;

    // the energy thresholds must remain within range
    if (f_upperEnergyThreshold > f_capacityOfEnergyBucket_main)
    {
      f_upperEnergyThreshold = f_capacityOfEnergyBucket_main;
    }

    // Only the active load may be switched during this period. All other loads must
    // wait until the recent transition has had sufficient opportunity to take effect.
    bOK_toAddLoad = (tempLoad == activeLoad);
  }

  if (bOK_toAddLoad)
  {
    loadPrioritiesAndState[tempLoad] |= loadStateOnBit;
    activeLoad = tempLoad;
    postTransitionCount = 0;
    b_recentTransition = true;
  }
}

/**
 * @brief Process the case of low energy level, some action may be required.
 *
 * @ingroup TimeCritical
 */
void proceedLowEnergyLevel()
{
  bool bOK_toRemoveLoad{ true };
  const auto tempLoad{ nextLogicalLoadToBeRemoved() };

  if (tempLoad >= NO_OF_DUMPLOADS)
  {
    return;
  }

  // a load which is now ON has been identified for potentially being switched OFF
  if (b_recentTransition)
  {
    // During the post-transition period, any decrease in the energy level is noted.
    f_lowerEnergyThreshold = f_energyInBucket_main;

    // the energy thresholds must remain within range
    if (f_lowerEnergyThreshold < 0)
    {
      f_lowerEnergyThreshold = 0;
    }

    // Only the active load may be switched during this period. All other loads must
    // wait until the recent transition has had sufficient opportunity to take effect.
    bOK_toRemoveLoad = (tempLoad == activeLoad);
  }

  if (bOK_toRemoveLoad)
  {
    loadPrioritiesAndState[tempLoad] &= loadStateMask;
    activeLoad = tempLoad;
    postTransitionCount = 0;
    b_recentTransition = true;
  }
}

/**
 * @brief This code is executed once per 20mS, shortly after the start of each new
 *        mains cycle on phase 0.
 * @details Changing the state of the loads  is a 3-part process:
 *          - change the LOGICAL load states as necessary to maintain the energy level
 *          - update the PHYSICAL load states according to the logical -> physical mapping
 *          - update the driver lines for each of the loads.
 *
 * @ingroup TimeCritical
 */
void processStartNewCycle()
{
  // Restrictions apply for the period immediately after a load has been switched.
  // Here the b_recentTransition flag is checked and updated as necessary.
  // if (b_recentTransition)
  //   b_recentTransition = (++postTransitionCount < POST_TRANSITION_MAX_COUNT);
  // for optimization, the next line is equivalent to the two lines above
  b_recentTransition &= (++postTransitionCount < POST_TRANSITION_MAX_COUNT);

  if (f_energyInBucket_main > f_midPointOfEnergyBucket_main)
  {
    // the energy state is in the upper half of the working range
    f_lowerEnergyThreshold = f_lowerThreshold_default;  // reset the "opposite" threshold
    if (f_energyInBucket_main > f_upperEnergyThreshold)
    {
      // Because the energy level is high, some action may be required
      proceedHighEnergyLevel();
    }
  }
  else
  {
    // the energy state is in the lower half of the working range
    f_upperEnergyThreshold = f_upperThreshold_default;  // reset the "opposite" threshold
    if (f_energyInBucket_main < f_lowerEnergyThreshold)
    {
      // Because the energy level is low, some action may be required
      proceedLowEnergyLevel();
    }
  }

  updatePhysicalLoadStates();  // allows the logical-to-physical mapping to be changed

  updatePortsStates();  // update the control ports for each of the physical loads

  // Now that the energy-related decisions have been taken, min and max limits can now
  // be applied  to the level of the energy bucket. This is to ensure correct operation
  // when conditions change, i.e. when import changes to export, and vice versa.
  //
  if (f_energyInBucket_main > f_capacityOfEnergyBucket_main)
  {
    f_energyInBucket_main = f_capacityOfEnergyBucket_main;
  }
  else if (f_energyInBucket_main < 0)
  {
    f_energyInBucket_main = 0;
  }
}

/**
 * @brief Process the start of a new -ve half cycle, for this phase, just after the zero-crossing point.
 *
 * @param phase the phase number [0..NO_OF_PHASES[
 *
 * @ingroup TimeCritical
 */
void processMinusHalfCycle(const uint8_t phase)
{
  // This is a convenient point to update the Low Pass Filter for removing the DC
  // component from the phase that is being processed.
  // The portion which is fed back into the integrator is approximately one percent
  // of the average offset of all the SampleVs in the previous mains cycle.
  //
  l_DCoffset_V[phase] += (l_cumVdeltasThisCycle[phase] >> 12);
  l_cumVdeltasThisCycle[phase] = 0;

  // To ensure that this LP filter will always start up correctly when 240V AC is
  // available, its output value needs to be prevented from drifting beyond the likely range
  // of the voltage signal.
  //
  if (l_DCoffset_V[phase] < l_DCoffset_V_min)
  {
    l_DCoffset_V[phase] = l_DCoffset_V_min;
  }
  else if (l_DCoffset_V[phase] > l_DCoffset_V_max)
  {
    l_DCoffset_V[phase] = l_DCoffset_V_max;
  }
}

#if !defined(__DOXYGEN__)
uint8_t nextLogicalLoadToBeAdded() __attribute__((optimize("-O3")));
#endif
/**
 * @brief Retrieve the next load that could be added (be aware of the order)
 *
 * @return The load number if successful, NO_OF_DUMPLOADS in case of failure
 *
 * @ingroup TimeCritical
 */
uint8_t nextLogicalLoadToBeAdded()
{
  for (uint8_t index = 0; index < NO_OF_DUMPLOADS; ++index)
  {
    if (0x00 == (loadPrioritiesAndState[index] & loadStateOnBit))
    {
      return (index);
    }
  }

  return (NO_OF_DUMPLOADS);
}

#if !defined(__DOXYGEN__)
uint8_t nextLogicalLoadToBeRemoved() __attribute__((optimize("-O3")));
#endif
/**
 * @brief Retrieve the next load that could be removed (be aware of the reverse-order)
 *
 * @return The load number if successful, NO_OF_DUMPLOADS in case of failure
 *
 * @ingroup TimeCritical
 */
uint8_t nextLogicalLoadToBeRemoved()
{
  uint8_t index{ NO_OF_DUMPLOADS };
  do
  {
    if (loadPrioritiesAndState[--index] & loadStateOnBit)
    {
      return (index);
    }
  } while (index);

  return (NO_OF_DUMPLOADS);
}

/**
 * @brief Process the latest contribution after each phase specific new cycle
 *        additional processing is performed after each main cycle based on phase 0.
 *
 * @param phase the phase number [0..NO_OF_PHASES[
 *
 * @ingroup TimeCritical
 */
void processLatestContribution(const uint8_t phase)
{
  // for efficiency, the energy scale is Joules * SUPPLY_FREQUENCY
  // add the latest energy contribution to the main energy accumulator
  f_energyInBucket_main += (l_sumP[phase] / n_samplesDuringThisMainsCycle[phase]) * f_powerCal[phase];

  // apply any adjustment that is required.
  if (0 == phase)
  {
    f_energyInBucket_main -= REQUIRED_EXPORT_IN_WATTS;  // energy scale is Joules x 50
    b_newMainsCycle = true;                             //  a 50 Hz 'tick' for use by the main code
  }
  // Applying max and min limits to the main accumulator's level
  // is deferred until after the energy related decisions have been taken
  //
}

#if !defined(__DOXYGEN__)
void processDataLogging() __attribute__((optimize("-O3")));
#endif
/**
 * @brief Process with data logging.
 * @details At the end of each datalogging period, copies are made of the relevant variables
 *          for use by the main code. These variable are then reset for use during the next
 *          datalogging period.
 *
 * @ingroup TimeCritical
 */
void processDataLogging()
{
  if (++n_cycleCountForDatalogging < DATALOG_PERIOD_IN_MAINS_CYCLES)
  {
    return;  // data logging period not yet reached
  }

  n_cycleCountForDatalogging = 0;

  uint8_t phase{ NO_OF_PHASES };
  do
  {
    --phase;
    copyOf_sumP_atSupplyPoint[phase] = l_sumP_atSupplyPoint[phase];
    l_sumP_atSupplyPoint[phase] = 0;

    copyOf_sum_Vsquared[phase] = l_sum_Vsquared[phase];
    l_sum_Vsquared[phase] = 0;
  } while (phase);

  uint8_t i{ NO_OF_DUMPLOADS };
  do
  {
    --i;
    copyOf_countLoadON[i] = countLoadON[i];
    countLoadON[i] = 0;
  } while (i);

  copyOf_sampleSetsDuringThisDatalogPeriod = i_sampleSetsDuringThisDatalogPeriod;  // (for diags only)
  copyOf_lowestNoOfSampleSetsPerMainsCycle = n_lowestNoOfSampleSetsPerMainsCycle;  // (for diags only)
  copyOf_energyInBucket_main = f_energyInBucket_main;                              // (for diags only)

  n_lowestNoOfSampleSetsPerMainsCycle = UINT8_MAX;
  i_sampleSetsDuringThisDatalogPeriod = 0;

  // signal the main processor that logging data are available
  // we skip the period from start to running stable
  b_datalogEventPending = beyondStartUpPeriod;
}

/**
 * @brief Process the start of a new +ve half cycle, for this phase, just after the zero-crossing point.
 *
 * @param phase the phase number [0..NO_OF_PHASES[
 *
 * @ingroup TimeCritical
 */
void processPlusHalfCycle(const uint8_t phase)
{
  processLatestContribution(phase);  // runs at 6.6 ms intervals

  // A performance check to monitor and display the minimum number of sets of
  // ADC samples per mains cycle, the expected number being 20ms / (104us * 6) = 32.05
  //
  if (0 == phase)
  {
    if (n_samplesDuringThisMainsCycle[phase] < n_lowestNoOfSampleSetsPerMainsCycle)
    {
      n_lowestNoOfSampleSetsPerMainsCycle = n_samplesDuringThisMainsCycle[phase];
    }

    processDataLogging();
  }

  l_sumP[phase] = 0;
  n_samplesDuringThisMainsCycle[phase] = 0;
}

/**
 * @brief This routine is called by the ISR when a pair of V & I sample becomes available.
 *
 * @param phase the phase number [0..NO_OF_PHASES[
 *
 * @ingroup TimeCritical
 */
void processRawSamples(const uint8_t phase)
{
  // The raw V and I samples are processed in "phase pairs"
  const auto &lastPolarity{ polarityConfirmedOfLastSampleV[phase] };

  if (Polarities::POSITIVE == polarityConfirmed[phase])
  {
    // the polarity of this sample is positive
    if (Polarities::POSITIVE != lastPolarity)
    {
      // This is the start of a new +ve half cycle, for this phase, just after the zero-crossing point.
      if (beyondStartUpPeriod)
      {
        processPlusHalfCycle(phase);
      }
      else
      {
        processStartUp(phase);
      }
    }

    // still processing samples where the voltage is POSITIVE ...
    // check to see whether the trigger device can now be reliably armed
    if ((0 == phase) && beyondStartUpPeriod && (2 == n_samplesDuringThisMainsCycle[0]))  // lower value for larger sample set
    {
      // This code is executed once per 20mS, shortly after the start of each new mains cycle on phase 0.
      processStartNewCycle();
    }
  }
  else
  {
    // the polarity of this sample is negative
    if (Polarities::NEGATIVE != lastPolarity)
    {
      // This is the start of a new -ve half cycle (just after the zero-crossing point)
      processMinusHalfCycle(phase);
    }
  }
}

/**
 * @brief Process the current voltage raw sample for the specific phase
 *
 * @param phase the phase number [0..NO_OF_PHASES[
 * @param rawSample the current sample for the specified phase
 *
 * @ingroup TimeCritical
 */
void processVoltageRawSample(const uint8_t phase, const int16_t rawSample)
{
  processPolarity(phase, rawSample);
  confirmPolarity(phase);
  //
  processRawSamples(phase);  // deals with aspects that only occur at particular stages of each mains cycle
  //
  processVoltage(phase);

  if (phase == 0)
  {
    ++i_sampleSetsDuringThisDatalogPeriod;
  }
}

/**
 * @brief Print the settings used for the selected output mode.
 *
 */
void printParamsForSelectedOutputMode()
{
  // display relevant settings for selected output mode
  DBUG(F("Output mode:    "));
  if (OutputModes::NORMAL == outputMode)
  {
    DBUGLN(F("normal"));
  }
  else
  {
    DBUGLN(F("anti-flicker"));
    DBUG(F("\toffsetOfEnergyThresholds  = "));
    DBUGLN(f_offsetOfEnergyThresholdsInAFmode);
  }
  DBUG(F("\tf_capacityOfEnergyBucket_main = "));
  DBUGLN(f_capacityOfEnergyBucket_main);
  DBUG(F("\tf_lowerEnergyThreshold   = "));
  DBUGLN(f_lowerThreshold_default);
  DBUG(F("\tf_upperEnergyThreshold   = "));
  DBUGLN(f_upperThreshold_default);
}
