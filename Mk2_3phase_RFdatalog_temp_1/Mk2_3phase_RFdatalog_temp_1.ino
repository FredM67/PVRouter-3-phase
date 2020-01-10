/* Mk2_3phase_RFdatalog_temp_1.ino

   Issue 1 was released in January 2015.

   This sketch provides continuous monitoring of real power on three phases.
   Surplus power is diverted to multiple loads in sequential order. A suitable
   output-stage is required for each load; this can be either triac-based, or a
   Solid State Relay.

   Datalogging of real power and Vrms is provided for each phase.
   The presence or absence of the RFM12B needs to be set at compile time

   January 2016, renamed as Mk2_3phase_RFdatalog_2 with these changes:
   - Improved control of multiple loads has been imported from the
       equivalent 1-phase sketch, Mk2_multiLoad_wired_6.ino
   - the ISR has been upgraded to fix a possible timing anomaly
   - variables to store ADC samples are now declared as "volatile"
   - for RF69 RF module is now supported
   - a performance check has been added with the result being sent to the Serial port
   - control signals for loads are now active-high to suit the latest 3-phase PCB

   February 2016, renamed as Mk2_3phase_RFdatalog_3 with these changes:
   - improvements to the start-up logic. The start of normal operation is now
      synchronized with the start of a new mains cycle.
   - reduce the amount of feedback in the Low Pass Filter for removing the DC content
       from the Vsample stream. This resolves an anomaly which has been present since
       the start of this project. Although the amount of feedback has previously been
       excessive, this anomaly has had minimal effect on the system's overall behaviour.
   - The reported power at each of the phases has been inverted. These values are now in
       line with the Open Energy Monitor convention, whereby import is positive and
       export is negative.

        Robin Emley
        www.Mk2PVrouter.co.uk

   October 2019, renamed as Mk2_3phase_RFdatalog_temp_1 with these changes:
      This sketch has been restructured in order to make better use of the ISR. All of
   the time-critical code is now contained within the ISR and its helper functions.
   Values for datalogging are transferred to the main code using a flag-based handshake
   mechanism. The diversion of surplus power can no longer be affected by slower
   activities which may be running in the main code such as Serial statements and RF.
      Temperature sensing is supported. A pullup resistor (4K7 or similar) is required
   for the Dallas sensor.
     The output mode, i.e. NORMAL or ANTI_FLICKER, is now set at compile time.
   Also:
   - The ADC is now in free-running mode, at ~104 us per conversion.
   - a persistence check has been added for zero-crossing detection (polarityConfirmed)
   - a lowestNoOfSampleSetsPerMainsCycle check has been added, to detect any disturbances
   - Vrms has been added to the datalog payload (as Vrms x 100)
   - temperature has been added to the datalog payload (as degrees C x 100)
   - the phaseCal/f_voltageCal mechanism has been modified to be the same for all phases
   - RF capability made switchable so that the code will continue to run
     when an RF module is not fitted. Dataloging can then take place via the Serial port.
   - temperature capability made switchable so that the code will continue to run w/o sensor.
   - priority pin changed to handle peak/off-peak tariff
   - add rotating load priorities: this sketch is intended to control a 3-phase
     water heater which is composed of 3 independent heating elements wired in WYE
     with a neutral wire. The router will control each element in a specific order.
     To ensure that in average (over many days/months), each load runs the same time,
     each day, the router will rotate the priorities.
   - most functions have been splitted in one or more sub-functions. This way, each function
     has a specific task and is much smaller than before.
   - renaming of most of the variables with a single-letter prefix to identify its type.
   - direct port manipulation to save code size and speed-up performance

   January 2020, changes:
   - This sketch has been again re-engineered. All 'defines' have been removed except
     the ones for compile-time optional functionalities.
   - All constants have been replaced with constexpr initialized at compile-time
   - all number-types have been replaced with fixed width number types
   - old fashion enums replaced by scoped enums with fixed types
   - off-peak tariff made switchable at compile-time
   - rotation of load priorities made switcheable at compile-time
   - enhanced configuration for forcing specific loads during off-peak period
     Fred Metrich
*/

#include <Arduino.h> // may not be needed, but it's probably a good idea to include this

//#define TEMP_SENSOR     // <- this line must be commented out if the temperature sensor is not present

#define OFF_PEAK_TARIFF // <- this line must be commented out if there's only one single tariff each day

//#define RF_PRESENT // <- this line must be commented out if the RFM12B module is not present

#ifdef TEMP_SENSOR
#include <OneWire.h> // for temperature sensing
#endif

#ifdef RF_PRESENT
#define RF69_COMPAT 0 // for the RFM12B
// #define RF69_COMPAT 1 // for the RF69
#include <JeeLib.h>
#endif

// In this sketch, the ADC is free-running with a cycle time of ~104uS.

// -----------------------------------------------------
// Change these values to suit the local mains frequency and supply meter
constexpr int32_t CYCLES_PER_SECOND{50}; // number of cycles/s of the grid power supply
constexpr int32_t WORKING_ZONE_IN_JOULES{3600};
constexpr int32_t REQUIRED_EXPORT_IN_WATTS{10}; // when set to a negative value, this acts as a PV generator

// ----------------
// general literals
constexpr int32_t DATALOG_PERIOD_IN_MAINS_CYCLES{250}; // Period of datalogging in cycles

