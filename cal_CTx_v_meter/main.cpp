
static_assert(__cplusplus >= 201703L, "**** Please define 'gnu++17' in 'platform.txt' ! ****");
static_assert(__cplusplus >= 201703L, "See also : https://github.com/FredM67/PVRouter-3-phase/blob/Laclare/Mk2_3phase_RFdatalog_temp/Readme.md");

#include <Arduino.h>  // may not be needed, but it's probably a good idea to include this

#include "main.h"

// In this sketch, the ADC is free-running with a cycle time of ~104uS.

// -----------------------------------------------------
// Change these values to suit the local mains frequency and supply meter
constexpr uint8_t SUPPLY_FREQUENCY{ 50 };            /**< number of cycles/s of the grid power supply */
constexpr uint32_t WORKING_ZONE_IN_JOULES{ 3600UL }; /**< number of joule for 1Wh */

// ----------------
// general literals
constexpr uint16_t DATALOG_PERIOD_IN_MAINS_CYCLES{ 250 }; /**< Period of datalogging in cycles */

constexpr uint8_t NO_OF_PHASES{ 3 }; /**< number of phases of the main supply. */

// -------------------------------
// definitions of enumerated types

/** Polarities */
enum class Polarities : uint8_t
{
  NEGATIVE, /**< polarity is negative */
  POSITIVE  /**< polarity is positive */
};

/** @brief container for datalogging
    @details This class is used for datalogging.
*/
class Tx_struct
{
public:
  int16_t power;                     /**< main power, import = +ve, to match OEM convention */
  int16_t power_L[NO_OF_PHASES];     /**< power for phase #, import = +ve, to match OEM convention */
  int16_t Vrms_L_x100[NO_OF_PHASES]; /**< average voltage over datalogging period (in 100th of Volt)*/
};

Tx_struct tx_data; /**< logging data */

// ----------- Pinout assignments  -----------
//
// analogue input pins
constexpr uint8_t sensorV[NO_OF_PHASES]{ 0, 2, 4 }; /**< for 3-phase PCB, voltage measurement for each phase */
constexpr uint8_t sensorI[NO_OF_PHASES]{ 1, 3, 5 }; /**< for 3-phase PCB, current measurement for each phase */

// --------------  general global variables -----------------
//
// Some of these variables are used in multiple blocks so cannot be static.
// For integer maths, some variables need to be 'int32_t'
//
bool beyondStartUpPeriod{ false };        /**< start-up delay, allows things to settle */
constexpr uint32_t initialDelay{ 3000 };  /**< in milli-seconds, to allow time to open the Serial monitor */
constexpr uint32_t startUpPeriod{ 3000 }; /**< in milli-seconds, to allow LP filter to settle */

int32_t l_DCoffset_V[NO_OF_PHASES]; /**< <--- for LPF */

// Define operating limits for the LP filters which identify DC offset in the voltage
// sample streams. By limiting the output range, these filters always should start up
// correctly.
constexpr int32_t l_DCoffset_V_min{ (512L - 100L) << 8 }; /**< mid-point of ADC minus a working margin */
constexpr int32_t l_DCoffset_V_max{ (512L + 100L) << 8 }; /**< mid-point of ADC plus a working margin */
constexpr int16_t i_DCoffset_I_nom{ 512L };               /**< nominal mid-point value of ADC @ x1 scale */

/**< main energy bucket for 3-phase use, with units of Joules * SUPPLY_FREQUENCY */
constexpr float f_capacityOfEnergyBucket_main{ (float)(WORKING_ZONE_IN_JOULES * SUPPLY_FREQUENCY) };

float f_energyInBucket_main{ 0 }; /**< main energy bucket (over all phases) */
float f_lowerEnergyThreshold;     /**< dynamic lower threshold */
float f_upperEnergyThreshold;     /**< dynamic upper threshold */

int32_t l_sumP[NO_OF_PHASES];                /**< cumulative power per phase */
int32_t l_sampleVminusDC[NO_OF_PHASES];      /**< for the phaseCal algorithm */
int32_t l_lastSampleVminusDC[NO_OF_PHASES];  /**< for the phaseCal algorithm */
int32_t l_cumVdeltasThisCycle[NO_OF_PHASES]; /**< for the LPF which determines DC offset (voltage) */
int32_t l_sumP_atSupplyPoint[NO_OF_PHASES];  /**< for summation of 'real power' values during datalog period */
int32_t l_sum_Vsquared[NO_OF_PHASES];        /**< for summation of V^2 values during datalog period */

