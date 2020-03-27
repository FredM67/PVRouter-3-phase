
/* Mk2_fasterControl_twoLoads_2.ino
 *
 *  (initially released as Mk2_bothDisplays_1 in March 2014)
 * This sketch is for diverting suplus PV power to a dump load using a triac or  
 * Solid State Relay. It is based on the Mk2i PV Router code that I have posted in on  
 * the OpenEnergyMonitor forum.  The original version, and other related material, 
 * can be found on my Summary Page at www.openenergymonitor.org/emon/node/1757
 *
 * In this latest version, the pin-allocations have been changed to suit my 
 * PCB-based hardware for the Mk2 PV Router.  The integral voltage sensor is 
 * fed from one of the secondary coils of the transformer.  Current is measured 
 * via Current Transformers at the CT1 and CT1 ports.  
 * 
 * CT1 is for 'grid' current, to be measured at the grid supply point.
 * CT2 is for the load current, so that diverted energy can be recorded
 *
 * A persistence-based 4-digit display is supported. This can be driven in two
 * different ways, one with an extra pair of logic chips, and one without.  The 
 * appropriate version of the sketch must be selected by including or commenting 
 * out the "#define PIN_SAVING_HARDWARE" statement near the top of the code.
 *
 * September 2014: renamed as Mk2_bothDisplays_2, with these changes:
 * - cycleCount removed (was not actually used in this sketch, but could have overflowed);
 * - removal of unhelpful comments in the IO pin section;
 * - tidier initialisation of display logic in setup();
 * - addition of REQUIRED_EXPORT_IN_WATTS logic (useful as a built-in PV simulation facility);
 *
 * December 2014: renamed as Mk2_bothDisplays_3, with these changes:
 * - persistence check added for zero-crossing detection (polarityConfirmed)
 * - lowestNoOfSampleSetsPerMainsCycle added, to check for any disturbances
 *
 * December 2014: renamed as Mk2_bothDisplays_3a, with some typographical errors fixed.
 *
 * January 2016: renamed as Mk2_bothDisplays_3b, with a minor change in the ISR to 
 *   remove a timing uncertainty.
 *
 * January 2016: updated to Mk2_bothDisplays_3c:
 *   The variables to store the ADC results are now declared as "volatile" to remove 
 *   any possibility of incorrect operation due to optimisation by the compiler.
 *
 * February 2016: updated to Mk2_bothDisplays_4, with these changes:
 * - improvements to the start-up logic.  The start of normal operation is now 
 *    synchronised with the start of a new mains cycle.
 * - reduce the amount of feedback in the Low Pass Filter for removing the DC content
 *     from the Vsample stream. This resolves an anomaly which has been present since 
 *     the start of this project.  Although the amount of feedback has previously been 
 *     excessive, this anomaly has had minimal effect on the system's overall behaviour.
 * - removal of the unhelpful "triggerNeedsToBeArmed" mechanism
 * - tidying of the "confirmPolarity" logic to make its behaviour more clear
 * - SWEETZONE_IN_JOULES changed to WORKING_RANGE_IN_JOULES 
 * - change "triac" to "load" wherever appropriate
 *
 * November 2019: updated to Mk2_fasterControl_1 with these changes:
 * - Half way through each mains cycle, a prediction is made of the likely energy level at the
 *   end of the cycle.  That predicted value allows the triac to be switched at the +ve going 
 *   zero-crossing point rather than waiting for a further 10 ms.  These changes allow for 
 *   faster switching of the load.
 * - The range of the energy bucket has been reduced to one tenth of its former value. This
 *   allows the unit's operation to commence more rapidly whenever surplus power is available.
 * - controlMode is no longer selectable, the unit's operation being effectively hard-coded 
 *   as "Normal" rather than Anti-flicker. 
 * - Port D3 now supports an indicator which shows when the level in the energy bucket
 *   reaches either end of its range.  While the unit is actively diverting surplus power,
 *   it is vital that the level in the reduced capacity energy bucket remains within its 
 *   permitted range, hence the addition of this indicator.
 *   
 * February 2020: updated to Mk2_fasterControl_twoLoads_1 with these changes:
 * - the energy overflow indicator has been disabled to free up port D3 
 * - port D3 now supports a second load
 * 
 * February 2020: updated to Mk2_fasterControl_twoLoads_2 with these changes:
 * - improved multi-load control logic to prevent the primary load from being disturbed by
 *   the lower priority one.  This logic now mirrors that in the Mk2_multiLoad_wired_n line.
 * 
 *   
 * *      Robin Emley
 *      www.Mk2PVrouter.co.uk
 */

#include <Arduino.h>
#include <TimerOne.h>

#ifdef TEMP_SENSOR
#include <OneWire.h> // for temperature sensing
#endif

#define ADC_TIMER_PERIOD 125 // uS (determines the sampling rate / amount of idle time)

// -----------------------------------------------------
// Change these values to suit the local mains frequency and supply meter
constexpr int32_t CYCLES_PER_SECOND{50};       /**< number of cycles/s of the grid power supply */
constexpr int32_t WORKING_ZONE_IN_JOULES{360}; /**< 0.1 Wh, reduced for faster start-up */
constexpr int32_t REQUIRED_EXPORT_IN_WATTS{0}; /**< when set to a negative value, this acts as a PV generator */

// to prevent the diverted energy total from 'creeping'
constexpr int32_t ANTI_CREEP_LIMIT { 5 } /**< in Joules per mains cycle (has no effect when set to 0) */

//  The two versions of the hardware require different logic.  The following line should
//  be included if the additional logic chips are present, or excluded if they are
//  absent (in which case some wire links need to be fitted)
//
#define PIN_SAVING_HARDWARE

constexpr uint8_t NO_OF_DUMPLOADS{2};

#ifdef TEMP_SENSOR
// --------------------------
// Dallas DS18B20 commands
constexpr uint8_t SKIP_ROM{0xcc};
constexpr uint8_t CONVERT_TEMPERATURE{0x44};
constexpr uint8_t READ_SCRATCHPAD{0xbe};
constexpr uint16_t BAD_TEMPERATURE{30000}; /**< this value (300C) is sent if no sensor is present */
#endif

/** Polarities */
enum class Polarities : uint8_t
{
  NEGATIVE, /**< polarity is negative */
  POSITIVE  /**< polarity is positive */
};

/** Load state (all loads are logically active-low) */
enum class LoadStates : uint8_t
{
  LOAD_ON,
  LOAD_OFF
};

LoadStates logicalLoadState[NO_OF_DUMPLOADS];
LoadStates physicalLoadState[NO_OF_DUMPLOADS];

