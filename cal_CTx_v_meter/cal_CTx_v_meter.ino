/* cal_CTx_v_meter.ino

   September 2019
   This calibration sketch is based on cal_CT1_v_meter.ino.
   It is intended to be used with the 3-phase rev 2 PCB.
   Each CT/phase can be calibrated be setting the appropriate 'CURRENT_CAL_PHASE'.
        Fred Metrich

   February 2018
   This calibration sketch is based on Mk2_bothDisplays_4.ino. Its purpose is to
   mimic the behaviour of a digital electricity meter.

   CT1 should be clipped around one of the live cables that pass through the
   meter. The energy flow measured by CT1 is noted and a short pulse is generated
   whenever a pre-set amount of energy has been recorded (normally 3600J).

   This stream of pulses can then be compared against optical pulses from a standard
   electrical utility meter. The pulse rate can be varied by adjusting the value
   of powerCal_grid. When the two streams of pulses are in synch, correct calibration
   of the CT1 channel has been achieved.

        Robin Emley
        www.Mk2PVrouter.co.uk
*/

#include <Arduino.h>

// Change this value to suit the local mains frequency
#define CYCLES_PER_SECOND 50

// Change this value to suit the electricity meter's Joules-per-flash rate (3600J = 1Wh).
#define ENERGY_BUCKET_CAPACITY_IN_JOULES 3600

constexpr int32_t DATALOG_PERIOD_IN_MAINS_CYCLES{250}; /**< Period of datalogging in cycles */

// Modification for 3-phase PCB (Fred)
// ***************************
#define NO_OF_PHASES 3      // nb of phases
#define CURRENT_CAL_PHASE 2 // current phase/CT to be calibrated (L1-->0, L2-->1,L3-->2)

// definition of enumerated types

/** Polarities */
enum class Polarities : uint8_t
{
  NEGATIVE, /**< polarity is negative */
  POSITIVE  /**< polarity is positive */
};

enum LED_states
{
  LED_ON,
  LED_OFF
}; // active low for use at the "trigger" port which is active low

// allocation of digital pins
// **************************
const byte outputForLED = 4; // <-- the "trigger" port is active-low

// allocation of analogue pins
// ***************************
const byte sensorV[NO_OF_PHASES] = {0, 2, 4}; // Voltage Sensors for 3-phase PCB
const byte sensorI[NO_OF_PHASES] = {1, 3, 5}; // Current Sensors for 3-phase PCB

uint32_t cycleCount{0};                      // used to time LED events, rather than calling millis()
int32_t l_samplesDuringThisMainsCycle;       /**< number of sample sets for each phase during each mains cycle */
int32_t l_sampleSetsDuringThisDatalogPeriod; /**< number of sample sets during each datalogging period */
int32_t l_cycleCountForDatalogging{0};       /**< for counting how often datalog is updated */

// General global variables that are used in multiple blocks so cannot be static.
// For integer maths, many variables need to be 'long'

bool beyondStartUpPeriod{false};        /**< start-up delay, allows things to settle */
constexpr uint32_t initialDelay{3000};  /**< in milli-seconds, to allow time to open the Serial monitor */
constexpr uint32_t startUpPeriod{3000}; /**< in milli-seconds, to allow LP filter to settle */

int32_t l_sumP{0};                   // for per-cycle summation of 'real power'
int32_t energyInBucket_long;         // in Integer Energy Units
int32_t capacityOfEnergyBucket_long; // depends on powerCal, frequency & the 'sweetzone' size.
int32_t l_cumVdeltasThisCycle;       /**< for the LPF which determines DC offset (voltage) */
int32_t l_sumP_atSupplyPoint;        /**< for summation of 'real power' values during datalog period */
int32_t l_sum_Vsquared;              /**< for summation of V^2 values during datalog period */

int32_t l_DCoffset_V; /**< <--- for LPF */

// Define operating limits for the LP filters which identify DC offset in the voltage
// sample streams. By limiting the output range, these filters always should start up
// correctly.
constexpr int32_t l_DCoffset_V_min{(512L - 100L) * 256L}; /**< mid-point of ADC minus a working margin */
constexpr int32_t l_DCoffset_V_max{(512L + 100L) * 256L}; /**< mid-point of ADC plus a working margin */
constexpr int16_t i_DCoffset_I_nom{512L};                 /**< nominal mid-point value of ADC @ x1 scale */

int32_t l_sampleVminusDC;     /**< for the phaseCal algorithm */
int32_t l_lastSampleVminusDC; /**< for the phaseCal algorithm */