uint8_t n_samplesDuringThisMainsCycle[NO_OF_PHASES]; /**< number of sample sets for each phase during each mains cycle */
uint16_t n_sampleSetsDuringThisDatalogPeriod;        /**< number of sample sets during each datalogging period */
uint16_t n_cycleCountForDatalogging{ 0 };            /**< for counting how often datalog is updated */

// For a mechanism to check the integrity of this code structure
uint8_t n_lowestNoOfSampleSetsPerMainsCycle;

// for interaction between the main processor and the ISR
volatile bool b_datalogEventPending{ false }; /**< async trigger to signal datalog is available */
volatile bool b_newMainsCycle{ false };       /**< async trigger to signal start of new main cycle based on first phase */

// Since there's no real locking feature for shared variables, a couple of data
// generated from inside the ISR are copied from time to time to be passed to the
// main processor. When the data are available, the ISR signals it to the main processor.
volatile int32_t copyOf_sumP_atSupplyPoint[NO_OF_PHASES];   /**< copy of cumulative power per phase */
volatile int32_t copyOf_sum_Vsquared[NO_OF_PHASES];         /**< copy of for summation of V^2 values during datalog period */
volatile float copyOf_energyInBucket_main;                  /**< copy of main energy bucket (over all phases) */
volatile uint8_t copyOf_lowestNoOfSampleSetsPerMainsCycle;  /**<  */
volatile uint16_t copyOf_sampleSetsDuringThisDatalogPeriod; /**< copy of for counting the sample sets during each datalogging period */

// For an enhanced polarity detection mechanism, which includes a persistence check
constexpr uint8_t PERSISTENCE_FOR_POLARITY_CHANGE{ 2 };  /**< allows polarity changes to be confirmed */
Polarities polarityOfMostRecentVsample[NO_OF_PHASES];    /**< for zero-crossing detection */
Polarities polarityConfirmed[NO_OF_PHASES];              /**< for zero-crossing detection */
Polarities polarityConfirmedOfLastSampleV[NO_OF_PHASES]; /**< for zero-crossing detection */

// Calibration values
//-------------------
// Three calibration values are used in this sketch: f_powerCal, f_phaseCal and f_voltageCal.
// With most hardware, the default values are likely to work fine without
// need for change. A compact explanation of each of these values now follows:

// When calculating real power, which is what this code does, the individual
// conversion rates for voltage and current are not of importance. It is
// only the conversion rate for POWER which is important. This is the
// product of the individual conversion rates for voltage and current. It
// therefore has the units of ADC-steps squared per Watt. Most systems will
// have a power conversion rate of around 20 (ADC-steps squared per Watt).
//
// powerCal is the RECIPR0CAL of the power conversion rate. A good value
// to start with is therefore 1/20 = 0.05 (Watts per ADC-step squared)
//
inline constexpr float f_powerCal[NO_OF_PHASES]{ 0.04504F, 0.04535F, 0.04510F };

// f_phaseCal is used to alter the phase of the voltage waveform relative to the
// current waveform. The algorithm interpolates between the most recent pair
// of voltage samples according to the value of f_phaseCal.
//
//    With f_phaseCal = 1, the most recent sample is used.
//    With f_phaseCal = 0, the previous sample is used
//    With f_phaseCal = 0.5, the mid-point (average) value in used
//
// NB. Any tool which determines the optimal value of f_phaseCal must have a similar
// scheme for taking sample values as does this sketch.
//
constexpr float f_phaseCal{ 1.0F }; /**< Nominal values only */
// When using integer maths, calibration values that have been supplied in
// floating point form need to be rescaled.
constexpr int16_t i_phaseCal{ 256 }; /**< to avoid the need for floating-point maths (f_phaseCal * 256) */