constexpr uint8_t NO_OF_PHASES{3}; // number of phases of the main supply.

constexpr uint8_t NO_OF_DUMPLOADS{3}; // number of dump loads connected to the diverter

#ifdef TEMP_SENSOR
// --------------------------
// Dallas DS18B20 commands
constexpr uint8_t SKIP_ROM{0xcc};
constexpr uint8_t CONVERT_TEMPERATURE{0x44};
constexpr uint8_t READ_SCRATCHPAD{0xbe};
constexpr uint16_t BAD_TEMPERATURE{30000}; // this value (300C) is sent if no sensor is present
#endif

#ifdef OFF_PEAK_TARIFF
#define PRIORITY_ROTATION                     // <- this line must be commented out if you want fixed priorities
constexpr uint32_t ul_OFF_PEAK_DURATION{8ul}; // <- this is the duration of the off-peak period in hours
// off-peak forced control for loads 0..n
// for load 'i' => rg_ForceLoad[i] = { offset, duration }
// the load 'i' will be started with full power at start_offpeak + 'offset' for a duration of 'duration'
// - all values are in hours.
// - to leave the load at full power till the end of the off-peak period, set the duration to 'UINT32_MAX' (somehow infinite time)
constexpr uint32_t rg_ForceLoad[NO_OF_DUMPLOADS][2] = {{5, UINT32_MAX},  // <-- for load #1
                                                       {5, UINT32_MAX},  // <-- for load #2
                                                       {5, UINT32_MAX}}; // <-- for load #3
#endif

// -------------------------------
// definitions of enumerated types
// polarities
enum class Polarities : uint8_t
{
  NEGATIVE,
  POSITIVE
};

// output modes
enum class OutputModes : uint8_t
{
  ANTI_FLICKER,
  NORMAL
};

// enum loadStates {LOAD_ON, LOAD_OFF}; // for use if loads are active low (original PCB)
enum class LoadStates : uint8_t
{
  LOAD_OFF,
  LOAD_ON
}; // for use if loads are active high (Rev 2 PCB)

LoadStates physicalLoadState[NO_OF_DUMPLOADS];

// For this multi-load version, the same mechanism has been retained but the
// output mode is hard-coded as below:
constexpr OutputModes outputMode{OutputModes::ANTI_FLICKER};

// Load priorities at startup
uint8_t loadPrioritiesAndState[NO_OF_DUMPLOADS]{0, 1, 2}; // load priorities and states.
constexpr uint8_t loadStateOnBit{0x80U};                  // bit mask for load state ON
constexpr uint8_t loadStateMask{0x7FU};                   // bit mask for masking load state

/* --------------------------------------
   RF configuration (for the RFM12B module)
   frequency options are RF12_433MHZ, RF12_868MHZ or RF12_915MHZ
*/
#ifdef RF_PRESENT
#define freq RF12_868MHZ

constexpr int nodeID{10};        //  RFM12B node ID
constexpr int networkGroup{210}; // wireless network group - needs to be same for all nodes
constexpr int UNO{1};            // for when the processor contains the UNO bootloader.
#endif

// -------------------------------
// definitions of a structure for datalogging
using Tx_struct = struct
{
  int16_t power;                 // import = +ve, to match OEM convention
  int16_t power_L[NO_OF_PHASES]; // import = +ve, to match OEM convention
  int16_t Vrms_L_times100[NO_OF_PHASES];
#ifdef TEMP_SENSOR
  int16_t temperature_times100;
#endif
}; // revised data for RF comms
Tx_struct tx_data;

// ----------- Pinout assignments  -----------
//
// digital pins:
// D0 & D1 are reserved for the Serial i/f
// D2 is for the RFM12B
#ifdef OFF_PEAK_TARIFF
constexpr uint8_t offPeakForcePin{3}; // for 3-phase PCB, off-peak trigger
#endif
#ifdef TEMP_SENSOR
constexpr uint8_t tempSensorPin{4}; // for 3-phase PCB, sensor pin
#endif
constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS]{5, 6, 7}; // for 3-phase PCB, Load #1/#2/#3 (Rev 2 PCB)
// D8 is not in use
// D9 is not in use
// D10 is for the RFM12B
// D11 is for the RFM12B
// D12 is for the RFM12B
// D13 is for the RFM12B

// analogue input pins
constexpr uint8_t sensorV[NO_OF_PHASES]{0, 2, 4}; // for 3-phase PCB, voltage measurement for each phase
constexpr uint8_t sensorI[NO_OF_PHASES]{1, 3, 5}; // for 3-phase PCB, current measurement for each phase

// --------------  general global variables -----------------
//
// Some of these variables are used in multiple blocks so cannot be static.
// For integer maths, some variables need to be 'int32_t'
//
bool beyondStartUpPeriod{false};        // start-up delay, allows things to settle
constexpr uint32_t initialDelay{3000};  // in milli-seconds, to allow time to open the Serial monitor
constexpr uint32_t startUpPeriod{3000}; // in milli-seconds, to allow LP filter to settle

#ifdef OFF_PEAK_TARIFF
uint32_t ul_TimeOffPeak;                     // 'timestamp' for start of off-peak period
uint32_t rg_OffsetForce[NO_OF_DUMPLOADS][2]; // start & stop offsets for each load
#endif