// For an enhanced polarity detection mechanism, which includes a persistence check
constexpr uint8_t PERSISTENCE_FOR_POLARITY_CHANGE{2}; /**< allows polarity changes to be confirmed */
Polarities polarityOfMostRecentVsample;
Polarities polarityConfirmed;
Polarities polarityConfirmedOfLastSampleV;

// For a mechanism to check the continuity of the sampling sequence
#define CONTINUITY_CHECK_MAXCOUNT 250 // mains cycles
int sampleCount_forContinuityChecker;
int sampleSetsDuringThisMainsCycle;
int lowestNoOfSampleSetsPerMainsCycle;

// For a mechanism to check the integrity of this code structure
int32_t l_lowestNoOfSampleSetsPerMainsCycle;

// for interaction between the main processor and the ISR
volatile bool b_datalogEventPending{false}; /**< async trigger to signal datalog is available */
volatile bool b_newMainsCycle{false};       /**< async trigger to signal start of new main cycle based on first phase */

// since there's no real locking feature for shared variables, a couple of data
// generated from inside the ISR are copied from time to time to be passed to the
// main processor. When the data are available, the ISR signals it to the main processor.
volatile int32_t copyOf_sumP_atSupplyPoint;                /**< copy of cumulative power per phase */
volatile int32_t copyOf_sum_Vsquared;                      /**< copy of for summation of V^2 values during datalog period */
volatile int32_t copyOf_energyInBucket_long;               /**< copy of main energy bucket (over all phases) */
volatile int32_t copyOf_lowestNoOfSampleSetsPerMainsCycle; /**<  */
volatile int32_t copyOf_sampleSetsDuringThisDatalogPeriod; /**< copy of for counting the sample sets during each datalogging period */

// for control of the LED at the "trigger" port (port D4)
enum LED_states LED_state;
boolean LED_pulseInProgress = false;
uint32_t LED_onAt;

// Calibration values
//-------------------
// Two calibration values are used: powerCal and phaseCal.
//
// powerCal is a floating point variable which is used for converting the
// product of voltage and current samples into Watts.
//
// The correct value of powerCal is dependent on the hardware that is
// in use. For best resolution, the hardware should be configured so that the
// voltage and current waveforms each span most of the ADC's usable range. For
// many systems, the maximum power that will need to be measured is around 3kW.
//
// My sketch "RawSamplesTool.ino" provides a one-shot visual display of the
// voltage and current waveforms as recorded by the processor. This provides
// a simple way for the user to be confident that their system has been set up
// correctly for the power levels that are to be measured.
//
// In the case of 240V mains voltage, the numerical value of the input signal
// in Volts is likely to be fairly similar to the output signal in ADC levels.
// 240V AC has a peak-to-peak amplitude of 679V, which is not far from the ideal
// output range. Stated more formally, the conversion rate of the overall system
// for measuring VOLTAGE is likely to be around 1 ADC-step per Volt.
//
// In the case of AC current, however, the situation is very different. At
// mains voltage, a power of 3kW corresponds to an RMS current of 12.5A which
// has a peak-to-peak range of 35A. This value is numerically smaller than the
// likely output signal from the ADC when measuring current by a factor of
// approximately twenty. The conversion rate of the overall system for measuring
// CURRENT is therefore likely to be around 20 ADC-steps per Amp.
//
// When calculating "real power", which is what this code does, the individual
// conversion rates for voltage and current are not of importance. It is
// only the conversion rate for POWER which is important. This is the
// product of the individual conversion rates for voltage and current. It
// therefore has the units of ADC-steps squared per Watt. Most systems will
// have a power conversion rate of around 20 (ADC-steps squared per Watt).
//
// powerCal is the RECIPR0CAL of the power conversion rate. A good value
// to start with is therefore 1/20 = 0.05 (Watts per ADC-step squared)
//
const float powerCal_grid[NO_OF_PHASES] = {0.0558, 0.0562, 0.0560};