// For datalogging purposes, f_voltageCal has been added too. Because the range of ADC values is
// similar to the actual range of volts, the optimal value for this cal factor is likely to be
// close to unity.
inline constexpr float f_voltageCal[NO_OF_PHASES]{ 0.8151F, 0.8184F, 0.8195F }; /**< compared with Sentron PAC 4200 */

/**
   @brief Interrupt Service Routine - Interrupt-Driven Analog Conversion.
   @details An Interrupt Service Routine is now defined which instructs the ADC to perform a conversion
            for each of the voltage and current sensors in turn.

            This Interrupt Service Routine is for use when the ADC is in the free-running mode.
            It is executed whenever an ADC conversion has finished, approx every 104 µs. In
            free-running mode, the ADC has already started its next conversion by the time that
            the ISR is executed. The ISR therefore needs to "look ahead".

            At the end of conversion Type N, conversion Type N+1 will start automatically. The ISR
            which runs at this point therefore needs to capture the results of conversion Type N,
            and set up the conditions for conversion Type N+2, and so on.

            By means of various helper functions, all of the time-critical activities are processed
            within the ISR.

            The main code is notified by means of a flag when fresh copies of loggable data are available.

            Keep in mind, when writing an Interrupt Service Routine (ISR):
              - Keep it short
              - Don't use delay ()
              - Don't do serial prints
              - Make variables shared with the main code volatile
              - Variables shared with main code may need to be protected by "critical sections"
              - Don't try to turn interrupts off or on

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

/* -----------------------------------------------------------
   Start of various helper functions which are used by the ISR
*/

/*!
   @defgroup TimeCritical Time critical functions Group
   Functions used by the ISR
*/

/**
   @brief Process the calculation for the actual current raw sample for the specific phase

   @param phase the phase number [0..NO_OF_PHASES[
   @param rawSample the current sample for the specified phase

   @ingroup TimeCritical
*/
void processCurrentRawSample(const uint8_t phase, const int16_t rawSample)
{
  // remove most of the DC offset from the current sample (the precise value does not matter)
  const int32_t sampleIminusDC = (static_cast< int32_t >(rawSample - i_DCoffset_I_nom)) << 8;
  //
  // phase-shift the voltage waveform so that it aligns with the grid current waveform
  const int32_t phaseShiftedSampleVminusDC = l_lastSampleVminusDC[phase] + (((l_sampleVminusDC[phase] - l_lastSampleVminusDC[phase]) * i_phaseCal) >> 8);
  //
  // calculate the "real power" in this sample pair and add to the accumulated sum
  const int32_t filtV_div4 = phaseShiftedSampleVminusDC >> 2;  // reduce to 16-bits (now x64, or 2^6)
  const int32_t filtI_div4 = sampleIminusDC >> 2;              // reduce to 16-bits (now x64, or 2^6)
  int32_t instP = filtV_div4 * filtI_div4;                     // 32-bits (now x4096, or 2^12)
  instP >>= 12;                                                // scaling is now x1, as for Mk2 (V_ADC x I_ADC)

  l_sumP[phase] += instP;                // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
  l_sumP_atSupplyPoint[phase] += instP;  // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
}

/**
   @brief Process the current voltage raw sample for the specific phase

   @param phase the phase number [0..NO_OF_PHASES[
   @param rawSample the current sample for the specified phase

   @ingroup TimeCritical
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
    ++n_sampleSetsDuringThisDatalogPeriod;
}

/**
   @brief Process with the polarity for the actual voltage sample for the specific phase

   @param phase the phase number [0..NO_OF_PHASES[
   @param rawSample the current sample for the specified phase

   @ingroup TimeCritical
*/
void processPolarity(const uint8_t phase, const int16_t rawSample)
{
  l_lastSampleVminusDC[phase] = l_sampleVminusDC[phase];  // required for phaseCal algorithm
  // remove DC offset from each raw voltage sample by subtracting the accurate value
  // as determined by its associated LP filter.
  l_sampleVminusDC[phase] = (static_cast< int32_t >(rawSample) << 8) - l_DCoffset_V[phase];
  polarityOfMostRecentVsample[phase] = (l_sampleVminusDC[phase] > 0) ? Polarities::POSITIVE : Polarities::NEGATIVE;
}