int32_t l_DCoffset_V[NO_OF_PHASES]; // <--- for LPF

// Define operating limits for the LP filters which identify DC offset in the voltage
// sample streams. By limiting the output range, these filters always should start up
// correctly.
constexpr int32_t l_DCoffset_V_min{(512L - 100L) * 256L}; // mid-point of ADC minus a working margin
constexpr int32_t l_DCoffset_V_max{(512L + 100L) * 256L}; // mid-point of ADC plus a working margin
constexpr int16_t i_DCoffset_I_nom{512L};                 // nominal mid-point value of ADC @ x1 scale

// for 3-phase use, with units of Joules * CYCLES_PER_SECOND
constexpr float f_capacityOfEnergyBucket_main{(float)(WORKING_ZONE_IN_JOULES * CYCLES_PER_SECOND)};
constexpr float f_midPointOfEnergyBucket_main{f_capacityOfEnergyBucket_main * 0.5}; // for resetting flexible thresholds
constexpr float f_offsetOfEnergyThresholdsInAFmode{0.1f};                           // <-- must not exceed 0.4

// set default threshold at compile time so the variable can be read-only
constexpr float initThreshold(const bool lower)
{
  return lower
             ? f_capacityOfEnergyBucket_main * (0.5 - ((OutputModes::ANTI_FLICKER == outputMode) ? f_offsetOfEnergyThresholdsInAFmode : 0))
             : f_capacityOfEnergyBucket_main * (0.5 + ((OutputModes::ANTI_FLICKER == outputMode) ? f_offsetOfEnergyThresholdsInAFmode : 0));
}

constexpr float f_lowerThreshold_default{initThreshold(true)};  // lower default threshold set accordingly to the output mode
constexpr float f_upperThreshold_default{initThreshold(false)}; // upper default threshold set accordingly to the output mode

float f_energyInBucket_main{0}; // main energy bucket (over all phases)
float f_lowerEnergyThreshold;   // dynamic lower threshold
float f_upperEnergyThreshold;   // dynamic upper threshold

// for improved control of multiple loads
bool b_recentTransition{false};                 // a load state has been recently toggled
uint8_t postTransitionCount;                    // counts the number of cycle since last transition
constexpr uint8_t POST_TRANSITION_MAX_COUNT{3}; // <-- allows each transition to take effect
//constexpr uint8_t POST_TRANSITION_MAX_COUNT{50}; // <-- for testing only
uint8_t activeLoad{NO_OF_DUMPLOADS}; // current active load

int32_t l_sumP[NO_OF_PHASES];                // cumulative power per phase
int32_t l_sampleVminusDC[NO_OF_PHASES];      // for the phaseCal algorithm
int32_t l_lastSampleVminusDC[NO_OF_PHASES];  // for the phaseCal algorithm
int32_t l_cumVdeltasThisCycle[NO_OF_PHASES]; // for the LPF which determines DC offset (voltage)
int32_t l_sumP_atSupplyPoint[NO_OF_PHASES];  // for summation of 'real power' values during datalog period
int32_t l_sum_Vsquared[NO_OF_PHASES];        // for summation of V^2 values during datalog period

int32_t l_samplesDuringThisMainsCycle[NO_OF_PHASES]; // for counting the sample sets during each mains cycle
int32_t l_sampleSetsDuringThisDatalogPeriod;         // for counting the sample sets during each datalogging period
int32_t l_cycleCountForDatalogging{0};               // for counting how often datalog is updated

// For a mechanism to check the integrity of this code structure
int32_t l_lowestNoOfSampleSetsPerMainsCycle;

// for interaction between the main processor and the ISR
volatile bool b_forceLoadsOn[NO_OF_DUMPLOADS]; // async trigger to force specific load(s) to ON
#ifdef PRIORITY_ROTATION
volatile bool b_reOrderLoads{false}; // async trigger for loads re-ordering
#endif
volatile bool b_datalogEventPending{false}; // async trigger to signal datalog is available
volatile bool b_newMainsCycle{false};       // async trigger to signal start of new main cycle based on first phase

// since there's no real locking feature for shared variables, a couple of data
// generated from inside the ISR are copied from time to time to be passed to the
// main processor. When the data are available, the ISR signals it to the main processor.
volatile int32_t copyOf_sumP_atSupplyPoint[NO_OF_PHASES];
volatile int32_t copyOf_sum_Vsquared[NO_OF_PHASES];
volatile float copyOf_energyInBucket_main;
volatile int32_t copyOf_lowestNoOfSampleSetsPerMainsCycle;
volatile int32_t copyOf_sampleSetsDuringThisDatalogPeriod;

#ifdef TEMP_SENSOR
// For temperature sensing
OneWire oneWire(tempSensorPin);
#endif

// For an enhanced polarity detection mechanism, which includes a persistence check
constexpr uint8_t PERSISTENCE_FOR_POLARITY_CHANGE{2};    // <-- allows polarity changes to be confirmed
Polarities polarityOfMostRecentVsample[NO_OF_PHASES];    // for zero-crossing detection
Polarities polarityConfirmed[NO_OF_PHASES];              // for zero-crossing detection
Polarities polarityConfirmedOfLastSampleV[NO_OF_PHASES]; // for zero-crossing detection

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
constexpr float f_powerCal[NO_OF_PHASES]{0.0556f, 0.0560f, 0.0558f};

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
constexpr float f_phaseCal{1}; // <- nominal values only
// When using integer maths, calibration values that have been supplied in
// floating point form need to be rescaled.
constexpr int16_t i_phaseCal{256}; // to avoid the need for floating-point maths (f_phaseCal * 256)