// For this go-faster version, the unit's operation will effectively always be "Normal";
// there is no "Anti-flicker" option. The controlMode variable has been removed.

// allocation of digital pins which are not dependent on the display type that is in use
// *************************************************************************************
// const uint8_t outOfRangeIndication = 3; // <-- this output port is active-high
constexpr uint8_t physicalLoad_1_pin{3}; /**< <-- the "mode" port is active-high */
constexpr uint8_t physicalLoad_0_pin{4}; /**< <-- the "trigger" port is active-low */

// allocation of analogue pins which are not dependent on the display type that is in use
// **************************************************************************************
constexpr uint8_t voltageSensor = 3;          /**< A3 is for the voltage sensor */
constexpr uint8_t currentSensor_diverted = 4; /**< A4 is for CT2 which measures diverted current */
constexpr uint8_t currentSensor_grid = 5;     /**< A5 is for CT1 which measures grid current */

constexpr uint32_t delayBeforeSerialStarts{1000}; /**< in milli-seconds, to allow time to open the Serial monitor */
constexpr uint32_t startUpPeriod{3000};           /**< in milli-seconds, to allow LP filter to settle */

int32_t DCoffset_V_long; /**< <--- for LPF */

// Define operating limits for the LP filters which identify DC offset in the voltage
// sample streams. By limiting the output range, these filters always should start up
// correctly.
constexpr int16_t DCoffset_I{512L};                            /**< nominal mid-point value of ADC @ x1 scale */
constexpr int32_t DCoffset_V_min{(int32_t)(512L - 100) * 256}; /**< mid-point of ADC minus a working margin */
constexpr int32_t DCoffset_V_max{(int32_t)(512L + 100) * 256}; /**< mid-point of ADC plus a working margin */

// General global variables that are used in multiple blocks so cannot be static.
// For integer maths, many variables need to be 'int32_t'
//
boolean beyondStartUpPhase = false; // start-up delay, allows things to settle

// Calibration values
//-------------------
// Two calibration values are used: powerCal and phaseCal.
// A full explanation of each of these values now follows:
//
// powerCal is a floating point variable which is used for converting the
// product of voltage and current samples into Watts.
//
// The correct value of powerCal is dependent on the hardware that is
// in use.  For best resolution, the hardware should be configured so that the
// voltage and current waveforms each span most of the ADC's usable range.  For
// many systems, the maximum power that will need to be measured is around 3kW.
//
// My sketch "RawSamplesTool_2chan.ino" provides a one-shot visual display of the
// voltage and current waveforms.  This provides an easy way for the user to be
// confident that their system has been set up correctly for the power levels
// that are to be measured.
//
// The ADC has an input range of 0-5V and an output range of 0-1023 levels.
// The purpose of each input sensor is to convert the measured parameter into a
// low-voltage signal which fits nicely within the ADC's input range.
//
// In the case of 240V mains voltage, the numerical value of the input signal
// in Volts is likely to be fairly similar to the output signal in ADC levels.
// 240V AC has a peak-to-peak amplitude of 679V, which is not far from the ideal
// output range.  Stated more formally, the conversion rate of the overall system
// for measuring VOLTAGE is likely to be around 1 ADC-step per Volt (RMS).
//
// In the case of AC current, however, the situation is very different.  At
// mains voltage, a power of 3kW corresponds to an RMS current of 12.5A which
// has a peak-to-peak range of 35A.  This is smaller than the output signal by
// around a factor of twenty.  The conversion rate of the overall system for
// measuring CURRENT is therefore likely to be around 20 ADC-steps per Amp.
//
// When calculating "real power", which is what this code does, the individual
// conversion rates for voltage and current are not of importance.  It is
// only the conversion rate for POWER which is important.  This is the
// product of the individual conversion rates for voltage and current.  It
// therefore has the units of ADC-steps squared per Watt.  Most systems will
// have a power conversion rate of around 20 (ADC-steps squared per Watt).
//
// powerCal is the RECIPR0CAL of the power conversion rate.  A good value
// to start with is therefore 1/20 = 0.05 (Watts per ADC-step squared)
//
constexpr float powerCal_grid{0.0435f};     /**< for CT1 */
constexpr float powerCal_diverted{0.0435f}; /**< for CT2 */

constexpr int32_t capacityOfEnergyBucket_long{(int32_t)WORKING_ZONE_IN_JOULES * CYCLES_PER_SECOND * (1 / powerCal_grid)}; /**< depends on powerCal, frequency & the 'sweetzone' size. */
int32_t energyInBucket_long{0};                                                                                           // in Integer Energy Units

constexpr int32_t nominalEnergyThreshold{capacityOfEnergyBucket_long * 0.5};
constexpr int32_t workingEnergyThreshold_upper{nominalEnergyThreshold}; // initial value
constexpr int32_t workingEnergyThreshold_lower{nominalEnergyThreshold}; // initial value

int32_t divertedEnergyRecent_IEU = 0; // Hi-res accumulator of limited range
uint16_t divertedEnergyTotal_Wh = 0;  // WattHour register of 63K range

// For recording the accumulated amount of diverted energy data (using CT2), a similar
// calibration mechanism is required.  Rather than a bucket with a fixed capacity, the
// accumulator for diverted energy just needs to be scaled correctly.  As soon as its
// value exceeds 1 Wh, an associated WattHour register is incremented, and the
// accumulator's value is decremented accordingly. The calculation below is to determine
// the scaling for this accumulator.
constexpr int32_t IEU_per_Wh{(int32_t)JOULES_PER_WATT_HOUR * CYCLES_PER_SECOND * (1 / powerCal_diverted)}; /**< depends on powerCal, frequency & the 'sweetzone' size. */

// to avoid the diverted energy accumulator 'creeping' when the load is not active
constexpr int32_t antiCreepLimit_inIEUperMainsCycle{(float)ANTI_CREEP_LIMIT * (1 / powerCal_grid)};
constexpr int32_t mainsCyclesPerHour{(int32_t)CYCLES_PER_SECOND * 3600};
constexpr uint32_t displayShutdown_inMainsCycles{DISPLAY_SHUTDOWN_IN_HOURS * mainsCyclesPerHour};
constexpr int32_t requiredExportPerMainsCycle_inIEU{(int32_t)REQUIRED_EXPORT_IN_WATTS * (1 / powerCal_grid)};

uint32_t absenceOfDivertedEnergyCount = 0;

#define POST_TRANSITION_MAX_COUNT 3 // <-- allows each transition to take effect

// for interaction between the main processor and the ISRs
volatile boolean dataReady = false;
volatile int16_t sampleI_grid;
volatile int16_t sampleI_diverted;
volatile int16_t sampleV;

