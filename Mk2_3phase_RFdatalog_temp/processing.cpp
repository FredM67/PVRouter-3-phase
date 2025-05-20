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
#include "shared_var.h"

// Define operating limits for the LP filters which identify DC offset in the voltage
// sample streams. By limiting the output range, these filters always should start up
// correctly.
constexpr int32_t l_DCoffset_V_min{ (512L - 100L) * 256L }; /**< mid-point of ADC minus a working margin */
constexpr int32_t l_DCoffset_V_max{ (512L + 100L) * 256L }; /**< mid-point of ADC plus a working margin */
constexpr int16_t i_DCoffset_I_nom{ 512L };                 /**< nominal mid-point value of ADC @ x1 scale */

int32_t l_DCoffset_V[NO_OF_PHASES]{}; /**< <--- for LPF */

/**< main energy bucket for 3-phase use, with units of Joules * SUPPLY_FREQUENCY */
constexpr float f_capacityOfEnergyBucket_main{ static_cast< float >(WORKING_ZONE_IN_JOULES * SUPPLY_FREQUENCY) };
/**< for resetting flexible thresholds */
constexpr float f_midPointOfEnergyBucket_main{ f_capacityOfEnergyBucket_main * 0.5F };
/**< threshold in anti-flicker mode - must not exceed 0.4 */
constexpr float f_offsetOfEnergyThresholdsInAFmode{ 0.1F };

constexpr OutputModes outputMode{ OutputModes::NORMAL }; /**< Output mode to be used */

bool b_diversionStarted{ false }; /**< Tracks whether diversion has started */

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

float f_energyInBucket_main{ 0.0F };  /**< main energy bucket (over all phases) */
float f_lowerEnergyThreshold{ 0.0F }; /**< dynamic lower threshold */
float f_upperEnergyThreshold{ 0.0F }; /**< dynamic upper threshold */

// for improved control of multiple loads
bool b_recentTransition{ false };                 /**< a load state has been recently toggled */
uint8_t postTransitionCount{ 0 };                 /**< counts the number of cycle since last transition */
constexpr uint8_t POST_TRANSITION_MAX_COUNT{ 3 }; /**< allows each transition to take effect */
// constexpr uint8_t POST_TRANSITION_MAX_COUNT{50}; /**< for testing only */
uint8_t activeLoad{ NO_OF_DUMPLOADS }; /**< current active load */

int32_t l_sumP[NO_OF_PHASES]{};                /**< cumulative power per phase */
int32_t l_sampleVminusDC[NO_OF_PHASES]{};      /**< current raw voltage sample filtered */
int32_t l_cumVdeltasThisCycle[NO_OF_PHASES]{}; /**< for the LPF which determines DC offset (voltage) */
int32_t l_sumP_atSupplyPoint[NO_OF_PHASES]{};  /**< for summation of 'real power' values during datalog period */
int32_t l_sum_Vsquared[NO_OF_PHASES]{};        /**< for summation of V^2 values during datalog period */

uint8_t n_samplesDuringThisMainsCycle[NO_OF_PHASES]{}; /**< number of sample sets for each phase during each mains cycle */
uint16_t i_sampleSetsDuringThisDatalogPeriod{ 0 };     /**< number of sample sets during each datalogging period */

remove_cv< remove_reference< decltype(DATALOG_PERIOD_IN_MAINS_CYCLES) >::type >::type n_cycleCountForDatalogging{ 0 }; /**< for counting how often datalog is updated */

uint8_t n_lowestNoOfSampleSetsPerMainsCycle{ 0 }; /**< For a mechanism to check the integrity of this code structure */

// For an enhanced polarity detection mechanism, which includes a persistence check
Polarities polarityOfMostRecentSampleV[NO_OF_PHASES]{};    /**< for zero-crossing detection */
Polarities polarityConfirmed[NO_OF_PHASES]{};              /**< for zero-crossing detection */
Polarities polarityConfirmedOfLastSampleV[NO_OF_PHASES]{}; /**< for zero-crossing detection */