// For datalogging purposes, f_voltageCal has been added too. Because the range of ADC values is
// similar to the actual range of volts, the optimal value for this cal factor is likely to be
// close to unity.
constexpr float f_voltageCal{1.03f}; // compared with Fluke 77 meter

// update the control ports for each of the physical loads
//
void updatePortsStates()
{
  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    // update the local load's state.
    if (LoadStates::LOAD_OFF == physicalLoadState[i])
      PORTD &= ~(1 << physicalLoadPin[i]);
    else
      PORTD |= (1 << physicalLoadPin[i]);
  }
}

/* Since the pre-processor doesn't generate the function prototypes, we must declare them by hand !
*/
void processCurrentRawSample(const uint8_t phase, const int16_t rawSample);
void processVoltageRawSample(const uint8_t phase, const int16_t rawSample);
void processPolarity(const uint8_t phase, const int16_t rawSample);
void confirmPolarity(const uint8_t phase);
void processVoltage(const uint8_t phase);

void processRawSamples(const uint8_t phase);
void processStartUp(const uint8_t phase);
void processStartNewCycle();
void processMinusHalfCycle(const uint8_t phase);
void processPlusHalfCycle(const uint8_t phase);
void processLatestContribution(const uint8_t phase);

uint8_t nextLogicalLoadToBeAdded();
void proceedHighEnergyLevel();
uint8_t nextLogicalLoadToBeRemoved();
void proceedLowEnergyLevel();

void updatePhysicalLoadStates();
void updatePortsStates();

void processDataLogging();

/*
  An Interrupt Service Routine is now defined which instructs the ADC to perform a conversion
    for each of the voltage and current sensors in turn.
  This Interrupt Service Routine is for use when the ADC is in the free-running mode.
  It is executed whenever an ADC conversion has finished, approx every 104 us. In
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
  static uint8_t sample_index{0};
  static int16_t rawSample;

  switch (sample_index)
  {
  case 0:
    rawSample = ADC;           // store the ADC value (this one is for Voltage L1)
    ADMUX = 0x40 + sensorV[1]; // the conversion for I1 is already under way
    ++sample_index;            // increment the control flag
    //
    processVoltageRawSample(0, rawSample);
    break;
  case 1:
    rawSample = ADC;           // store the ADC value (this one is for Current L1)
    ADMUX = 0x40 + sensorI[1]; // the conversion for V2 is already under way
    ++sample_index;            // increment the control flag
    //
    processCurrentRawSample(0, rawSample);
    break;
  case 2:
    rawSample = ADC;           // store the ADC value (this one is for Voltage L2)
    ADMUX = 0x40 + sensorV[2]; // the conversion for I2 is already under way
    ++sample_index;            // increment the control flag
    //
    processVoltageRawSample(1, rawSample);
    break;
  case 3:
    rawSample = ADC;           // store the ADC value (this one is for Current L2)
    ADMUX = 0x40 + sensorI[2]; // the conversion for V3 is already under way
    ++sample_index;            // increment the control flag
    //
    processCurrentRawSample(1, rawSample);
    break;
  case 4:
    rawSample = ADC;           // store the ADC value (this one is for Voltage L3)
    ADMUX = 0x40 + sensorV[0]; // the conversion for I3 is already under way
    ++sample_index;            // increment the control flag
    //
    processVoltageRawSample(2, rawSample);
    break;
  case 5:
    rawSample = ADC;           // store the ADC value (this one is for Current L3)
    ADMUX = 0x40 + sensorI[0]; // the conversion for V1 is already under way
    sample_index = 0;          // reset the control flag
    //
    processCurrentRawSample(2, rawSample);
    break;
  default:
    sample_index = 0; // to prevent lockup (should never get here)
  }
} // end of ISR

/* -----------------------------------------------------------
   Start of various helper functions which are used by the ISR
*/

// Process the calculation for the actual current raw sample for the specific phase
//
void processCurrentRawSample(const uint8_t phase, const int16_t rawSample)
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
  phaseShiftedSampleVminusDC = l_lastSampleVminusDC[phase] + (((l_sampleVminusDC[phase] - l_lastSampleVminusDC[phase]) * i_phaseCal) >> 8);
  //
  // calculate the "real power" in this sample pair and add to the accumulated sum
  filtV_div4 = phaseShiftedSampleVminusDC >> 2; // reduce to 16-bits (now x64, or 2^6)
  filtI_div4 = sampleIminusDC >> 2;             // reduce to 16-bits (now x64, or 2^6)
  instP = filtV_div4 * filtI_div4;              // 32-bits (now x4096, or 2^12)
  instP >>= 12;                                 // scaling is now x1, as for Mk2 (V_ADC x I_ADC)

  l_sumP[phase] += instP;               // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
  l_sumP_atSupplyPoint[phase] += instP; // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
}