// For an enhanced polarity detection mechanism, which includes a persistence check
#define PERSISTENCE_FOR_POLARITY_CHANGE 1 // sample sets
Polarities polarityOfMostRecentVsample;
Polarities polarityConfirmed;
Polarities polarityConfirmedOfLastSampleV;

// For a mechanism to check the continuity of the sampling sequence
#define CONTINUITY_CHECK_MAXCOUNT 250 // mains cycles
int16_t sampleCount_forContinuityChecker;
int16_t sampleSetsDuringThisMainsCycle;
int16_t lowestNoOfSampleSetsPerMainsCycle;

// for this go-faster sketch, the phaseCal logic has been removed.  If required, it can be
// found in most of the standard Mk2_bothDisplay_n versions

// Various settings for the 4-digit display, which needs to be refreshed every few mS
constexpr uint8_t noOfDigitLocations{4};
constexpr uint8_t noOfPossibleCharacters{22};
constexpr uint8_t MAX_DISPLAY_TIME_COUNT { 10 }           // no of processing loops between display updates
constexpr uint8_t UPDATE_PERIOD_FOR_DISPLAYED_DATA { 50 } // mains cycles
constexpr uint8_t DISPLAY_SHUTDOWN_IN_HOURS { 8 }         // auto-reset after this period of inactivity
// #define DISPLAY_SHUTDOWN_IN_HOURS 0.01 // for testing that the display clears after 36 seconds

//  The two versions of the hardware require different logic.
#ifdef PIN_SAVING_HARDWARE

#define DRIVER_CHIP_DISABLED HIGH
#define DRIVER_CHIP_ENABLED LOW

// the primary segments are controlled by a pair of logic chips
const uint8_t noOfDigitSelectionLines = 4; // <- for the 74HC4543 7-segment display driver
const uint8_t noOfDigitLocationLines = 2;  // <- for the 74HC138 2->4 line demultiplexer

uint8_t enableDisableLine = 5; // <- affects the primary 7 segments only (not the DP)
uint8_t decimalPointLine = 14; // <- this line has to be individually controlled.

uint8_t digitLocationLine[noOfDigitLocationLines] = {16, 15};
uint8_t digitSelectionLine[noOfDigitSelectionLines] = {7, 9, 8, 6};

// The final column of digitValueMap[] is for the decimal point status.  In this version,
// the decimal point has to be treated differently than the other seven segments, so
// a convenient means of accessing this column is provided.
//
uint8_t digitValueMap[noOfPossibleCharacters][noOfDigitSelectionLines + 1] = {
    LOW, LOW, LOW, LOW, LOW,     // '0' <- element 0
    LOW, LOW, LOW, HIGH, LOW,    // '1' <- element 1
    LOW, LOW, HIGH, LOW, LOW,    // '2' <- element 2
    LOW, LOW, HIGH, HIGH, LOW,   // '3' <- element 3
    LOW, HIGH, LOW, LOW, LOW,    // '4' <- element 4
    LOW, HIGH, LOW, HIGH, LOW,   // '5' <- element 5
    LOW, HIGH, HIGH, LOW, LOW,   // '6' <- element 6
    LOW, HIGH, HIGH, HIGH, LOW,  // '7' <- element 7
    HIGH, LOW, LOW, LOW, LOW,    // '8' <- element 8
    HIGH, LOW, LOW, HIGH, LOW,   // '9' <- element 9
    LOW, LOW, LOW, LOW, HIGH,    // '0.' <- element 10
    LOW, LOW, LOW, HIGH, HIGH,   // '1.' <- element 11
    LOW, LOW, HIGH, LOW, HIGH,   // '2.' <- element 12
    LOW, LOW, HIGH, HIGH, HIGH,  // '3.' <- element 13
    LOW, HIGH, LOW, LOW, HIGH,   // '4.' <- element 14
    LOW, HIGH, LOW, HIGH, HIGH,  // '5.' <- element 15
    LOW, HIGH, HIGH, LOW, HIGH,  // '6.' <- element 16
    LOW, HIGH, HIGH, HIGH, HIGH, // '7.' <- element 17
    HIGH, LOW, LOW, LOW, HIGH,   // '8.' <- element 18
    HIGH, LOW, LOW, HIGH, HIGH,  // '9.' <- element 19
    HIGH, HIGH, HIGH, HIGH, LOW, // ' '  <- element 20
    HIGH, HIGH, HIGH, HIGH, HIGH // '.'  <- element 21
};

// a tidy means of identifying the DP status data when accessing the above table
const uint8_t DPstatus_columnID = noOfDigitSelectionLines;

uint8_t digitLocationMap[noOfDigitLocations][noOfDigitLocationLines] = {
    LOW, LOW,   // Digit 1
    LOW, HIGH,  // Digit 2
    HIGH, LOW,  // Digit 3
    HIGH, HIGH, // Digit 4
};

#else // PIN_SAVING_HARDWARE

#define ON HIGH
#define OFF LOW

const uint8_t noOfSegmentsPerDigit = 8; // includes one for the decimal point
enum digitEnableStates
{
  DIGIT_ENABLED,
  DIGIT_DISABLED
};

uint8_t digitSelectorPin[noOfDigitLocations] = {16, 10, 13, 11};
uint8_t segmentDrivePin[noOfSegmentsPerDigit] = {2, 5, 12, 6, 7, 9, 8, 14};

// The final column of segMap[] is for the decimal point status.  In this version,
// the decimal point is treated just like all the other segments, so there is
// no need to access this column specifically.
//
uint8_t segMap[noOfPossibleCharacters][noOfSegmentsPerDigit] = {
    ON, ON, ON, ON, ON, ON, OFF, OFF,       // '0' <- element 0
    OFF, ON, ON, OFF, OFF, OFF, OFF, OFF,   // '1' <- element 1
    ON, ON, OFF, ON, ON, OFF, ON, OFF,      // '2' <- element 2
    ON, ON, ON, ON, OFF, OFF, ON, OFF,      // '3' <- element 3
    OFF, ON, ON, OFF, OFF, ON, ON, OFF,     // '4' <- element 4
    ON, OFF, ON, ON, OFF, ON, ON, OFF,      // '5' <- element 5
    ON, OFF, ON, ON, ON, ON, ON, OFF,       // '6' <- element 6
    ON, ON, ON, OFF, OFF, OFF, OFF, OFF,    // '7' <- element 7
    ON, ON, ON, ON, ON, ON, ON, OFF,        // '8' <- element 8
    ON, ON, ON, ON, OFF, ON, ON, OFF,       // '9' <- element 9
    ON, ON, ON, ON, ON, ON, OFF, ON,        // '0.' <- element 10
    OFF, ON, ON, OFF, OFF, OFF, OFF, ON,    // '1.' <- element 11
    ON, ON, OFF, ON, ON, OFF, ON, ON,       // '2.' <- element 12
    ON, ON, ON, ON, OFF, OFF, ON, ON,       // '3.' <- element 13
    OFF, ON, ON, OFF, OFF, ON, ON, ON,      // '4.' <- element 14
    ON, OFF, ON, ON, OFF, ON, ON, ON,       // '5.' <- element 15
    ON, OFF, ON, ON, ON, ON, ON, ON,        // '6.' <- element 16
    ON, ON, ON, OFF, OFF, OFF, OFF, ON,     // '7.' <- element 17
    ON, ON, ON, ON, ON, ON, ON, ON,         // '8.' <- element 18
    ON, ON, ON, ON, OFF, ON, ON, ON,        // '9.' <- element 19
    OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF, // ' ' <- element 20
    OFF, OFF, OFF, OFF, OFF, OFF, OFF, ON   // '.' <- element 11
};
#endif // PIN_SAVING_HARDWARE