// phaseCal is used to alter the phase of the voltage waveform relative to the
// current waveform. The algorithm interpolates between the most recent pair
// of voltage samples according to the value of phaseCal.
//
//    With phaseCal = 1, the most recent sample is used.
//    With phaseCal = 0, the previous sample is used
//    With phaseCal = 0.5, the mid-point (average) value in used
//
// Values outside the 0 to 1 range involve extrapolation, rather than interpolation
// and are not recommended. By altering the order in which V and I samples are
// taken, and for how many loops they are stored, it should always be possible to
// arrange for the optimal value of phaseCal to lie within the range 0 to 1. When
// measuring a resistive load, the voltage and current waveforms should be perfectly
// aligned. In this situation, the calculated Power Factor will be 1.
//
constexpr float phaseCal{1.0}; // <- nominal values only
// When using integer maths, calibration values that have been supplied in
// floating point form need to be rescaled.
constexpr int16_t i_phaseCal{256}; /**< to avoid the need for floating-point maths (f_phaseCal * 256) */

void setup()
{
  pinMode(outputForLED, OUTPUT);
  delay(100);
  LED_state = LED_ON;                    // to mimic the behaviour of an electricity
  digitalWrite(outputForLED, LED_state); // meter which starts up in 'sleep' mode

  delay(initialDelay); // allows time to open the Serial Monitor

  Serial.begin(9600);
  Serial.println();
  Serial.println("-------------------------------------");
  Serial.println("Sketch ID:      cal_CTx_v_meter.ino");
  Serial.println();

  // When using integer maths, the SIZE of the ENERGY BUCKET is altered to match the
  // scaling of the energy detection mechanism that is in use. This avoids the need
  // to re-scale every energy contribution, thus saving processing time. This process
  // is described in more detail in the function, allGeneralProcessing(), just before
  // the energy bucket is updated at the start of each new cycle of the mains.
  //
  // For the flow of energy at the 'grid' connection point (CTx)
  capacityOfEnergyBucket_long =
      (long)ENERGY_BUCKET_CAPACITY_IN_JOULES * CYCLES_PER_SECOND * (1 / powerCal_grid[CURRENT_CAL_PHASE]);
  energyInBucket_long = 0;

  // When using integer maths, calibration values that have supplied in floating point
  // form need to be rescaled.
  //
  Serial.println(F("ADC mode:       free-running"));

  // Set up the ADC to be free-running
  ADCSRA = (1 << ADPS0) + (1 << ADPS1) + (1 << ADPS2); // Set the ADC's clock to system clock / 128
  ADCSRA |= (1 << ADEN);                               // Enable the ADC

  ADCSRA |= (1 << ADATE); // set the Auto Trigger Enable bit in the ADCSRA register. Because
  // bits ADTS0-2 have not been set (i.e. they are all zero), the
  // ADC's trigger source is set to "free running mode".

  ADCSRA |= (1 << ADIE); // set the ADC interrupt enable bit. When this bit is written
  // to one and the I-bit in SREG is set, the
  // ADC Conversion Complete Interrupt is activated.

  ADCSRA |= (1 << ADSC); // start ADC manually first time
  sei();                 // Enable Global Interrupts

  Serial.print("Calibrating phase L");
  Serial.println(CURRENT_CAL_PHASE + 1);
  Serial.print("powerCal_grid =      ");
  Serial.println(powerCal_grid[CURRENT_CAL_PHASE], 4);

  Serial.print("zero-crossing persistence (sample sets) = ");
  Serial.println(PERSISTENCE_FOR_POLARITY_CHANGE);
  Serial.print("continuity sampling display rate (mains cycles) = ");
  Serial.println(CONTINUITY_CHECK_MAXCOUNT);

  Serial.println("----");
}

// An Interrupt Service Routine is now defined in which the ADC is instructed to
// measure each analogue input in sequence. A "data ready" flag is set after each
// voltage conversion has been completed.
//   For each set of samples, the two samples for current  are taken before the one
// for voltage. This is appropriate because each waveform current is generally slightly
// advanced relative to the waveform for voltage. The data ready flag is cleared
// within loop().
//   This Interrupt Service Routine is for use when the ADC is fixed timer mode. It is
// executed whenever the ADC timer expires. In this mode, the next ADC conversion is
// initiated from within this ISR.
//
ISR(ADC_vect)
{
  static uint8_t sample_index{0};
  static int16_t rawSample;

  switch (sample_index)
  {
  case 0:
    rawSample = ADC;                           // store the ADC value (this one is for Voltage)
    ADMUX = 0x40 + sensorV[CURRENT_CAL_PHASE]; // set up the next conversion, which is for Grid Current
    ++sample_index;                            // increment the control flag
    //
    processVoltageRawSample(rawSample);
    break;
  case 1:
    rawSample = ADC;                           // store the ADC value (this one is for Grid Current)
    ADMUX = 0x40 + sensorI[CURRENT_CAL_PHASE]; // set up the next conversion, which is for Voltage
    sample_index = 0;                          // reset the control flag
    //
    processCurrentRawSample(rawSample);
    break;
  default:
    sample_index = 0; // to prevent lockup (should never get here)
  }
}