// Process the current voltage raw sample for the specific phase
//
void processVoltageRawSample(const uint8_t phase, const int16_t rawSample)
{
  processPolarity(phase, rawSample);
  confirmPolarity(phase);
  //
  processRawSamples(phase); // deals with aspects that only occur at particular stages of each mains cycle
  //
  processVoltage(phase);

  if (phase == 0)
    ++l_sampleSetsDuringThisDatalogPeriod;
}

// Process with the polarity for the actual voltage sample for the specific phase
//
void processPolarity(const uint8_t phase, const int16_t rawSample)
{
  l_lastSampleVminusDC[phase] = l_sampleVminusDC[phase]; // required for phaseCal algorithm
  // remove DC offset from each raw voltage sample by subtracting the accurate value
  // as determined by its associated LP filter.
  l_sampleVminusDC[phase] = (((int32_t)rawSample) << 8) - l_DCoffset_V[phase];
  polarityOfMostRecentVsample[phase] = (l_sampleVminusDC[phase] > 0) ? Polarities::POSITIVE : Polarities::NEGATIVE;
}

// This routine prevents a zero-crossing point from being declared until a certain number
// of consecutive samples in the 'other' half of the waveform have been encountered.
//
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

// Process the calculation for the current voltage sample for the specific phase
//
void processVoltage(const uint8_t phase)
{
  static int32_t filtV_div4;
  static int32_t inst_Vsquared;

  // for the Vrms calculation (for datalogging only)
  filtV_div4 = l_sampleVminusDC[phase] >> 2; // reduce to 16-bits (now x64, or 2^6)
  inst_Vsquared = filtV_div4 * filtV_div4;   // 32-bits (now x4096, or 2^12)
  inst_Vsquared >>= 12;                      // scaling is now x1 (V_ADC x I_ADC)
  l_sum_Vsquared[phase] += inst_Vsquared;    // cumulative V^2 (V_ADC x I_ADC)
  //
  // store items for use during next loop
  l_cumVdeltasThisCycle[phase] += l_sampleVminusDC[phase];          // for use with LP filter
  polarityConfirmedOfLastSampleV[phase] = polarityConfirmed[phase]; // for identification of half cycle boundaries
  ++l_samplesDuringThisMainsCycle[phase];                           // for real power calculations
}