uint8_t charsForDisplay[noOfDigitLocations] = {20, 20, 20, 20}; // all blank

boolean EDD_isActive = false; // energy divertion detection

void setup()
{
  //  pinMode(outOfRangeIndication, OUTPUT);
  //  digitalWrite (outOfRangeIndication, LED_OFF);

  pinMode(physicalLoad_0_pin, OUTPUT); // driver pin for the primary load
  pinMode(physicalLoad_1_pin, OUTPUT); // driver pin for an additional load
  //
  for (int i = 0; i < NO_OF_DUMPLOADS; i++) // re-using the logic from my multiLoad code
  {
    logicalLoadState[i] = LOAD_OFF;
    physicalLoadState[i] = LOAD_OFF;
  }
  //
  digitalWrite(physicalLoad_0_pin, physicalLoadState[0]);  // the primary load is active low.
  digitalWrite(physicalLoad_1_pin, !physicalLoadState[1]); // the additional load is active high.

  delay(delayBeforeSerialStarts); // allow time to open Serial monitor

  Serial.begin(9600);
  Serial.println();
  Serial.println("-------------------------------------");
  Serial.println("Sketch ID:      Mk2_fasterControl_twoLoads_2.ino");
  Serial.println();

#ifdef PIN_SAVING_HARDWARE
  // configure the IO drivers for the 4-digit display
  //
  // the Decimal Point line is driven directly from the processor
  pinMode(decimalPointLine, OUTPUT); // the 'decimal point' line

  // set up the control lines for the 74HC4543 7-seg display driver
  for (int i = 0; i < noOfDigitSelectionLines; i++)
  {
    pinMode(digitSelectionLine[i], OUTPUT);
  }

  // an enable line is required for the 74HC4543 7-seg display driver
  pinMode(enableDisableLine, OUTPUT); // for the 74HC4543 7-seg display driver
  digitalWrite(enableDisableLine, DRIVER_CHIP_DISABLED);

  // set up the control lines for the 74HC138 2->4 demux
  for (int i = 0; i < noOfDigitLocationLines; i++)
  {
    pinMode(digitLocationLine[i], OUTPUT);
  }
#else
  for (int i = 0; i < noOfSegmentsPerDigit; i++)
  {
    pinMode(segmentDrivePin[i], OUTPUT);
  }

  for (int i = 0; i < noOfDigitLocations; i++)
  {
    pinMode(digitSelectorPin[i], OUTPUT);
  }

  for (int i = 0; i < noOfDigitLocations; i++)
  {
    digitalWrite(digitSelectorPin[i], DIGIT_DISABLED);
  }

  for (int i = 0; i < noOfSegmentsPerDigit; i++)
  {
    digitalWrite(segmentDrivePin[i], OFF);
  }
#endif

  // When using integer maths, calibration values that have supplied in floating point
  // form need to be rescaled.

  // When using integer maths, the SIZE of the ENERGY BUCKET is altered to match the
  // scaling of the energy detection mechanism that is in use.  This avoids the need
  // to re-scale every energy contribution, thus saving processing time.  This process
  // is described in more detail in the function, allGeneralProcessing(), just before
  // the energy bucket is updated at the start of each new cycle of the mains.
  //
  // An electricity meter has a small range over which energy can ebb and flow without
  // penalty.  This has been termed its "sweet-zone".  For optimal performance, the energy
  // bucket of a PV Router should match this value.  The sweet-zone value is therefore
  // included in the calculation below.
  //
  // For the flow of energy at the 'grid' connection point (CT1)

  // Define operating limits for the LP filter which identifies DC offset in the voltage
  // sample stream.  By limiting the output range, the filter always should start up
  // correctly.
  DCoffset_V_long = 512L * 256; // nominal mid-point value of ADC @ x256 scale

  Serial.print("ADC mode:       ");
  Serial.print(ADC_TIMER_PERIOD);
  Serial.println(" uS fixed timer");

  // Set up the ADC to be triggered by a hardware timer of fixed duration
  ADCSRA = (1 << ADPS0) + (1 << ADPS1) + (1 << ADPS2); // Set the ADC's clock to system clock / 128
  ADCSRA |= (1 << ADEN);                               // Enable ADC

  Timer1.initialize(ADC_TIMER_PERIOD); // set Timer1 interval
  Timer1.attachInterrupt(timerIsr);    // declare timerIsr() as interrupt service routine

  Serial.print("powerCal_grid =      ");
  Serial.println(powerCal_grid, 4);
  Serial.print("powerCal_diverted = ");
  Serial.println(powerCal_diverted, 4);

  Serial.print("Anti-creep limit (Joules / mains cycle) = ");
  Serial.println(ANTI_CREEP_LIMIT);
  Serial.print("Export rate (Watts) = ");
  Serial.println(REQUIRED_EXPORT_IN_WATTS);

  Serial.print("zero-crossing persistence (sample sets) = ");
  Serial.println(PERSISTENCE_FOR_POLARITY_CHANGE);
  Serial.print("continuity sampling display rate (mains cycles) = ");
  Serial.println(CONTINUITY_CHECK_MAXCOUNT);

  Serial.print("  capacityOfEnergyBucket_long = ");
  Serial.println(capacityOfEnergyBucket_long);
  Serial.print("  nominalEnergyThreshold   = ");
  Serial.println(nominalEnergyThreshold);

  Serial.print(">>free RAM = ");
  Serial.println(freeRam()); // a useful value to keep an eye on

  Serial.println("----");
}