// When using interrupt-based logic, the main processor waits in loop() until the
// dataReady flag has been set by the ADC. Once this flag has been set, the main
// processor clears the flag and proceeds with all the processing for one set of
// V & I samples. It then returns to loop() to wait for the next set to become
// available.
//
void loop()
{
  if (b_datalogEventPending)
  {
    b_datalogEventPending = false;

    Serial.print("Power L");
    Serial.print(CURRENT_CAL_PHASE + 1);
    Serial.print(": ");
    Serial.print((long)(copyOf_sumP_atSupplyPoint / copyOf_sampleSetsDuringThisDatalogPeriod * powerCal_grid[CURRENT_CAL_PHASE]));
    Serial.println("W");
  }
} // end of loop()

/*!
*  @defgroup TimeCritical Time critical functions Group
*  Functions used by the ISR
*/

/**
 * @brief Process the calculation for the actual current raw sample for the specific phase
 * 
 * @param rawSample the current sample for the specified phase
 * 
 * @ingroup TimeCritical
 */
void processCurrentRawSample(const int16_t rawSample)
{
  static int32_t sampleIminusDC;
  static int32_t phaseShiftedSampleVminusDC;
  static int32_t filtV_div4;
  static int32_t filtI_div4;
  static int32_t instP;

  // remove most of the DC offset from the current sample (the precise value does not matter)
  sampleIminusDC = ((int32_t)(rawSample - i_DCoffset_I_nom)) << 8;
  //
  // phase-shift the voltage waveform so that it aligns with the grid current waveform
  phaseShiftedSampleVminusDC = l_lastSampleVminusDC + (((l_sampleVminusDC - l_lastSampleVminusDC) * i_phaseCal) >> 8);
  //
  // calculate the "real power" in this sample pair and add to the accumulated sum
  filtV_div4 = phaseShiftedSampleVminusDC >> 2; // reduce to 16-bits (now x64, or 2^6)
  filtI_div4 = sampleIminusDC >> 2;             // reduce to 16-bits (now x64, or 2^6)
  instP = filtV_div4 * filtI_div4;              // 32-bits (now x4096, or 2^12)
  instP >>= 12;                                 // scaling is now x1, as for Mk2 (V_ADC x I_ADC)

  l_sumP += instP;               // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
  l_sumP_atSupplyPoint += instP; // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
}

/**
 * @brief Process the current voltage raw sample for the specific phase
 * 
 * @param rawSample the current sample for the specified phase
 * 
 * @ingroup TimeCritical
 */
void processVoltageRawSample(const int16_t rawSample)
{
  processPolarity(rawSample);
  confirmPolarity();
  //
  processRawSamples(); // deals with aspects that only occur at particular stages of each mains cycle
  //
  processVoltage();

  ++l_sampleSetsDuringThisDatalogPeriod;
}

/**
 * @brief Process with the polarity for the actual voltage sample for the specific phase
 * 
 * @param rawSample the current sample for the specified phase
 * 
 * @ingroup TimeCritical
 */
void processPolarity(const int16_t rawSample)
{
  l_lastSampleVminusDC = l_sampleVminusDC; // required for phaseCal algorithm
  // remove DC offset from each raw voltage sample by subtracting the accurate value
  // as determined by its associated LP filter.
  l_sampleVminusDC = (((int32_t)rawSample) << 8) - l_DCoffset_V;
  polarityOfMostRecentVsample = (l_sampleVminusDC > 0) ? Polarities::POSITIVE : Polarities::NEGATIVE;
}

/**
 * @brief This routine prevents a zero-crossing point from being declared until a certain number
 *        of consecutive samples in the 'other' half of the waveform have been encountered.
 * 
 * @ingroup TimeCritical
 */
void confirmPolarity()
{
  static uint8_t count{};

  if (polarityOfMostRecentVsample != polarityConfirmedOfLastSampleV)
    ++count;
  else
    count = 0;

  if (count > PERSISTENCE_FOR_POLARITY_CHANGE)
  {
    count = 0;
    polarityConfirmed = polarityOfMostRecentVsample;
  }
}

/**
 * @brief Process the calculation for the current voltage sample for the specific phase
 * 
 * @ingroup TimeCritical
 */