/*
   This routine is called by the ISR when a pair of V & I sample becomes available.
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
    if (beyondStartUpPeriod && (phase == 0) && (2 == l_samplesDuringThisMainsCycle[0])) // lower value for larger sample set
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

// process the startup period for the router
//
void processStartUp(const uint8_t phase)
{
  // wait until the DC-blocking filters have had time to settle
  if (millis() <= (initialDelay + startUpPeriod))
    return; // still settling, do nothing

  // the DC-blocking filters have had time to settle
  beyondStartUpPeriod = true;
  l_sumP[phase] = 0;
  l_sumP_atSupplyPoint[phase] = 0;
  l_samplesDuringThisMainsCycle[phase] = 0;
  l_sampleSetsDuringThisDatalogPeriod = 0;

  l_lowestNoOfSampleSetsPerMainsCycle = 999L;
  // can't say "Go!" here 'cos we're in an ISR!
}

/*
   This code is executed once per 20mS, shortly after the start of each new
   mains cycle on phase 0.

   Changing the state of the loads  is a 3-part process:
   - change the LOGICAL load states as necessary to maintain the energy level
   - update the PHYSICAL load states according to the logical -> physical mapping
   - update the driver lines for each of the loads.
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
    f_lowerEnergyThreshold = f_lowerThreshold_default; // reset the "opposite" threshold
    if (f_energyInBucket_main > f_upperEnergyThreshold)
    {
      // Because the energy level is high, some action may be required
      proceedHighEnergyLevel();
    }
  }
  else
  {
    // the energy state is in the lower half of the working range
    f_upperEnergyThreshold = f_upperThreshold_default; // reset the "opposite" threshold
    if (f_energyInBucket_main < f_lowerEnergyThreshold)
    {
      // Because the energy level is low, some action may be required
      proceedLowEnergyLevel();
    }
  }

  updatePhysicalLoadStates(); // allows the logical-to-physical mapping to be changed

  updatePortsStates(); // update the control ports for each of the physical loads

  // Now that the energy-related decisions have been taken, min and max limits can now
  // be applied  to the level of the energy bucket. This is to ensure correct operation
  // when conditions change, i.e. when import changes to export, and vice versa.
  //
  if (f_energyInBucket_main > f_capacityOfEnergyBucket_main)
    f_energyInBucket_main = f_capacityOfEnergyBucket_main;
  else if (f_energyInBucket_main < 0)
    f_energyInBucket_main = 0;
}

// Process the start of a new -ve half cycle, for this phase, just after the zero-crossing point.
//
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

// retrieve the next load that could be added (be aware of the order)
// return NO_OF_DUMPLOADS in case of failure
//
uint8_t nextLogicalLoadToBeAdded()
{
  for (uint8_t index = 0; index < NO_OF_DUMPLOADS; ++index)
    if (0x00 == (loadPrioritiesAndState[index] & loadStateOnBit))
      return (index);

  return (NO_OF_DUMPLOADS);
}

// retrieve the next load that could be removed (be aware of the reverse-order)
// return NO_OF_DUMPLOADS in case of failure
//
uint8_t nextLogicalLoadToBeRemoved()
{
  uint8_t index{NO_OF_DUMPLOADS};
  do
  {
    --index;
    if (loadPrioritiesAndState[index] & loadStateOnBit)
      return (index);
  } while (0 != index);

  return (NO_OF_DUMPLOADS);
}

// Process the case of high energy level, some action may be required
//
void proceedHighEnergyLevel()
{
  bool bOK_toAddLoad{true};
  auto tempLoad{nextLogicalLoadToBeAdded()};

  if (tempLoad >= NO_OF_DUMPLOADS)
    return;

  // a load which is now OFF has been identified for potentially being switched ON
  if (b_recentTransition)
  {
    // During the post-transition period, any increase in the energy level is noted.
    if (f_energyInBucket_main > f_upperEnergyThreshold)
    {
      f_upperEnergyThreshold = f_energyInBucket_main;

      // the energy thresholds must remain within range
      if (f_upperEnergyThreshold > f_capacityOfEnergyBucket_main)
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

// Process the case of low energy level, some action may be required
//
void proceedLowEnergyLevel()
{
  bool bOK_toRemoveLoad{true};
  auto tempLoad{nextLogicalLoadToBeRemoved()};

  if (tempLoad >= NO_OF_DUMPLOADS)
    return;

  // a load which is now ON has been identified for potentially being switched OFF
  if (b_recentTransition)
  {
    // During the post-transition period, any decrease in the energy level is noted.
    if (f_energyInBucket_main < f_lowerEnergyThreshold)
    {
      f_lowerEnergyThreshold = f_energyInBucket_main;

      // the energy thresholds must remain within range
      if (f_lowerEnergyThreshold < 0)
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

// process the lastest contribution after each phase specific new cycle
// additional processing is performed after each main cycle based on phase 0
//
void processLatestContribution(const uint8_t phase)
{
  b_newMainsCycle = true; // <--  a 50 Hz 'tick' for use by the main code

  // for efficiency, the energy scale is Joules * CYCLES_PER_SECOND
  // add the latest energy contribution to the main energy accumulator
  f_energyInBucket_main += (l_sumP[phase] / l_samplesDuringThisMainsCycle[phase]) * f_powerCal[phase];

  // apply any adjustment that is required.
  if (0 == phase)
    f_energyInBucket_main -= REQUIRED_EXPORT_IN_WATTS; // energy scale is Joules x 50

  // Applying max and min limits to the main accumulator's level
  // is deferred until after the energy related decisions have been taken
  //
}

// Process the start of a new +ve half cycle, for this phase, just after the zero-crossing point.
//
void processPlusHalfCycle(const uint8_t phase)
{
  processLatestContribution(phase); // runs at 6.6 ms intervals

  // A performance check to monitor and display the minimum number of sets of
  // ADC samples per mains cycle, the expected number being 20ms / (104us * 6) = 32.05
  //
  if (0 == phase)
  {
    if (l_samplesDuringThisMainsCycle[phase] < l_lowestNoOfSampleSetsPerMainsCycle)
      l_lowestNoOfSampleSetsPerMainsCycle = l_samplesDuringThisMainsCycle[phase];

    processDataLogging();
  }

  l_sumP[phase] = 0;
  l_samplesDuringThisMainsCycle[phase] = 0;
}

/*
   This function provides the link between the logical and physical loads. The
   array, logicalLoadState[], contains the on/off state of all logical loads, with
   element 0 being for the one with the highest priority. The array,
   physicalLoadState[], contains the on/off state of all physical loads.

   The lowest 7 bits of element is the load number as defined in 'physicalLoadState'.
   The highest bit of each 'loadPrioritiesAndState' determines if the load is ON or OFF.
   The order of each element in 'loadPrioritiesAndState' determines the load priority.
     'loadPrioritiesAndState[i] & loadStateMask' will extract the load number at position 'i'
     'loadPrioritiesAndState[i] & loadStateOnBit' will extract the load state at position 'i'

   Any other mapping relationships could be configured here.
*/
void updatePhysicalLoadStates()
{
  uint8_t i;

#ifdef PRIORITY_ROTATION
  if (b_reOrderLoads)
  {
    const auto temp{loadPrioritiesAndState[0]};
    for (i = 0; i < NO_OF_DUMPLOADS - 1; ++i)
      loadPrioritiesAndState[i] = loadPrioritiesAndState[i + 1];

    loadPrioritiesAndState[i] = temp;

    b_reOrderLoads = false;
  }
#endif

  for (i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    const auto iLoad{loadPrioritiesAndState[i] & loadStateMask};
    physicalLoadState[iLoad] =
        (loadPrioritiesAndState[i] & loadStateOnBit) || b_forceLoadsOn[iLoad] ? LoadStates::LOAD_ON : LoadStates::LOAD_OFF;
  }
}