// An Interrupt Service Routine is now defined in which the ADC is instructed to
// measure each analogue input in sequence.  A "data ready" flag is set after each
// voltage conversion has been completed.
//   For each set of samples, the two samples for current  are taken before the one
// for voltage.  This is appropriate because each waveform current is generally slightly
// advanced relative to the waveform for voltage.  The data ready flag is cleared
// within loop().
//   This Interrupt Service Routine is for use when the ADC is fixed timer mode.  It is
// executed whenever the ADC timer expires.  In this mode, the next ADC conversion is
// initiated from within this ISR.
//
void timerIsr(void)
{
  static unsigned char sample_index = 0;
  static int sampleI_grid_raw;
  static int sampleI_diverted_raw;

  switch (sample_index)
  {
  case 0:
    sampleV = ADC;                         // store the ADC value (this one is for Voltage)
    ADMUX = 0x40 + currentSensor_diverted; // set up the next conversion, which is for Diverted Current
    ADCSRA |= (1 << ADSC);                 // start the ADC
    sample_index++;                        // increment the control flag
    sampleI_diverted = sampleI_diverted_raw;
    sampleI_grid = sampleI_grid_raw;
    dataReady = true; // all three ADC values can now be processed
    break;
  case 1:
    sampleI_diverted_raw = ADC;        // store the ADC value (this one is for Diverted Current)
    ADMUX = 0x40 + currentSensor_grid; // set up the next conversion, which is for Grid Current
    ADCSRA |= (1 << ADSC);             // start the ADC
    sample_index++;                    // increment the control flag
    break;
  case 2:
    sampleI_grid_raw = ADC;       // store the ADC value (this one is for Grid Current)
    ADMUX = 0x40 + voltageSensor; // set up the next conversion, which is for Voltage
    ADCSRA |= (1 << ADSC);        // start the ADC
    sample_index = 0;             // reset the control flag
    break;
  default:
    sample_index = 0; // to prevent lockup (should never get here)
  }
}

// When using interrupt-based logic, the main processor waits in loop() until the
// dataReady flag has been set by the ADC.  Once this flag has been set, the main
// processor clears the flag and proceeds with all the processing for one set of
// V & I samples.  It then returns to loop() to wait for the next set to become
// available.
//   If there is insufficient processing capacity to do all that is required, the
//  workload rate can be reduced by increasing the duration of ADC_TIMER_PERIOD.
//
void loop()
{
  if (dataReady) // flag is set after every set of ADC conversions
  {
    dataReady = false;      // reset the flag
    allGeneralProcessing(); // executed once for each set of V&I samples
  }
} // end of loop()