/**
   @brief This routine prevents a zero-crossing point from being declared until a certain number
          of consecutive samples in the 'other' half of the waveform have been encountered.

   @param phase the phase number [0..NO_OF_PHASES[

   @ingroup TimeCritical
*/
void confirmPolarity(const uint8_t phase)
{
  static uint8_t count[NO_OF_PHASES]{};

  if (polarityOfMostRecentVsample[phase] != polarityConfirmedOfLastSampleV[phase])
    ++count[phase];
  else
    count[phase] = 0;

  if (count[phase] > PERSISTENCE_FOR_POLARITY_CHANGE)
  {
    count[phase] = 0;
    polarityConfirmed[phase] = polarityOfMostRecentVsample[phase];
  }
}

/**
   @brief Process the calculation for the current voltage sample for the specific phase

   @param phase the phase number [0..NO_OF_PHASES[

   @ingroup TimeCritical
*/
void processVoltage(const uint8_t phase)
{
  // for the Vrms calculation (for datalogging only)
  int32_t filtV_div4 = l_sampleVminusDC[phase] >> 2;  // reduce to 16-bits (now x64, or 2^6)
  int32_t inst_Vsquared = filtV_div4 * filtV_div4;    // 32-bits (now x4096, or 2^12)
  inst_Vsquared >>= 12;                               // scaling is now x1 (V_ADC x I_ADC)
  l_sum_Vsquared[phase] += inst_Vsquared;             // cumulative V^2 (V_ADC x I_ADC)
  //
  // store items for use during next loop
  l_cumVdeltasThisCycle[phase] += l_sampleVminusDC[phase];           // for use with LP filter
  polarityConfirmedOfLastSampleV[phase] = polarityConfirmed[phase];  // for identification of half cycle boundaries
  ++n_samplesDuringThisMainsCycle[phase];                            // for real power calculations
}

/**
   @brief This routine is called by the ISR when a pair of V & I sample becomes available.

   @param phase the phase number [0..NO_OF_PHASES[

   @ingroup TimeCritical
*/
void processRawSamples(const uint8_t phase)
{
  // The raw V and I samples are processed in "phase pairs"
  if (Polarities::POSITIVE == polarityConfirmed[phase])
  {
    // the polarity of this sample is positive
    if (Polarities::POSITIVE != polarityConfirmedOfLastSampleV[phase])
    {
      if (beyondStartUpPeriod)
      {
        // This is the start of a new +ve half cycle, for this phase, just after the zero-crossing point.
        processPlusHalfCycle(phase);
      }
      else
        processStartUp(phase);
    }

    // still processing samples where the voltage is POSITIVE ...
    // check to see whether the trigger device can now be reliably armed
    if (beyondStartUpPeriod && (phase == 0) && (2 == n_samplesDuringThisMainsCycle[0]))  // lower value for larger sample set
    {
      // This code is executed once per 20mS, shortly after the start of each new mains cycle on phase 0.
      processStartNewCycle();
    }
  }
  else
  {
    // the polarity of this sample is negative
    if (Polarities::NEGATIVE != polarityConfirmedOfLastSampleV[phase])
    {
      // This is the start of a new -ve half cycle (just after the zero-crossing point)
      processMinusHalfCycle(phase);
    }
  }
}
// end of processRawSamples()

/**
   @brief Process the startup period for the router.

   @param phase the phase number [0..NO_OF_PHASES[

   @ingroup TimeCritical
*/
void processStartUp(const uint8_t phase)
{
  // wait until the DC-blocking filters have had time to settle
  if (millis() <= (initialDelay + startUpPeriod))
    return;  // still settling, do nothing

  // the DC-blocking filters have had time to settle
  beyondStartUpPeriod = true;
  l_sumP[phase] = 0;
  l_sumP_atSupplyPoint[phase] = 0;
  n_samplesDuringThisMainsCycle[phase] = 0;
  n_sampleSetsDuringThisDatalogPeriod = 0;

  n_lowestNoOfSampleSetsPerMainsCycle = UINT8_MAX;
  // can't say "Go!" here 'cos we're in an ISR!
}