void processVoltage()
{
  static int32_t filtV_div4;
  static int32_t inst_Vsquared;

  // for the Vrms calculation (for datalogging only)
  filtV_div4 = l_sampleVminusDC >> 2;      // reduce to 16-bits (now x64, or 2^6)
  inst_Vsquared = filtV_div4 * filtV_div4; // 32-bits (now x4096, or 2^12)
  inst_Vsquared >>= 12;                    // scaling is now x1 (V_ADC x I_ADC)
  l_sum_Vsquared += inst_Vsquared;         // cumulative V^2 (V_ADC x I_ADC)
  //
  // store items for use during next loop
  l_cumVdeltasThisCycle += l_sampleVminusDC;          // for use with LP filter
  polarityConfirmedOfLastSampleV = polarityConfirmed; // for identification of half cycle boundaries
  ++l_samplesDuringThisMainsCycle;                    // for real power calculations
}

/**
 * @brief This routine is called by the ISR when a pair of V & I sample becomes available.
 * 
 * @ingroup TimeCritical
 */
void processRawSamples()
{
  // The raw V and I samples are processed in "phase pairs"
  if (Polarities::POSITIVE == polarityConfirmed)
  {
    // the polarity of this sample is positive
    if (Polarities::POSITIVE != polarityConfirmedOfLastSampleV)
    {
      if (beyondStartUpPeriod)
      {
        // This is the start of a new +ve half cycle, for this phase, just after the zero-crossing point.
        processPlusHalfCycle();
      }
      else
        processStartUp();
    }

    // still processing samples where the voltage is POSITIVE ...
    // check to see whether the trigger device can now be reliably armed
    if (beyondStartUpPeriod) // lower value for larger sample set
    {
      // This code is executed once per 20mS, shortly after the start of each new mains cycle on phase 0.
      processStartNewCycle();
    }
  }
  else
  {
    // the polarity of this sample is negative
    if (Polarities::NEGATIVE != polarityConfirmedOfLastSampleV)
    {
      // This is the start of a new -ve half cycle (just after the zero-crossing point)
      processMinusHalfCycle();
    }
  }
}
// end of processRawSamples()

/**
 * @brief Process the startup period for the router.
 * 
 * @param phase the phase number [0..NO_OF_PHASES[
 * 
 * @ingroup TimeCritical
 */
void processStartUp()
{
  // wait until the DC-blocking filters have had time to settle
  if (millis() <= (initialDelay + startUpPeriod))
    return; // still settling, do nothing

  // the DC-blocking filters have had time to settle
  beyondStartUpPeriod = true;
  l_sumP = 0;
  l_sumP_atSupplyPoint = 0;
  l_samplesDuringThisMainsCycle = 0;
  l_sampleSetsDuringThisDatalogPeriod = 0;

  l_lowestNoOfSampleSetsPerMainsCycle = 999L;
  // can't say "Go!" here 'cos we're in an ISR!
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
  // Now that the energy-related decisions have been taken, min and max limits can now
  // be applied  to the level of the energy bucket. This is to ensure correct operation
  // when conditions change, i.e. when import changes to export, and vice versa.
  //
  if (energyInBucket_long > capacityOfEnergyBucket_long)
  {
    energyInBucket_long -= capacityOfEnergyBucket_long;
    //registerConsumedPower();
  }
  else if (energyInBucket_long < 0)
  {
    digitalWrite(outputForLED, LED_ON); // to mimic the nehaviour of an electricity meter
    energyInBucket_long = 0;
  }
}

/**
 * @brief Process the start of a new -ve half cycle, for this phase, just after the zero-crossing point.
 * 
 * @ingroup TimeCritical
 */
void processMinusHalfCycle()
{
  // This is a convenient point to update the Low Pass Filter for removing the DC
  // component from the phase that is being processed.
  // The portion which is fed back into the integrator is approximately one percent
  // of the average offset of all the Vsamples in the previous mains cycle.
  //
  l_DCoffset_V += (l_cumVdeltasThisCycle >> 12);
  l_cumVdeltasThisCycle = 0;

  // To ensure that this LP filter will always start up correctly when 240V AC is
  // available, its output value needs to be prevented from drifting beyond the likely range
  // of the voltage signal.
  //
  if (l_DCoffset_V < l_DCoffset_V_min)
    l_DCoffset_V = l_DCoffset_V_min;
  else if (l_DCoffset_V > l_DCoffset_V_max)
    l_DCoffset_V = l_DCoffset_V_max;
}

/**
 * @brief Process the lastest contribution after each phase specific new cycle
 *        additional processing is performed after each main cycle based on phase 0.
 * 
 * @ingroup TimeCritical
 */