// This routine is called to process each set of V & I samples. The main processor and
// the ADC work autonomously, their operation being only linked via the dataReady flag.
// As soon as a new set of data is made available by the ADC, the main processor can
// start to work on it immediately.
//
void allGeneralProcessing()
{
  static int32_t sumP_grid;                // for per-cycle summation of 'real power'
  static int32_t sumP_diverted;            // for per-cycle summation of 'real power'
  static int32_t cumVdeltasThisCycle_long; // for the LPF which determines DC offset (voltage)
  static uint8_t timerForDisplayUpdate = 0;
  static int sampleSetsDuringNegativeHalfOfMainsCycle; // for arming the triac/trigger
  static int32_t energyInBucket_prediction;
  static bool loadHasJustChangedState;
  static boolean recentTransition;
  static uint8_t postTransitionCount;
  static uint8_t activeLoad = 0;

  // remove DC offset from the raw voltage sample by subtracting the accurate value
  // as determined by a LP filter.
  int32_t sampleVminusDC_long = ((int32_t)sampleV << 8) - DCoffset_V_long;

  // determine the polarity of the latest voltage sample
  if (sampleVminusDC_long > 0)
  {
    polarityOfMostRecentVsample = POSITIVE;
  }
  else
  {
    polarityOfMostRecentVsample = NEGATIVE;
  }
  confirmPolarity();

  if (polarityConfirmed == POSITIVE)
  {
    if (polarityConfirmedOfLastSampleV != POSITIVE)
    {
      // This is the start of a new +ve half cycle (just after the zero-crossing point)
      if (beyondStartUpPhase)
      {
        // a simple routine for checking the performance of this new ISR structure
        if (sampleSetsDuringThisMainsCycle < lowestNoOfSampleSetsPerMainsCycle)
        {
          lowestNoOfSampleSetsPerMainsCycle = sampleSetsDuringThisMainsCycle;
        }

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
        // a 'int32_t') is therefore (1/powerCal) times larger than the actual power in Watts.
        //
        int32_t realPower_grid = sumP_grid / sampleSetsDuringThisMainsCycle;         // proportional to Watts
        int32_t realPower_diverted = sumP_diverted / sampleSetsDuringThisMainsCycle; // proportional to Watts

        realPower_grid -= requiredExportPerMainsCycle_inIEU; // <- useful for PV simulation

        // Next, the energy content of this power rating needs to be determined.  Energy is
        // power multiplied by time, so the next step would normally be to multiply the measured
        // value of power by the time over which it was measured.
        //   Instanstaneous power is calculated once every mains cycle. When integer maths is
        // being used, a repetitive power-to-energy conversion seems an unnecessary workload.
        // As all sampling periods are of similar duration, it is more efficient to just
        // add all of the power samples together, and note that their sum is actually
        // CYCLES_PER_SECOND greater than it would otherwise be.
        //   Although the numerical value itself does not change, I thought that a new name
        // may be helpful so as to minimise confusion.
        //   The 'energy' variable below is CYCLES_PER_SECOND * (1/powerCal) times larger than
        // the actual energy in Joules.
        //
        int32_t realEnergy_grid = realPower_grid;
        int32_t realEnergy_diverted = realPower_diverted;

        // Energy contributions from the grid connection point (CT1) are summed in an
        // accumulator which is known as the energy bucket.  The purpose of the energy bucket
        // is to mimic the operation of the supply meter.  The range over which energy can
        // pass to and fro without loss or charge to the user is known as its 'sweet-zone'.
        // The capacity of the energy bucket is set to this same value within setup().
        //
        // The latest contribution can now be added to this energy bucket
        energyInBucket_long += realEnergy_grid;

        // Apply max and min limits to bucket's level.  This is to ensure correct operation
        // when conditions change, i.e. when import changes to export, and vici versa.
        //
        bool endOfRangeEncountered = false;
        if (energyInBucket_long > capacityOfEnergyBucket_long)
        {
          energyInBucket_long = capacityOfEnergyBucket_long;
          endOfRangeEncountered = true;
        }
        else if (energyInBucket_long < 0)
        {
          energyInBucket_long = 0;
          endOfRangeEncountered = true;
        }

        /*
        if (endOfRangeEncountered) {
          digitalWrite (outOfRangeIndication , LED_ON); }
        else {
          digitalWrite (outOfRangeIndication , LED_OFF); }            
*/
        if (EDD_isActive) // Energy Diversion Display
        {
          // For diverted energy, the latest contribution needs to be added to an
          // accumulator which operates with maximum precision.

          if (realEnergy_diverted < antiCreepLimit_inIEUperMainsCycle)
          {
            realEnergy_diverted = 0;
          }
          divertedEnergyRecent_IEU += realEnergy_diverted;

          // Whole kWhours are then recorded separately
          if (divertedEnergyRecent_IEU > IEU_per_Wh)
          {
            divertedEnergyRecent_IEU -= IEU_per_Wh;
            divertedEnergyTotal_Wh++;
          }
        }

        if (timerForDisplayUpdate > UPDATE_PERIOD_FOR_DISPLAYED_DATA)
        { // the 4-digit display needs to be refreshed every few mS. For convenience,
          // this action is performed every N times around this processing loop.
          timerForDisplayUpdate = 0;

          // After a pre-defined period of inactivity, the 4-digit display needs to
          // close down in readiness for the next's day's data.
          //
          if (absenceOfDivertedEnergyCount > displayShutdown_inMainsCycles)
          {
            // clear the accumulators for diverted energy
            divertedEnergyTotal_Wh = 0;
            divertedEnergyRecent_IEU = 0;
            EDD_isActive = false; // energy diversion detector is now inactive
          }

          configureValueForDisplay();
        }
        else
        {
          timerForDisplayUpdate++;
        }

        // continuity checker
        sampleCount_forContinuityChecker++;
        if (sampleCount_forContinuityChecker >= CONTINUITY_CHECK_MAXCOUNT)
        {
          sampleCount_forContinuityChecker = 0;
          Serial.println(lowestNoOfSampleSetsPerMainsCycle);
          lowestNoOfSampleSetsPerMainsCycle = 999;
        }

        // clear the per-cycle accumulators for use in this new mains cycle.
        sampleSetsDuringThisMainsCycle = 0;
        sumP_grid = 0;
        sumP_diverted = 0;
        sampleSetsDuringNegativeHalfOfMainsCycle = 0;
      }
      else
      {
        // wait until the DC-blocking filters have had time to settle
        if (millis() > (delayBeforeSerialStarts + startUpPeriod))
        {
          beyondStartUpPhase = true;
          sumP_grid = 0;
          sumP_diverted = 0;
          sampleSetsDuringThisMainsCycle = 0;   // not yet dealt with for this cycle
          sampleCount_forContinuityChecker = 1; // opportunity has been missed for this cycle
          lowestNoOfSampleSetsPerMainsCycle = 999;
          Serial.println("Go!");
        }
      }
    } // end of processing that is specific to the first Vsample in each +ve half cycle

    // still processing samples where the voltage is POSITIVE ...
    if (sampleSetsDuringThisMainsCycle == 5) // to distribute the workload within each mains cycle
    {
      if (beyondStartUpPhase)
      {
        // Restrictions apply for the period immediately after a load has been switched.
        // Here the recentTransition flag is checked and updated as necessary.
        if (recentTransition)
        {
          postTransitionCount++;
          if (postTransitionCount >= POST_TRANSITION_MAX_COUNT)
          {
            recentTransition = false;
          }
        }

        if (recentTransition)
        {
          // During the post-transition period, any change in the energy level is noted.
          if (energyInBucket_long > workingEnergyThreshold_upper)
          {
            workingEnergyThreshold_lower = nominalEnergyThreshold; // reset the opposite threshold
            workingEnergyThreshold_upper = energyInBucket_long;

            // the energy thresholds must remain within range
            if (workingEnergyThreshold_upper > capacityOfEnergyBucket_long)
            {
              workingEnergyThreshold_upper = capacityOfEnergyBucket_long;
            }
          }
          else if (energyInBucket_long < workingEnergyThreshold_lower)
          {
            workingEnergyThreshold_upper = nominalEnergyThreshold; // reset the opposite threshold
            workingEnergyThreshold_lower = energyInBucket_long;

            // the energy thresholds must remain within range
            if (workingEnergyThreshold_lower < 0)
            {
              workingEnergyThreshold_lower = 0;
            }
          }
        }
      }
    }
  } // end of processing that is specific to samples where the voltage is positive

  else // the polatity of this sample is negative
  {
    if (polarityConfirmedOfLastSampleV != NEGATIVE)
    {
      // This is the start of a new -ve half cycle (just after the zero-crossing point)
      // which is a convenient point to update the Low Pass Filter for DC-offset removal
      //  The portion which is fed back into the integrator is approximately one percent
      // of the average offset of all the Vsamples in the previous mains cycle.
      //
      int32_t previousOffset = DCoffset_V_long;
      DCoffset_V_long = previousOffset + (cumVdeltasThisCycle_long >> 12);
      cumVdeltasThisCycle_long = 0;

      // To ensure that the LPF will always start up correctly when 240V AC is available, its
      // output value needs to be prevented from drifting beyond the likely range of the
      // voltage signal.  This avoids the need to use a HPF as was done for initial Mk2 builds.
      //
      if (DCoffset_V_long < DCoffset_V_min)
      {
        DCoffset_V_long = DCoffset_V_min;
      }
      else if (DCoffset_V_long > DCoffset_V_max)
      {
        DCoffset_V_long = DCoffset_V_max;
      }

      // The average power that has been measured during the first half of this mains cycle can now be used
      // to predict the energy state at the end of this mains cycle.  That prediction will be used to alter
      // the state of the load as necessary. The arming signal for the triac can't be set yet - that must
      // wait until the voltage has advanced further beyond the -ve going zero-crossing point.
      //
      int32_t averagePower = sumP_grid / sampleSetsDuringThisMainsCycle; // for 1st half of this mains cycle only
      //
      // To avoid repetitive and unnecessary calculations, the increase in energy during each mains cycle is
      // deemed to be numerically equal to the average power.  The predicted value for the energy state at the
      // end of this mains cycle will therefore be the known energy state at its start plus the average power
      // as measured. Although the average power has been determined over only half a mains cycle, the correct
      // number of contributing sample sets has been used so the result can be expected to be a true measurement
      // of average power, not half of it.
      // However, it can be shown that the average power during the first half of any mains cycle after the
      // load has changed state will alway be under-recorded so its value should now be increased by 30%.  This
      // arbitrary looking adjustment gives good test results with differening amounts of surplus power and it
      // only affects the predicted value of the energy state at the end of the current mains cycle; it does
      // not affect the value in the main energy bucket.  This complication is a fundamental consequence
      // of the floating CTs that we use.
      //
      if (loadHasJustChangedState)
      {
        averagePower = averagePower * 1.3;
      }
      energyInBucket_prediction = energyInBucket_long + averagePower; // at end of this mains cycle

    } // end of processing that is specific to the first Vsample in each -ve half cycle

    if (sampleSetsDuringNegativeHalfOfMainsCycle == 5)
    {
      // the zero-crossing trigger device(s) can now be reliably armed
      if (beyondStartUpPhase)
      {
        /* Determining whether any of the loads need to be changed is is a 3-stage process:
         * - change the LOGICAL load states as necessary to maintain the energy level
         * - update the PHYSICAL load states according to the logical -> physical mapping 
         * - update the driver lines for each of the loads.
         */

        loadHasJustChangedState = false; // clear the predictive algorithm's flag
        if (energyInBucket_prediction > workingEnergyThreshold_upper)
        {
          // the predicted energy state is high so an extra load may need to be added
          boolean OK_toAddLoad = true; // default state of flag

          uint8_t tempLoad = nextLogicalLoadToBeAdded();
          if (tempLoad < NO_OF_DUMPLOADS)
          {
            // a load which is now OFF has been identified for potentially being switched ON
            if (recentTransition)
            {
              if (tempLoad != activeLoad)
              {
                OK_toAddLoad = false;
              }
            }

            if (OK_toAddLoad)
            {
              // tempLoad will be either the active load, or the next load in the priority sequence
              logicalLoadState[tempLoad] = LOAD_ON;
              activeLoad = tempLoad;
              postTransitionCount = 0;
              recentTransition = true;
              loadHasJustChangedState = true;
            }
          }
        }
        else if (energyInBucket_prediction < workingEnergyThreshold_lower)
        {
          // the predicted energy state is low so some load may need to be removed
          boolean OK_toRemoveLoad = true; // default state of flag

          uint8_t tempLoad = nextLogicalLoadToBeRemoved();
          if (tempLoad < NO_OF_DUMPLOADS)
          {
            // a load which is now ON has been identified for potentially being switched OFF
            if (recentTransition)
            {
              if (tempLoad != activeLoad)
              {
                OK_toRemoveLoad = false;
              }
            }

            if (OK_toRemoveLoad)
            {
              // tempLoad will be either the active load, or the next load in the priority sequence
              logicalLoadState[tempLoad] = LOAD_OFF;
              activeLoad = tempLoad;
              postTransitionCount = 0;
              recentTransition = true;
              loadHasJustChangedState = true;
            }
          }
        }

        updatePhysicalLoadStates(); // allows the logical-to-physical mapping to be changed

        // update each of the physical loads
        digitalWrite(physicalLoad_0_pin, physicalLoadState[0]);  // active low for trigger
        digitalWrite(physicalLoad_1_pin, !physicalLoadState[1]); // active high for additional load

        // update the Energy Diversion Detector
        if (physicalLoadState[0] == LOAD_ON)
        {
          absenceOfDivertedEnergyCount = 0;
          EDD_isActive = true;
        }
        else
        {
          absenceOfDivertedEnergyCount++;
        }
      }
    }

    sampleSetsDuringNegativeHalfOfMainsCycle++;
  } // end of processing that is specific to samples where the voltage is negative

  // processing for EVERY set of samples
  //
  // First, deal with the power at the grid connection point (as measured via CT1)
  // remove most of the DC offset from the current sample (the precise value does not matter)
  int32_t sampleIminusDC_grid = ((int32_t)(sampleI_grid - DCoffset_I)) << 8;

  // calculate the "real power" in this sample pair and add to the accumulated sum
  int32_t filtV_div4 = sampleVminusDC_long >> 2; // reduce to 16-bits (now x64, or 2^6)
  int32_t filtI_div4 = sampleIminusDC_grid >> 2; // reduce to 16-bits (now x64, or 2^6)
  int32_t instP = filtV_div4 * filtI_div4;       // 32-bits (now x4096, or 2^12)
  instP = instP >> 12;                           // scaling is now x1, as for Mk2 (V_ADC x I_ADC)
  sumP_grid += instP;                            // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)

  // Now deal with the diverted power (as measured via CT2)
  // remove most of the DC offset from the current sample (the precise value does not matter)
  int32_t sampleIminusDC_diverted = ((int32_t)(sampleI_diverted - DCoffset_I)) << 8;

  // calculate the "real power" in this sample pair and add to the accumulated sum
  filtI_div4 = sampleIminusDC_diverted >> 2; // reduce to 16-bits (now x64, or 2^6)
  instP = filtV_div4 * filtI_div4;           // 32-bits (now x4096, or 2^12)
  instP = instP >> 12;                       // scaling is now x1, as for Mk2 (V_ADC x I_ADC)
  sumP_diverted += instP;                    // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)

  sampleSetsDuringThisMainsCycle++;

  // store items for use during next loop
  cumVdeltasThisCycle_long += sampleVminusDC_long;    // for use with LP filter
  polarityConfirmedOfLastSampleV = polarityConfirmed; // for identification of half cycle boundaries

  refreshDisplay();
}
//  ----- end of main Mk2i code -----