LoadStates physicalLoadState[NO_OF_DUMPLOADS]{}; /**< Physical state of the loads */
uint16_t countLoadON[NO_OF_DUMPLOADS]{};         /**< Number of cycle the load was ON (over 1 datalog period) */

uint32_t absenceOfDivertedEnergyCountInMC{ 0 }; /**< number of main cycles without diverted energy */

uint8_t perSecondCounter{ 0 }; /**< for counting  every second inside the ISR */

bool beyondStartUpPeriod{ false }; /**< start-up delay, allows things to settle */

/**
 * @brief Initializes all elements of a given array to a specified value.
 *
 * This function is a compile-time constant expression (`constexpr`) that allows
 * initializing arrays of any size with a specific value. It is particularly useful
 * in embedded systems where predictable initialization is required.
 *
 * @tparam N The size of the array (deduced automatically).
 * @param array A reference to the array to be initialized.
 * @param value The value to assign to each element of the array.
 *
 * @note The function can be evaluated at compile time if all inputs are known
 *       at compile time.
 *
 * @ingroup Initialization
 */
template< size_t N >
constexpr void initializeArray(int32_t (&array)[N], int32_t value)
{
  for (size_t i = 0; i < N; ++i)
  {
    array[i] = value;
  }
}

/**
 * @brief Retrieves the output pins configuration.
 *
 * This function determines which pins are configured as output pins
 * based on the current hardware setup. It ensures that no pin is
 * configured multiple times and handles special cases like the watchdog pin.
 *
 * @return A 16-bit value representing the configured output pins.
 *         Returns 0 if an invalid configuration is detected.
 *
 * @note This function is marked as `constexpr` and can be evaluated at compile time.
 *
 * @ingroup Initialization
 */
constexpr uint16_t getOutputPins()
{
  uint16_t output_pins{ 0 };

  for (const auto &loadPin : physicalLoadPin)
  {
    if (bit_read(output_pins, loadPin))
      return 0;

    bit_set(output_pins, loadPin);
  }

  if constexpr (WATCHDOG_PIN_PRESENT)
  {
    if (bit_read(output_pins, watchDogPin))
      return 0;

    bit_set(output_pins, watchDogPin);
  }

  if constexpr (RELAY_DIVERSION)
  {
    for (uint8_t idx = 0; idx < relays.get_size(); ++idx)
    {
      const auto relayPin = relays.get_relay(idx).get_pin();

      if (bit_read(output_pins, relayPin))
        return 0;

      bit_set(output_pins, relayPin);
    }
  }

  return output_pins;
}

/**
 * @brief Retrieves the input pins configuration.
 *
 * This function determines which pins are configured as input pins
 * based on the current hardware setup. It ensures that no pin is
 * configured multiple times and handles special cases like the dual tariff,
 * diversion, rotation, and force pins.
 *
 * @return A 16-bit value representing the configured input pins.
 *         Returns 0 if an invalid configuration is detected.
 *
 * @note This function is marked as `constexpr` and can be evaluated at compile time.
 *
 * @ingroup Initialization
 */
constexpr uint16_t getInputPins()
{
  uint16_t input_pins{ 0 };

  if constexpr (DUAL_TARIFF)
  {
    if (bit_read(input_pins, dualTariffPin))
      return 0;

    bit_set(input_pins, dualTariffPin);
  }

  if constexpr (DIVERSION_PIN_PRESENT)
  {
    if (bit_read(input_pins, diversionPin))
      return 0;

    bit_set(input_pins, diversionPin);
  }

  if constexpr (PRIORITY_ROTATION == RotationModes::PIN)
  {
    if (bit_read(input_pins, rotationPin))
      return 0;

    bit_set(input_pins, rotationPin);
  }

  if constexpr (OVERRIDE_PIN_PRESENT)
  {
    if (bit_read(input_pins, forcePin))
      return 0;

    bit_set(input_pins, forcePin);
  }

  return input_pins;
}