/**
   @brief This code is executed once per 20mS, shortly after the start of each new
          mains cycle on phase 0.
   @details Changing the state of the loads  is a 3-part process:
            - change the LOGICAL load states as necessary to maintain the energy level
            - update the PHYSICAL load states according to the logical -> physical mapping
            - update the driver lines for each of the loads.

   @ingroup TimeCritical
*/
void processStartNewCycle()
{
  // When operating as a cal program
  if (f_energyInBucket_main > f_capacityOfEnergyBucket_main)
  {
    f_energyInBucket_main -= f_capacityOfEnergyBucket_main;
    // registerConsumedPower();
  }

  if (f_energyInBucket_main < 0)
    f_energyInBucket_main = 0;
}

/**
   @brief Process the start of a new -ve half cycle, for this phase, just after the zero-crossing point.

   @param phase the phase number [0..NO_OF_PHASES[

   @ingroup TimeCritical
*/
void processMinusHalfCycle(const uint8_t phase)
{
  // This is a convenient point to update the Low Pass Filter for removing the DC
  // component from the phase that is being processed.
  // The portion which is fed back into the integrator is approximately one percent
  // of the average offset of all the Vsamples in the previous mains cycle.
  //
  l_DCoffset_V[phase] += (l_cumVdeltasThisCycle[phase] >> 12);
  l_cumVdeltasThisCycle[phase] = 0;

  // To ensure that this LP filter will always start up correctly when 240V AC is
  // available, its output value needs to be prevented from drifting beyond the likely range
  // of the voltage signal.
  //
  if (l_DCoffset_V[phase] < l_DCoffset_V_min)
    l_DCoffset_V[phase] = l_DCoffset_V_min;
  else if (l_DCoffset_V[phase] > l_DCoffset_V_max)
    l_DCoffset_V[phase] = l_DCoffset_V_max;
}

/**
   @brief Process the lastest contribution after each phase specific new cycle
          additional processing is performed after each main cycle based on phase 0.

   @ingroup TimeCritical
*/
void processLatestContribution(const uint8_t phase)
{
  // for efficiency, the energy scale is Joules * SUPPLY_FREQUENCY
  // add the latest energy contribution to the main energy accumulator
  f_energyInBucket_main += (l_sumP[phase] / n_samplesDuringThisMainsCycle[phase]) * f_powerCal[phase];

  // Applying max and min limits to the main accumulator's level
  // is deferred until after the energy related decisions have been taken
  //
}

/**
   @brief Process the start of a new +ve half cycle, for this phase, just after the zero-crossing point.

   @ingroup TimeCritical
*/
void processPlusHalfCycle(const uint8_t phase)
{
  processLatestContribution(phase);  // runs at 6.6 ms intervals

  // A performance check to monitor and display the minimum number of sets of
  // ADC samples per mains cycle, the expected number being 20ms / (104us * 6) = 32.05
  //
  if (0 == phase)
  {
    b_newMainsCycle = true;  //  a 50 Hz 'tick' for use by the main code

    if (n_samplesDuringThisMainsCycle[phase] < n_lowestNoOfSampleSetsPerMainsCycle)
      n_lowestNoOfSampleSetsPerMainsCycle = n_samplesDuringThisMainsCycle[phase];

    processDataLogging();
  }

  l_sumP[phase] = 0;
  n_samplesDuringThisMainsCycle[phase] = 0;
}

/**
   @brief Process with data logging.
   @details At the end of each datalogging period, copies are made of the relevant variables
            for use by the main code. These variable are then reset for use during the next
            datalogging period.

   @ingroup TimeCritical
*/
void processDataLogging()
{
  if (++n_cycleCountForDatalogging < DATALOG_PERIOD_IN_MAINS_CYCLES)
    return;  // data logging period not yet reached

  n_cycleCountForDatalogging = 0;

  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    copyOf_sumP_atSupplyPoint[phase] = l_sumP_atSupplyPoint[phase];
    l_sumP_atSupplyPoint[phase] = 0;

    copyOf_sum_Vsquared[phase] = l_sum_Vsquared[phase];
    l_sum_Vsquared[phase] = 0;
  }

  copyOf_sampleSetsDuringThisDatalogPeriod = n_sampleSetsDuringThisDatalogPeriod;  // (for diags only)
  copyOf_lowestNoOfSampleSetsPerMainsCycle = n_lowestNoOfSampleSetsPerMainsCycle;  // (for diags only)
  copyOf_energyInBucket_main = f_energyInBucket_main;                              // (for diags only)

  n_lowestNoOfSampleSetsPerMainsCycle = UINT8_MAX;
  n_sampleSetsDuringThisDatalogPeriod = 0;

  // signal the main processor that logging data are available
  b_datalogEventPending = true;
}
/* End of helper functions which are used by the ISR
   -------------------------------------------------
*/