void processLatestContribution()
{
  b_newMainsCycle = true; //  a 50 Hz 'tick' for use by the main code

  // Calculate the real power and energy during the last whole mains cycle.
  //
  // sumP contains the sum of many individual calculations of instantaneous power.  In
  // order to obtain the average power during the relevant period, sumP must first be
  // divided by the number of samples that have contributed to its value.
  //
  // The next stage would normally be to apply a calibration factor so that real power
  // can be expressed in Watts.  That's fine for floating point maths, but it's not such
  // a good idea when integer maths is being used.  To keep the numbers large, and also
  // to save time, calibration of power is omitted at this stage.  Real Power (stored as
  // a 'long') is therefore (1/powerCal) times larger than the actual power in Watts.

  // Next, the energy content of this power rating needs to be determined.  Energy is
  // power multiplied by time, so the next step is normally to multiply the measured
  // value of power by the time over which it was measured.
  //   Instanstaneous power is calculated once every mains cycle. When integer maths is
  // being used, a repetitive power-to-energy conversion seems an unnecessary workload.
  // As all sampling periods are of similar duration, it is more efficient simply to
  // add all of the power samples together, and note that their sum is actually
  // CYCLES_PER_SECOND greater than it would otherwise be.
  //   Although the numerical value itself does not change, I thought that a new name
  // may be helpful so as to minimise confusion.
  //   The 'energy' variable below is CYCLES_PER_SECOND * (1/powerCal) times larger than
  // the actual energy in Joules.

  // Energy contributions from the grid connection point (CT1) are summed in an
  // accumulator which is known as the energy bucket.  The purpose of the energy bucket
  // is to mimic the operation of the supply meter. Most meters generate a visible pulse
  // when a certain amount of forward energy flow has been recorded, often 3600 Joules.
  // For this calibration sketch, the capacity of the energy bucket is set to this same
  // value within setup().
  //
  // The latest contribution can now be added to this energy bucket
  energyInBucket_long += (l_sumP / l_samplesDuringThisMainsCycle);
}

/**
 * @brief Process the start of a new +ve half cycle, for this phase, just after the zero-crossing point.
 * 
 * @ingroup TimeCritical
 */
void processPlusHalfCycle()
{
  processLatestContribution(); // runs at 6.6 ms intervals

  // A performance check to monitor and display the minimum number of sets of
  // ADC samples per mains cycle, the expected number being 20ms / (104us * 6) = 32.05
  //
  if (l_samplesDuringThisMainsCycle < l_lowestNoOfSampleSetsPerMainsCycle)
    l_lowestNoOfSampleSetsPerMainsCycle = l_samplesDuringThisMainsCycle;

  processDataLogging();

  // clear the per-cycle accumulators for use in this new mains cycle.
  l_sumP = 0;
  l_samplesDuringThisMainsCycle = 0;
  check_LED_status();
}

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
  if (++l_cycleCountForDatalogging < DATALOG_PERIOD_IN_MAINS_CYCLES)
    return; // data logging period not yet reached

  l_cycleCountForDatalogging = 0;

  copyOf_sumP_atSupplyPoint = l_sumP_atSupplyPoint;
  l_sumP_atSupplyPoint = 0;

  copyOf_sum_Vsquared = l_sum_Vsquared;
  l_sum_Vsquared = 0;

  copyOf_sampleSetsDuringThisDatalogPeriod = l_sampleSetsDuringThisDatalogPeriod; // (for diags only)
  copyOf_lowestNoOfSampleSetsPerMainsCycle = l_lowestNoOfSampleSetsPerMainsCycle; // (for diags only)
  copyOf_energyInBucket_long = energyInBucket_long;                               // (for diags only)

  l_lowestNoOfSampleSetsPerMainsCycle = 999L;
  l_sampleSetsDuringThisDatalogPeriod = 0;

  // signal the main processor that logging data are available
  b_datalogEventPending = true;
}

int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void registerConsumedPower()
{
  LED_onAt = cycleCount;
  LED_state = LED_ON;
  digitalWrite(outputForLED, LED_state);
  LED_pulseInProgress = true;
}

void check_LED_status()
{
  if (!LED_pulseInProgress)
    return;

  if (cycleCount > (LED_onAt + 2u)) // normal pulse duration
  {
    LED_state = LED_OFF;
    digitalWrite(outputForLED, LED_state);
    LED_pulseInProgress = false;
  }
}