// Process with data logging.
// At the end of each datalogging period, copies are made of the relevant variables
// for use by the main code. These variable are then reset for use during the next
// datalogging period.
//
void processDataLogging()
{
  if (++l_cycleCountForDatalogging < DATALOG_PERIOD_IN_MAINS_CYCLES)
    return; // data logging period not yet reached

  l_cycleCountForDatalogging = 0;

  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    copyOf_sumP_atSupplyPoint[phase] = l_sumP_atSupplyPoint[phase];
    l_sumP_atSupplyPoint[phase] = 0;

    copyOf_sum_Vsquared[phase] = l_sum_Vsquared[phase];
    l_sum_Vsquared[phase] = 0;
  }

  copyOf_sampleSetsDuringThisDatalogPeriod = l_sampleSetsDuringThisDatalogPeriod; // (for diags only)
  copyOf_lowestNoOfSampleSetsPerMainsCycle = l_lowestNoOfSampleSetsPerMainsCycle; // (for diags only)
  copyOf_energyInBucket_main = f_energyInBucket_main;                             // (for diags only)

  l_lowestNoOfSampleSetsPerMainsCycle = 999L;
  l_sampleSetsDuringThisDatalogPeriod = 0;

  // signal the main processor that logging data are available
  b_datalogEventPending = true;
}
/* End of helper functions which are used by the ISR
   -------------------------------------------------
*/

// prints data logs to the Serial output
//
void printDataLogging()
{
  uint8_t phase;

  Serial.print(copyOf_energyInBucket_main / CYCLES_PER_SECOND);
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
    Serial.print((float)tx_data.Vrms_L_times100[phase] / 100);
  }

#ifdef TEMP_SENSOR
  Serial.print(", temperature ");
  Serial.print((float)tx_data.temperature_times100 / 100);
#endif
  Serial.print(F(", (minSampleSets/MC "));
  Serial.print(copyOf_lowestNoOfSampleSetsPerMainsCycle);
  Serial.print(F(", #ofSampleSets "));
  Serial.print(copyOf_sampleSetsDuringThisDatalogPeriod);
  Serial.println(F(")"));
}

// prints the load priorities to the Serial output
//
void logLoadPriorities()
{
  Serial.println(F("loadPriority: "));
  for (const auto loadPrioAndState : loadPrioritiesAndState)
  {
    Serial.print(F("\tload "));
    Serial.println(loadPrioAndState);
  }
}

// This function changes the value of the load priorities
// Since we don't have access to a clock, we detect the offPeak start from the main energy meter.
// Additionally, when off-peak period starts, we rotate the load priorities for the next day.
//
void checkLoadPrioritySelection()
{
#ifdef OFF_PEAK_TARIFF
  uint8_t i;

  static uint8_t pinOffPeakState{HIGH};

  const uint8_t pinNewState{PIND & (1 << offPeakForcePin)};

  if (pinOffPeakState && !pinNewState)
  {
    // we start off-peak period
    Serial.println(F("Change to off-peak period!"));
    ul_TimeOffPeak = millis();

#ifdef PRIORITY_ROTATION
    b_reOrderLoads = true;

    // waits till the priorities have been rotated from inside the ISR
    while (b_reOrderLoads)
      delay(10);
#endif

    // prints the (new) load priorities
    logLoadPriorities();
  }
  else
  {
    const auto ulElapsedTime{(uint32_t)(millis() - ul_TimeOffPeak)};

    for (i = 0; i < NO_OF_DUMPLOADS - 1; ++i)
    {
      // for each load, if we're inside off-peak period and within the 'force period', trigger the ISR to turn the load ON
      if (!pinOffPeakState && !pinNewState &&
          (ulElapsedTime >= rg_OffsetForce[i][0]) &&
          (ulElapsedTime < rg_OffsetForce[i][1]))
        b_forceLoadsOn[i] = true;
      else
        b_forceLoadsOn[i] = false;
    }
  }

  // end of off-peak period
  if (!pinOffPeakState && pinNewState)
    Serial.println(F("Change to peak period!"));

  pinOffPeakState = pinNewState;
#endif
}

// Although this sketch always operates in ANTI_FLICKER mode, it was convenient
// to leave this mechanism in place.
//
void printParamsForSelectedOutputMode()
{
  // display relevant settings for selected output mode
  Serial.print("Output mode:    ");
  if (OutputModes::NORMAL == outputMode)
    Serial.println("normal");
  else
  {
    Serial.println("anti-flicker");
    Serial.print("\toffsetOfEnergyThresholds  = ");
    Serial.println(f_offsetOfEnergyThresholdsInAFmode);
  }
  Serial.print(F("\tf_capacityOfEnergyBucket_main = "));
  Serial.println(f_capacityOfEnergyBucket_main);
  Serial.print(F("\tf_lowerEnergyThreshold   = "));
  Serial.println(f_lowerThreshold_default);
  Serial.print(F("\tf_upperEnergyThreshold   = "));
  Serial.println(f_upperThreshold_default);
}

#ifdef TEMP_SENSOR
// convert the internal value read from the sensor to a value in °C
//
void convertTemperature()
{
  oneWire.reset();
  oneWire.write(SKIP_ROM);
  oneWire.write(CONVERT_TEMPERATURE);
}

// read the temperature
//
int16_t readTemperature()
{
  uint8_t buf[9];

  oneWire.reset();
  oneWire.write(SKIP_ROM);
  oneWire.write(READ_SCRATCHPAD);
  for (auto &buf_elem : buf)
    buf_elem = oneWire.read();

  if (oneWire.crc8(buf, 8) != buf[8])
    return BAD_TEMPERATURE;

  // result is temperature x16, multiply by 6.25 to convert to temperature x100
  int16_t result = (buf[1] << 8) | buf[0];
  return (result * 6) + (result >> 2);
}
#endif