/**
   @brief Prints data logs to the Serial output in text or json format

   @param bOffPeak true if off-peak tariff is active
*/
void printDataLogging()
{
  uint8_t phase;

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

  Serial.print(F(", (minSampleSets/MC "));
  Serial.print(copyOf_lowestNoOfSampleSetsPerMainsCycle);
  Serial.print(F(", #ofSampleSets "));
  Serial.print(copyOf_sampleSetsDuringThisDatalogPeriod);
  Serial.println(F(")"));
}

/**
   @brief Print the configuration during start

*/
void printConfiguration()
{
  Serial.println();
  Serial.println();
  Serial.println(F("----------------------------------"));
  Serial.print(F("Sketch ID: "));
  Serial.println(__FILE__);
  Serial.print(F("Build on "));
  Serial.print(__DATE__);
  Serial.print(F(" "));
  Serial.println(__TIME__);

  Serial.println(F("ADC mode:       free-running"));

  Serial.println(F("Electrical settings"));
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    Serial.print(F("\tf_powerCal for L"));
    Serial.print(phase + 1);
    Serial.print(F(" =    "));
    Serial.println(f_powerCal[phase], 5);

    Serial.print(F("\tf_voltageCal, for Vrms  =      "));
    Serial.println(f_voltageCal[phase], 5);
  }
  Serial.print(F("\tf_phaseCal for all phases"));
  Serial.print(F(" =     "));
  Serial.println(f_phaseCal);

  Serial.print(F("\tzero-crossing persistence (sample sets) = "));
  Serial.println(PERSISTENCE_FOR_POLARITY_CHANGE);
}

int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

/**
   @brief Called once during startup.
   @details This function initializes a couple of variables we cannot init at compile time and
            sets a couple of parameters for runtime.

*/
void setup()
{
  delay(initialDelay);  // allows time to open the Serial Monitor

  Serial.begin(9600);  // initialize Serial interface

  // On start, always display config info in the serial monitor
  printConfiguration();

  for (auto &DCoffset_V : l_DCoffset_V)
    DCoffset_V = 512L * 256L;  // nominal mid-point value of ADC @ x256 scale

  // Set up the ADC to be free-running
  ADCSRA = bit(ADPS0) + bit(ADPS1) + bit(ADPS2);  // Set the ADC's clock to system clock / 128
  ADCSRA |= bit(ADEN);                            // Enable the ADC

  ADCSRA |= bit(ADATE);  // set the Auto Trigger Enable bit in the ADCSRA register. Because
  // bits ADTS0-2 have not been set (i.e. they are all zero), the
  // ADC's trigger source is set to "free running mode".

  ADCSRA |= bit(ADIE);  // set the ADC interrupt enable bit. When this bit is written
  // to one and the I-bit in SREG is set, the
  // ADC Conversion Complete Interrupt is activated.

  ADCSRA |= bit(ADSC);  // start ADC manually first time
  sei();                // Enable Global Interrupts

  Serial.print(F(">>free RAM = "));
  Serial.println(freeRam());  // a useful value to keep an eye on
  Serial.println(F("----"));
}

/**
   @brief Main processor.
   @details None of the workload in loop() is time-critical.
            All the processing of ADC data is done within the ISR.

*/
void loop()
{
  static uint8_t perSecondTimer{ 0 };

  if (b_newMainsCycle)  // flag is set after every pair of ADC conversions
  {
    b_newMainsCycle = false;  // reset the flag
    ++perSecondTimer;

    if (perSecondTimer >= SUPPLY_FREQUENCY)
      perSecondTimer = 0;
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

    printDataLogging();
  }
}  // end of loop()