void confirmPolarity()
{
  /* This routine prevents a zero-crossing point from being declared until 
   * a certain number of consecutive samples in the 'other' half of the 
   * waveform have been encountered.  
   */
  static uint8_t count{0};

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
 * @brief Retrieve the next load that could be added (be aware of the order)
 * 
 * @return The load number if successfull, NO_OF_DUMPLOADS in case of failure 
 * 
 * @ingroup TimeCritical
 */
uint8_t nextLogicalLoadToBeAdded()
{
  for (uint8_t index = 0; index < NO_OF_DUMPLOADS; ++index)
    if (logicalLoadState[index] == LOAD_OFF)
      return index;

  return (NO_OF_DUMPLOADS);
}

/**
 * @brief Retrieve the next load that could be removed (be aware of the reverse-order)
 * 
 * @return The load number if successfull, NO_OF_DUMPLOADS in case of failure 
 * 
 * @ingroup TimeCritical
 */
uint8_t nextLogicalLoadToBeRemoved()
{
  uint8_t index{NO_OF_DUMPLOADS};
  do
  {
    --index;
    if (logicalLoadState[index] == LOAD_ON)

      return index;
  } while (0 != index);

  return (NO_OF_DUMPLOADS);
}

/*
 * This function provides the link between the logical and physical loads.  The 
 * array, logicalLoadState[], contains the on/off state of all logical loads, with 
 * element 0 being for the one with the highest priority.  The array, 
 * physicalLoadState[], contains the on/off state of all physical loads. 
 * 
 * The association between the physical and logical loads is 1:1.  By default, numerical
 * equivalence is maintained, so logical(N) maps to physical(N).  If physical load 1 is set 
 * to have priority, rather than physical load 0, the logical-to-physical association for 
 * loads 0 and 1 are swapped.
 *
 * Any other mapping relaionships could be configured here.
 */
void updatePhysicalLoadStates()
{
  for (int i = 0; i < NO_OF_DUMPLOADS; i++)
    physicalLoadState[i] = logicalLoadState[i];
}

// called infrequently, to update the characters to be displayed
void configureValueForDisplay()
{
  static uint8_t locationOfDot = 0;

  //  Serial.println(divertedEnergyTotal_Wh);

  if (EDD_isActive)
  {
    uint16_t val = divertedEnergyTotal_Wh;
    boolean energyValueExceeds10kWh;

    if (val < 10000)
    {
      // no need to re-scale (display to 3 DPs)
      energyValueExceeds10kWh = false;
    }
    else
    {
      // re-scale is needed (display to 2 DPs)
      energyValueExceeds10kWh = true;
      val = val / 10;
    }

    uint8_t thisDigit = val / 1000;
    charsForDisplay[0] = thisDigit;
    val -= 1000 * thisDigit;

    thisDigit = val / 100;
    charsForDisplay[1] = thisDigit;
    val -= 100 * thisDigit;

    thisDigit = val / 10;
    charsForDisplay[2] = thisDigit;
    val -= 10 * thisDigit;

    charsForDisplay[3] = val;

    // assign the decimal point location
    if (energyValueExceeds10kWh)
    {
      charsForDisplay[1] += 10;
    } // dec point after 2nd digit
    else
    {
      charsForDisplay[0] += 10;
    } // dec point after 1st digit
  }
  else
  {
    // "walking dots" display
    charsForDisplay[locationOfDot] = 20; // blank

    locationOfDot++;
    if (locationOfDot >= noOfDigitLocations)
    {
      locationOfDot = 0;
    }

    charsForDisplay[locationOfDot] = 21; // dot
  }
}

void refreshDisplay()
{
  // This routine keeps track of which digit is being displayed and checks when its
  // display time has expired.  It then makes the necessary adjustments for displaying
  // the next digit.
  //   The two versions of the hardware require different logic.

#ifdef PIN_SAVING_HARDWARE
  // With this version of the hardware, care must be taken that all transitory states
  // are masked out.  Note that the enableDisableLine only masks the seven primary
  // segments, not the Decimal Point line which must therefore be treated separately.
  // The sequence is:
  //
  // 1. set the decimal point line to 'off'
  // 2. disable the 7-segment driver chip
  // 3. determine the next location which is to be active
  // 4. set up the location lines for the new active location
  // 5. determine the relevant character for the new active location
  // 6. configure the driver chip for the new character to be displayed
  // 7. set up decimal point line for the new active location
  // 8. enable the 7-segment driver chip

  static uint8_t displayTime_count = 0;
  static uint8_t digitLocationThatIsActive = 0;

  displayTime_count++;

  if (displayTime_count > MAX_DISPLAY_TIME_COUNT)
  {
    uint8_t lineState;

    displayTime_count = 0;

    // 1. disable the Decimal Point driver line;
    digitalWrite(decimalPointLine, LOW);

    // 2. disable the driver chip while changes are taking place
    digitalWrite(enableDisableLine, DRIVER_CHIP_DISABLED);

    // 3. determine the next digit location to be active
    digitLocationThatIsActive++;
    if (digitLocationThatIsActive >= noOfDigitLocations)
    {
      digitLocationThatIsActive = 0;
    }

    // 4. set up the digit location drivers for the new active location
    for (uint8_t line = 0; line < noOfDigitLocationLines; line++)
    {
      lineState = digitLocationMap[digitLocationThatIsActive][line];
      digitalWrite(digitLocationLine[line], lineState);
    }

    // 5. determine the character to be displayed at this new location
    // (which includes the decimal point information)
    uint8_t digitVal = charsForDisplay[digitLocationThatIsActive];

    // 6. configure the 7-segment driver for the character to be displayed
    for (uint8_t line = 0; line < noOfDigitSelectionLines; line++)
    {
      lineState = digitValueMap[digitVal][line];
      digitalWrite(digitSelectionLine[line], lineState);
    }

    // 7. set up the Decimal Point driver line;
    digitalWrite(decimalPointLine, digitValueMap[digitVal][DPstatus_columnID]);

    // 8. enable the 7-segment driver chip
    digitalWrite(enableDisableLine, DRIVER_CHIP_ENABLED);
  }

#else  // PIN_SAVING_HARDWARE

  // This version is more straightforward because the digit-enable lines can be
  // used to mask out all of the transitory states, including the Decimal Point.
  // The sequence is:
  //
  // 1. de-activate the digit-enable line that was previously active
  // 2. determine the next location which is to be active
  // 3. determine the relevant character for the new active location
  // 4. set up the segment drivers for the character to be displayed (includes the DP)
  // 5. activate the digit-enable line for the new active location

  static uint8_t displayTime_count = 0;
  static uint8_t digitLocationThatIsActive = 0;

  displayTime_count++;

  if (displayTime_count > MAX_DISPLAY_TIME_COUNT)
  {
    displayTime_count = 0;

    // 1. de-activate the location which is currently being displayed
    digitalWrite(digitSelectorPin[digitLocationThatIsActive], DIGIT_DISABLED);

    // 2. determine the next digit location which is to be displayed
    digitLocationThatIsActive++;
    if (digitLocationThatIsActive >= noOfDigitLocations)
    {
      digitLocationThatIsActive = 0;
    }

    // 3. determine the relevant character for the new active location
    uint8_t digitVal = charsForDisplay[digitLocationThatIsActive];

    // 4. set up the segment drivers for the character to be displayed (includes the DP)
    for (uint8_t segment = 0; segment < noOfSegmentsPerDigit; segment++)
    {
      uint8_t segmentState = segMap[digitVal][segment];
      digitalWrite(segmentDrivePin[segment], segmentState);
    }

    // 5. activate the digit-enable line for the new active location
    digitalWrite(digitSelectorPin[digitLocationThatIsActive], DIGIT_ENABLED);
  }
#endif // PIN_SAVING_HARDWARE

} // end of refreshDisplay()

int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}