#ifdef RF_PRESENT
//
// To avoid disturbance to the sampling process, the RFM12B needs to remain in its
// active state rather than being periodically put to sleep.
void send_rf_data()
{
  // check whether it's ready to send, and an exit route if it gets stuck
  uint32_t i = 0;
  while (!rf12_canSend() && i < 10)
  {
    rf12_recvDone();
    ++i;
  }
  rf12_sendNow(0, &tx_data, sizeof tx_data);
}
#endif

int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void setup()
{
  delay(initialDelay); // allows time to open the Serial Monitor

  Serial.begin(9600); // initialize Serial interface
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.println(F("----------------------------------"));
  Serial.println(F("Sketch ID:  Mk2_3phase_RFdatalog_temp_1.ino"));

  // initializes all loads to OFF at startup
  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    DDRD |= (1 << physicalLoadPin[i]); // driver pin for Load #n
    loadPrioritiesAndState[i] &= loadStateMask;
  }
  updatePhysicalLoadStates(); // allows the logical-to-physical mapping to be changed

  updatePortsStates(); // updates output pin states

#ifdef OFF_PEAK_TARIFF
  DDRD &= ~(1 << offPeakForcePin);                 // set as input
  PORTD |= (1 << offPeakForcePin);                 // enable the internal pullup resistor
  delay(100);                                      // allow time to settle
  uint8_t pinState{PIND & (1 << offPeakForcePin)}; // initial selection and

  ul_TimeOffPeak = millis();

  // calculates offsets for force start and stop of each load
  for (uint8_t i = 0; i < NO_OF_DUMPLOADS - 1; ++i)
  {
    rg_OffsetForce[i][0] = ul_OFF_PEAK_DURATION - rg_ForceLoad[i][0];
    rg_OffsetForce[i][1] = (UINT32_MAX != rg_ForceLoad[i][1]) ? rg_OffsetForce[i][0] + rg_ForceLoad[i][1] : rg_ForceLoad[i][1];

    rg_OffsetForce[i][0] *= 3600000ul; // convert in milli-seconds
    rg_OffsetForce[i][1] *= 3600000ul; // convert in milli-seconds
  }
#endif

  for (auto &bforceLoad : b_forceLoadsOn)
    bforceLoad = false;

  for (auto &DCoffset_V : l_DCoffset_V)
    DCoffset_V = 512L * 256L; // nominal mid-point value of ADC @ x256 scale

  Serial.println(F("ADC mode:       free-running"));
  Serial.print(F("requiredExport in Watts = "));
  Serial.println(REQUIRED_EXPORT_IN_WATTS);

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

  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    Serial.print(F("f_powerCal for L"));
    Serial.print(phase + 1);
    Serial.print(F(" =    "));
    Serial.println(f_powerCal[phase], 4);
  }
  Serial.print(F("f_phaseCal for all phases"));
  Serial.print(F(" =     "));
  Serial.println(f_phaseCal);

  Serial.print(F("f_voltageCal, for Vrms  =      "));
  Serial.println(f_voltageCal, 4);

  Serial.print(F("Export rate (Watts) = "));
  Serial.println(REQUIRED_EXPORT_IN_WATTS);

  Serial.print(F("zero-crossing persistence (sample sets) = "));
  Serial.println(PERSISTENCE_FOR_POLARITY_CHANGE);

  printParamsForSelectedOutputMode();

  logLoadPriorities();

  Serial.print(F(">>free RAM = "));
  Serial.println(freeRam()); // a useful value to keep an eye on

  Serial.println(F("----"));

#ifdef TEMP_SENSOR
  convertTemperature(); // start initial temperature conversion
#endif

  Serial.print("RF capability ");

#ifdef RF_PRESENT
  Serial.print(F("IS present, freq = "));
  if (freq == RF12_433MHZ)
    Serial.println(F("433 MHz"));
  else if (freq == RF12_868MHZ)
    Serial.println(F("868 MHz"));
  rf12_initialize(nodeID, freq, networkGroup); // initialize RF
#else
  Serial.println(F("is NOT present"));
#endif

#ifdef TEMP_SENSOR
  convertTemperature(); // start initial temperature conversion
#endif
}

/* None of the workload in loop() is time-critical. All the processing of
    ADC data is done within the ISR.
*/
void loop()
{
  static uint8_t perSecondTimer{0};

  if (b_newMainsCycle) // flag is set after every pair of ADC conversions
  {
    b_newMainsCycle = false; // reset the flag
    ++perSecondTimer;

    if (perSecondTimer >= CYCLES_PER_SECOND)
    {
      perSecondTimer = 0;
      checkLoadPrioritySelection(); // called every second
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

      tx_data.Vrms_L_times100[phase] = (int32_t)(100 * f_voltageCal * sqrt(copyOf_sum_Vsquared[phase] / copyOf_sampleSetsDuringThisDatalogPeriod));
    }
#ifdef TEMP_SENSOR
    tx_data.temperature_times100 = readTemperature();
#endif

#ifdef RF_PRESENT
    send_rf_data();
#endif

    printDataLogging();

#ifdef TEMP_SENSOR
    convertTemperature(); // for use next time around
#endif
  }
} // end of loop()