/**
 * @brief Initializes the processing engine, including ports, load states, and ADC setup.
 *
 * This function performs the following tasks:
 * - Initializes the DC offset array for voltage samples.
 * - Configures the input and output pins based on the hardware setup.
 * - Sets up the ADC in free-running mode with interrupts enabled.
 * - Prepares the system for processing energy and load states.
 *
 * @note This function must be called during system initialization to ensure proper operation.
 *
 * @ingroup Initialization
 */
void initializeProcessing()
{
  initializeArray(l_DCoffset_V, 512L * 256L);  // nominal mid-point value of ADC @ x256 scale

  setPinsAsOutput(getOutputPins());      // set the output pins as OUTPUT
  setPinsAsInputPullup(getInputPins());  // set the input pins as INPUT_PULLUP

  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    loadPrioritiesAndState[i] = loadPrioritiesAtStartup[i];
    loadPrioritiesAndState[i] &= loadStateMask;
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
 * @brief Updates the control ports for each of the physical loads.
 *
 * This function determines the ON/OFF state of each physical load and updates
 * the corresponding control ports. It ensures that the correct pins are set
 * to their respective states based on the load's current status.
 *
 * @details
 * - If a load is OFF, its corresponding pin is added to the `pinsOFF` mask.
 * - If a load is ON, its corresponding pin is added to the `pinsON` mask.
 * - Finally, the pins are updated using `setPinsOFF` and `setPinsON` functions.
 *
 * @ingroup TimeCritical
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
 * @brief Updates the physical load states based on logical load priorities and states.
 *
 * This function maps the logical load states to the physical load states. It ensures
 * that the physical loads are updated according to the logical priorities and states.
 * The function also handles priority rotation if enabled.
 *
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
 * @note This function is called during each cycle to ensure the physical load states
 *       are synchronized with the logical load states.
 *
 * @ingroup TimeCritical
 */
void updatePhysicalLoadStates()
{
  if constexpr (PRIORITY_ROTATION != RotationModes::OFF)
  {
    if (Shared::b_reOrderLoads)
    {
      uint8_t i{ NO_OF_DUMPLOADS - 1 };
      const auto temp{ loadPrioritiesAndState[i] };
      do
      {
        loadPrioritiesAndState[i] = loadPrioritiesAndState[i - 1];
        --i;
      } while (i);
      loadPrioritiesAndState[0] = temp;

      Shared::b_reOrderLoads = false;
    }
  }

  const bool bDiversionEnabled{ Shared::b_diversionEnabled };
  uint8_t idx{ NO_OF_DUMPLOADS };
  do
  {
    --idx;
    const auto iLoad{ loadPrioritiesAndState[idx] & loadStateMask };
    physicalLoadState[iLoad] = bDiversionEnabled && (Shared::b_overrideLoadOn[iLoad] || (loadPrioritiesAndState[idx] & loadStateOnBit)) ? LoadStates::LOAD_ON : LoadStates::LOAD_OFF;
  } while (idx);
}

/**
 * @brief Processes the polarity of the current voltage sample for a specific phase.
 *
 * This function removes the DC offset from the raw voltage sample and determines
 * the polarity (positive or negative) of the sample. The polarity is stored for
 * use in zero-crossing detection and other processing tasks.
 *
 * @param phase The phase number [0..NO_OF_PHASES[.
 * @param rawSample The current raw voltage sample for the specified phase.
 *
 * @details
 * - The DC offset is subtracted from the raw sample using a low-pass filter (LPF).
 * - The polarity is determined based on whether the filtered sample is greater
 *   than or less than zero.
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
 * @brief Processes the current raw sample for the specified phase.
 *
 * This function processes the raw current sample for a specific phase by applying
 * filtering to remove DC offset, compensating for the high-pass filter effect of
 * current transformers (CTs), and calculating the instantaneous power.
 *
 * @param phase The phase number [0..NO_OF_PHASES[.
 * @param rawSample The current raw sample for the specified phase.
 *
 * @details
 * - The DC offset is removed from the raw sample.
 * - Additional filtering is applied to offset the high-pass filter effect of CTs.
 * - The instantaneous power is calculated and accumulated for the phase.
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
 * @brief Confirms the polarity of the current voltage sample for a specific phase.
 *
 * This routine prevents a zero-crossing point from being declared until a certain number
 * of consecutive samples in the 'other' half of the waveform have been encountered.
 *
 * @param phase The phase number [0..NO_OF_PHASES[.
 *
 * @details
 * - If the polarity of the most recent sample matches the last confirmed polarity,
 *   the persistence counter is reset.
 * - If the polarity differs, the persistence counter is incremented.
 * - Once the persistence counter exceeds a predefined threshold, the polarity is confirmed.
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
 * @brief Processes the current voltage sample for the specified phase.
 *
 * This function processes the voltage sample for a specific phase by calculating
 * the cumulative voltage squared (V²) for RMS calculations, updating the low-pass
 * filter for DC offset removal, and preparing for zero-crossing detection.
 *
 * @param phase The phase number [0..NO_OF_PHASES[.
 *
 * @details
 * - The voltage squared (V²) is calculated and accumulated for RMS calculations.
 * - The low-pass filter is updated to remove the DC offset from the voltage signal.
 * - The polarity of the last confirmed sample is stored for zero-crossing detection.
 * - The number of samples during the current mains cycle is incremented.
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
 * @brief Processes the startup period for the router.
 *
 * This function handles the initial startup period, allowing the DC-blocking filters
 * to settle before normal operation begins. It ensures that the system is stable
 * before processing energy and load states.
 *
 * @param phase The phase number [0..NO_OF_PHASES[.
 *
 * @details
 * - During the startup period, the function waits until the filters have settled.
 * - Once the startup period is over, it resets key variables and flags to prepare
 *   for normal operation.
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
 * @brief Handles the case when the energy level is high, potentially adding a load.
 *
 * This function determines if a new load can be added based on the current energy level
 * and recent transitions. It updates the logical load states and thresholds accordingly.
 *
 * @details
 * - Identifies the next logical load to be added.
 * - Ensures that only the active load can be switched during the post-transition period.
 * - Updates the upper energy threshold and logical load states if a load is added.
 *
 * @ingroup TimeCritical
 */
void proceedHighEnergyLevel()
{
  bool bOK_toAddLoad{ true };
  const auto tempLoad{ nextLogicalLoadToBeAdded() };

  if (tempLoad == NO_OF_DUMPLOADS)
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
 * @brief Handles the case when the energy level is low, potentially removing a load.
 *
 * This function determines if a load can be removed based on the current energy level
 * and recent transitions. It updates the logical load states and thresholds accordingly.
 *
 * @details
 * - Identifies the next logical load to be removed.
 * - Ensures that only the active load can be switched during the post-transition period.
 * - Updates the lower energy threshold and logical load states if a load is removed.
 *
 * @ingroup TimeCritical
 */
void proceedLowEnergyLevel()
{
  bool bOK_toRemoveLoad{ true };
  const auto tempLoad{ nextLogicalLoadToBeRemoved() };

  if (tempLoad == NO_OF_DUMPLOADS)
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
 * @brief Processes the start of a new mains cycle on phase 0.
 *
 * This function is executed once per 20ms (for 50Hz), shortly after the start of each
 * new mains cycle on phase 0. It manages the energy level and load states, ensuring
 * proper operation of the system.
 *
 * @details
 * - Handles recent transitions and updates the post-transition counter.
 * - Adjusts energy thresholds and determines whether to add or remove loads.
 * - Updates the physical load states and control ports.
 * - Ensures the energy bucket level remains within defined limits.
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

  if (loadPrioritiesAndState[0] & loadStateOnBit)
  {
    absenceOfDivertedEnergyCountInMC = 0;
  }
  else
  {
    ++absenceOfDivertedEnergyCountInMC;
  }

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
 * @brief Processes the start of a new negative half cycle for the specified phase.
 *
 * This function is called just after the zero-crossing point of a negative half cycle.
 * It updates the low-pass filter (LPF) for removing the DC component from the voltage
 * signal and ensures the LPF output remains within defined limits.
 *
 * @param phase The phase number [0..NO_OF_PHASES[.
 *
 * @details
 * - Updates the low-pass filter for DC offset removal using the cumulative voltage deltas.
 * - Ensures the LPF output remains within the defined minimum and maximum range.
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

/**
 * @brief Retrieve the next logical load that could be added.
 *
 * This function identifies the next logical load that can be added based on the
 * current load priorities and states. It iterates through the load priorities
 * to find the first load that is currently OFF.
 *
 * @return The load number if a suitable load is found, or `NO_OF_DUMPLOADS` if no
 *         load can be added.
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

/**
 * @brief Retrieve the next logical load that could be removed (in reverse order).
 *
 * This function identifies the next logical load that can be removed based on the
 * current load priorities and states. It iterates through the load priorities
 * in reverse order to find the first load that is currently ON.
 *
 * @return The load number if a suitable load is found, or `NO_OF_DUMPLOADS` if no
 *         load can be removed.
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
 * @brief Process the latest contribution after each phase-specific new cycle.
 *
 * This function calculates and updates the energy contribution for the specified phase
 * after each new cycle. It ensures that the energy bucket is updated with the latest
 * power measurements and applies necessary adjustments.
 *
 * @param phase The phase number [0..NO_OF_PHASES[.
 *
 * @details
 * - Adds the latest energy contribution to the main energy bucket.
 * - Applies adjustments for required export energy on phase 0.
 * - Signals a new mains cycle for phase 0.
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
    // If diversion hasn't started yet, use start threshold, otherwise use regular offset
    if (!b_diversionStarted)
    {
      f_energyInBucket_main -= DIVERSION_START_THRESHOLD_WATTS;

      // Check if we've exceeded the threshold to start diversion
      if (f_energyInBucket_main > f_upperThreshold_default)
      {
        b_diversionStarted = true;
        // Once started, we divert all surplus according to the configured fixed offset
      }
    }
    else
    {
      // When diversion is already started, apply normal export offset if configured
      // Comment or remove this if you want to divert ALL surplus once started
      f_energyInBucket_main -= REQUIRED_EXPORT_IN_WATTS;
    }

    if (++perSecondCounter == SUPPLY_FREQUENCY)
    {
      perSecondCounter = 0;

      if (absenceOfDivertedEnergyCountInMC > SUPPLY_FREQUENCY)
      {
        ++Shared::absenceOfDivertedEnergyCountInSeconds;
        // Reset diversion state if we've had no diversion for a full second
        b_diversionStarted = false;
      }
      else
        Shared::absenceOfDivertedEnergyCountInSeconds = 0;
    }

    Shared::b_newMainsCycle = true;  //  a 50 Hz 'tick' for use by the main code
  }
  // Applying max and min limits to the main accumulator's level
  // is deferred until after the energy related decisions have been taken
  //
}

/**
 * @brief Process data logging at the end of each logging period.
 *
 * This function handles the data logging process by copying relevant variables
 * for use by the main code and resetting them for the next logging period.
 *
 * @details
 * - Copies cumulative power and voltage squared values for each phase.
 * - Copies load ON counts and other diagnostic variables.
 * - Resets variables for the next data logging period.
 * - Signals the main processor that logging data is available after the startup period.
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
    Shared::copyOf_sumP_atSupplyPoint[phase] = l_sumP_atSupplyPoint[phase];
    l_sumP_atSupplyPoint[phase] = 0;

    Shared::copyOf_sum_Vsquared[phase] = l_sum_Vsquared[phase];
    l_sum_Vsquared[phase] = 0;
  } while (phase);

  uint8_t i{ NO_OF_DUMPLOADS };
  do
  {
    --i;
    Shared::copyOf_countLoadON[i] = countLoadON[i];
    countLoadON[i] = 0;
  } while (i);

  Shared::copyOf_sampleSetsDuringThisDatalogPeriod = i_sampleSetsDuringThisDatalogPeriod;  // (for diags only)
  Shared::copyOf_lowestNoOfSampleSetsPerMainsCycle = n_lowestNoOfSampleSetsPerMainsCycle;  // (for diags only)
  Shared::copyOf_energyInBucket_main = f_energyInBucket_main;                              // (for diags only)

  n_lowestNoOfSampleSetsPerMainsCycle = UINT8_MAX;
  i_sampleSetsDuringThisDatalogPeriod = 0;

  // signal the main processor that logging data are available
  // we skip the period from start to running stable
  Shared::b_datalogEventPending = beyondStartUpPeriod;
}

/**
 * @brief Process the start of a new positive half cycle for the specified phase.
 *
 * This function is called just after the zero-crossing point of a positive half cycle.
 * It processes the latest energy contribution, updates performance metrics, and handles
 * data logging for phase 0.
 *
 * @param phase The phase number [0..NO_OF_PHASES[.
 *
 * @details
 * - Processes the latest energy contribution for the specified phase.
 * - Updates the minimum number of ADC sample sets per mains cycle for phase 0.
 * - Handles data logging at the end of the logging period for phase 0.
 * - Resets cumulative power and sample count for the phase.
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
 * @brief Processes raw voltage and current samples for the specified phase.
 *
 * This routine is called by the ISR when a pair of voltage and current samples
 * becomes available. It handles the processing of raw samples, including polarity
 * detection, zero-crossing handling, and half-cycle processing.
 *
 * @param phase The phase number [0..NO_OF_PHASES[.
 *
 * @details
 * - Determines the polarity of the current sample and handles transitions between
 *   positive and negative half cycles.
 * - Processes the start of new positive and negative half cycles.
 * - For phase 0, it triggers the start of a new mains cycle and handles startup logic.
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
 * @brief Processes the current voltage raw sample for the specified phase.
 *
 * This function processes the raw voltage sample for a specific phase by handling
 * polarity detection, zero-crossing confirmation, and voltage processing. It ensures
 * that the voltage sample is properly filtered and analyzed for further processing.
 *
 * @param phase The phase number [0..NO_OF_PHASES[.
 * @param rawSample The current raw voltage sample for the specified phase.
 *
 * @details
 * - Determines and confirms the polarity of the raw voltage sample.
 * - Handles zero-crossing detection and processes raw samples for the phase.
 * - Updates voltage-related metrics and increments the sample set count for phase 0.
 *
 * @ingroup TimeCritical
 */
void processVoltageRawSample(const uint8_t phase, const int16_t rawSample)
{
  processPolarity(phase, rawSample);
  confirmPolarity(phase);

  processRawSamples(phase);  // deals with aspects that only occur at particular stages of each mains cycle

  processVoltage(phase);

  if (phase == 0)
  {
    ++i_sampleSetsDuringThisDatalogPeriod;
  }
}

/**
 * @brief Print the settings used for the selected output mode.
 *
 * This function displays the relevant configuration parameters for the currently
 * selected output mode. It provides details about the energy bucket capacity,
 * thresholds, and mode-specific settings.
 *
 * @details
 * - For the "normal" mode, it displays the energy bucket capacity and thresholds.
 * - For the "anti-flicker" mode, it also displays the offset of energy thresholds.
 *
 * @ingroup Debugging
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
