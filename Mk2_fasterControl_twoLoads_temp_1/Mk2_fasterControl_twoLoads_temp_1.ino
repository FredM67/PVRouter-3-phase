/**
 * @file Mk2_fasterControl_twoLoads_temp_1.ino
 * @author Robin Emley (www.Mk2PVrouter.co.uk)
 * @author Frederic Metrich (frederic.metrich@live.fr)
 * @brief Mk2_fasterControl_twoLoads_temp_1.ino - A photovoltaïc energy diverter.
 * @date 2020-04-08
 * 
 * @mainpage A 3-phase photovoltaïc router/diverter
 * 
 * @section description Description
 * Mk2_fasterControl_twoLoads_temp_1.ino - Arduino program that maximizes the use of home photovoltaïc production
 * by monitoring energy consumption and diverting power to one or more resistive charge(s) when needed.
 * In the absence of such a system, surplus energy flows away to the grid and is of no benefit to the PV-owner.
 * 
 * @section history History
 * __Initially released as Mk2_bothDisplays_1 in March 2014.__
 *
 * This sketch is for diverting suplus PV power to a dump load using a triac or  
 * Solid State Relay. It is based on the Mk2i PV Router code that I have posted in on  
 * the OpenEnergyMonitor forum. The original version, and other related material, 
 * can be found on my Summary Page at www.openenergymonitor.org/emon/node/1757
 *
 * In this latest version, the pin-allocations have been changed to suit my 
 * PCB-based hardware for the Mk2 PV Router. The integral voltage sensor is 
 * fed from one of the secondary coils of the transformer. Current is measured 
 * via Current Transformers at the CT1 and CT1 ports. 
 * 
 * CT1 is for 'grid' current, to be measured at the grid supply point.
 * CT2 is for the load current, so that diverted energy can be recorded
 *
 * A persistence-based 4-digit display is supported. This can be driven in two
 * different ways, one with an extra pair of logic chips, and one without. The 
 * appropriate version of the sketch must be selected by including or commenting 
 * out the "#define PIN_SAVING_HARDWARE" statement near the top of the code.
 *
 * __September 2014: renamed as Mk2_bothDisplays_2, with these changes:__
 * - cycleCount removed (was not actually used in this sketch, but could have overflowed);
 * - removal of unhelpful comments in the IO pin section;
 * - tidier initialisation of display logic in setup();
 * - addition of REQUIRED_EXPORT_IN_WATTS logic (useful as a built-in PV simulation facility);
 *
 * __December 2014: renamed as Mk2_bothDisplays_3, with these changes:__
 * - persistence check added for zero-crossing detection (polarityConfirmed)
 * - lowestNoOfSampleSetsPerMainsCycle added, to check for any disturbances
 *
 * __December 2014: renamed as Mk2_bothDisplays_3a, with these changes:__
 * - some typographical errors fixed.
 *
 * __January 2016: renamed as Mk2_bothDisplays_3b, with these changes:__
 * - a minor change in the ISR to remove a timing uncertainty.
 *
 * __January 2016: updated to Mk2_bothDisplays_3c:__
 * -  The variables to store the ADC results are now declared as "volatile" to remove 
 *      any possibility of incorrect operation due to optimisation by the compiler.
 *
 * __February 2016: updated to Mk2_bothDisplays_4, with these changes:__
 * - improvements to the start-up logic. The start of normal operation is now 
 *     synchronised with the start of a new mains cycle.
 * - reduce the amount of feedback in the Low Pass Filter for removing the DC content
 *     from the Vsample stream. This resolves an anomaly which has been present since 
 *     the start of this project. Although the amount of feedback has previously been 
 *     excessive, this anomaly has had minimal effect on the system's overall behaviour.
 * - removal of the unhelpful "triggerNeedsToBeArmed" mechanism
 * - tidying of the "confirmPolarity" logic to make its behaviour more clear
 * - SWEETZONE_IN_JOULES changed to WORKING_RANGE_IN_JOULES 
 * - change "triac" to "load" wherever appropriate
 *
 * __November 2019: updated to Mk2_fasterControl_1 with these changes:__
 * - Half way through each mains cycle, a prediction is made of the likely energy level at the
 *     end of the cycle. That predicted value allows the triac to be switched at the +ve going 
 *     zero-crossing point rather than waiting for a further 10 ms. These changes allow for 
 *     faster switching of the load.
 * - The range of the energy bucket has been reduced to one tenth of its former value. This
 *     allows the unit's operation to commence more rapidly whenever surplus power is available.
 * - controlMode is no longer selectable, the unit's operation being effectively hard-coded 
 *     as "Normal" rather than Anti-flicker. 
 * - Port D3 now supports an indicator which shows when the level in the energy bucket
 *     reaches either end of its range. While the unit is actively diverting surplus power,
 *     it is vital that the level in the reduced capacity energy bucket remains within its 
 *     permitted range, hence the addition of this indicator.
 *   
 * __February 2020: updated to Mk2_fasterControl_twoLoads_1 with these changes:__
 * - the energy overflow indicator has been disabled to free up port D3 
 * - port D3 now supports a second load
 * 
 * __February 2020: updated to Mk2_fasterControl_twoLoads_2 with these changes:__
 * - improved multi-load control logic to prevent the primary load from being disturbed by
 *     the lower priority one. This logic now mirrors that in the Mk2_multiLoad_wired_n line.
 * 
 *   
 *      Robin Emley
 *      www.Mk2PVrouter.co.uk
 *
 * __April 2020: renamed as Mk2_fasterControl_twoLoads__temp_1 with these changes:__
 * - This sketch has been restructured in order to make better use of the ISR.
 * - This sketch has been again re-engineered. All 'defines' have been removed except
 *     the ones for compile-time optional functionalities.
 * - All constants have been replaced with constexpr initialized at compile-time
 * - all number-types have been replaced with fixed width number types
 * - old fashion enums replaced by scoped enums with fixed types
 * - All of the time-critical code is now contained within the ISR and its helper functions.
 * - Values for datalogging are transferred to the main code using a flag-based handshake mechanism.
 * - The diversion of surplus power can no longer be affected by slower
 *     activities which may be running in the main code such as Serial statements and RF.
 * - Temperature sensing is supported. A pullup resistor (4K7 or similar) is required for the Dallas sensor.
 * - Display shows diverted envergy and temperature in a cycling way (5 seconds cycle) 
 * 
 *   Fred Metrich
 *  
 * @copyright Copyright (c) 2020
 * 
*/

#include <Arduino.h>

#define TEMP_SENSOR ///< this line must be commented out if the temperature sensor is not present

#define DATALOG_OUTPUT ///< this line can be commented if no datalogging is needed

#ifdef TEMP_SENSOR
#include <OneWire.h> // for temperature sensing
#endif

// In this sketch, the ADC is free-running with a cycle time of ~104uS.

// Change these values to suit the local mains frequency and supply meter
constexpr int32_t CYCLES_PER_SECOND{50};         /**< number of cycles/s of the grid power supply */
constexpr uint32_t WORKING_RANGE_IN_JOULES{360}; /**< 0.1 Wh, reduced for faster start-up */
constexpr int32_t REQUIRED_EXPORT_IN_WATTS{0};   /**< when set to a Polarities::NEGATIVE value, this acts as a PV generator */

// Physical constants, please do not change!
constexpr int32_t JOULES_PER_WATT_HOUR{3600}; /**< (0.001 kWh = 3600 Joules) */

// to prevent the diverted energy total from 'creeping'
constexpr uint32_t ANTI_CREEP_LIMIT{5}; /**< in Joules per mains cycle (has no effect when set to 0) */

constexpr int32_t DATALOG_PERIOD_IN_MAINS_CYCLES{250}; /**< Period of datalogging in cycles */

constexpr uint8_t MAX_DISPLAY_TIME_COUNT{10};   /**< no of processing loops between display updates */
constexpr uint8_t DISPLAY_SHUTDOWN_IN_HOURS{8}; /**< auto-reset after this period of inactivity */
// #define DISPLAY_SHUTDOWN_IN_HOURS 0.01 // for testing that the display clears after 36 seconds

//  The two versions of the hardware require different logic. The following line should
//  be included if the additional logic chips are present, or excluded if they are
//  absent (in which case some wire links need to be fitted)
//
//#define PIN_SAVING_HARDWARE

constexpr uint8_t NO_OF_DUMPLOADS{2}; /**< number of dump loads connected to the diverter */

#ifdef TEMP_SENSOR
// --------------------------
// Dallas DS18B20 commands
constexpr uint8_t SKIP_ROM{0xcc};
constexpr uint8_t CONVERT_TEMPERATURE{0x44};
constexpr uint8_t READ_SCRATCHPAD{0xbe};
constexpr uint16_t BAD_TEMPERATURE{30000}; /**< this value (300C) is sent if no sensor is present */
#endif

// -------------------------------
// definition of enumerated types

/** Polarities */
enum class Polarities : uint8_t
{
  NEGATIVE, /**< polarity is negative */
  POSITIVE  /**< polarity is positive */
};

/** all loads are logically active-low */
enum class LoadStates : uint8_t
{
  LOAD_ON, /**< load is ON */
  LOAD_OFF /**< load is OFF */
};

LoadStates logicalLoadState[NO_OF_DUMPLOADS];  /**< Logical state of the loads */
LoadStates physicalLoadState[NO_OF_DUMPLOADS]; /**< Physical state of the loads */

/** @brief container for datalogging
 *  @details This class is used for datalogging.
*/
class Tx_struct
{
public:
  int16_t powerAtSupplyPoint_Watts; /**< main power, import = +ve, to match OEM convention */
  int16_t divertedEnergyTotal_Wh;   /**< diverted energy, always positive */
  int16_t Vrms_times100;            /**< average voltage over datalogging period (in 100th of Volt)*/
#ifdef TEMP_SENSOR
  int16_t temperature_times100; /**< temperature in 100th of °C */
#endif
};

Tx_struct tx_data; /**< logging data */

// For this go-faster version, the unit's operation will effectively always be "Normal";
// there is no "Anti-flicker" option. The controlMode variable has been removed.

// allocation of digital pins which are not dependent on the display type that is in use
// *************************************************************************************
// constexpr uint8_t outOfRangeIndication{3}; /**< <-- this output port is active-high */
constexpr uint8_t physicalLoad_1_pin{3}; /**< <-- the "mode" port is active-high */
constexpr uint8_t physicalLoad_0_pin{4}; /**< <-- the "trigger" port is active-low */

// allocation of analogue pins which are not dependent on the display type that is in use
// **************************************************************************************
constexpr uint8_t voltageSensor{3};          /**< A3 is for the voltage sensor */
constexpr uint8_t currentSensor_diverted{4}; /**< A4 is for CT2 which measures diverted current */
constexpr uint8_t currentSensor_grid{5};     /**< A5 is for CT1 which measures grid current */

constexpr uint8_t delayBeforeSerialStarts{1000}; /**< in milli-seconds, to allow Serial window to be opened */
constexpr uint8_t startUpPeriod{3000};           /**< in milli-seconds, to allow LP filter to settle */
constexpr int16_t DCoffset_I{512};               /**< nominal mid-point value of ADC @ x1 scale */

// General global variables that are used in multiple blocks so cannot be static.
// For integer maths, many variables need to be 'int32_t'
//
bool beyondStartUpPhase{false}; /**< start-up delay, allows things to settle */
int32_t energyInBucket_long{0}; /**< in Integer Energy Units */

// for improved control of multiple loads
bool b_recentTransition{false};                 /**< a load state has been recently toggled */
uint8_t postTransitionCount;                    /**< counts the number of cycle since last transition */
constexpr uint8_t POST_TRANSITION_MAX_COUNT{3}; /**< allows each transition to take effect */
//constexpr uint8_t POST_TRANSITION_MAX_COUNT{50}; /**< for testing only */
uint8_t activeLoad{NO_OF_DUMPLOADS};   /**< current active load */
uint16_t countLoadON[NO_OF_DUMPLOADS]; /**< Number of cycle the load was ON (over 1 datalog period) */

int32_t sumP_forEnergyBucket;                   /**< for per-cycle summation of 'real power' */
int32_t sumP_atSupplyPoint;                     /**< for per-cycle summation of 'real power' during datalog period */
int32_t sumP_diverted;                          /**< for per-cycle summation of 'real power' */
int32_t sum_Vsquared;                           /**< for summation of V^2 values during datalog period */
int32_t cumVdeltasThisCycle_long;               /**< for the LPF which determines DC offset (voltage) */
int32_t sampleVminusDC_long;                    /**< for the phaseCal algorithm */
int32_t sampleSetsDuringThisDatalogPeriod_long; /**< number of sample sets during each datalogging period */
int32_t cycleCountForDatalogging_long{0};       /**< for counting how often datalog is updated */

int32_t DCoffset_V_long{512L * 256}; /**< <--- for LPF nominal mid-point value of ADC @ x256 scale */

constexpr int32_t DCoffset_V_min{(512L - 100L) * 256L}; /**< mid-point of ADC minus a working margin */
constexpr int32_t DCoffset_V_max{(512L + 100L) * 256L}; /**< mid-point of ADC plus a working margin */

int32_t divertedEnergyRecent_IEU{0}; /**< Hi-res accumulator of limited range */
uint16_t divertedEnergyTotal_Wh{0};  /**< WattHour register of 63K range */

constexpr uint8_t displayCyclingInSeconds{5}; /**< duration for cycling between diverted energy and temperature */
constexpr uint32_t displayShutdown_inMainsCycles{DISPLAY_SHUTDOWN_IN_HOURS * CYCLES_PER_SECOND * 3600L};
uint32_t absenceOfDivertedEnergyCount{0};         /**< count the # of cycles w/o energy diversion */
int16_t sampleSetsDuringNegativeHalfOfMainsCycle; /**< for arming the triac/trigger */
int32_t energyInBucket_prediction;                /**< predicted energy level until the end of the cycle */
bool loadHasJustChangedState;                     /**< load has just changed its state - for predictive algorithm */

// for interaction between the main processor and the ISRs
volatile bool b_datalogEventPending{false}; /**< async trigger to signal datalog is available */
volatile bool b_newMainsCycle{false};       /**< async trigger to signal start of new main cycle based on first phase */

volatile int32_t copyOf_sumP_atSupplyPoint;                /**< for per-cycle summation of 'real power' */
volatile int32_t copyOf_divertedEnergyTotal_Wh;            /**< for per-cycle summation of 'real power' */
volatile int32_t copyOf_sum_Vsquared;                      /**< for summation of V^2 values during datalog period */
volatile int16_t copyOf_sampleSetsDuringThisDatalogPeriod; /**< copy of for counting the sample sets during each datalogging period */
volatile int16_t copyOf_lowestNoOfSampleSetsPerMainsCycle; /**< copy of lowest # of sample sets during this cycle */
volatile uint16_t copyOf_countLoadON[NO_OF_DUMPLOADS];     /**< copy of number of cycle the load was ON (over 1 datalog period) */

// For an enhanced polarity detection mechanism, which includes a persistence check
constexpr uint8_t PERSISTENCE_FOR_POLARITY_CHANGE{1}; /**< allows polarity changes to be confirmed */
Polarities polarityOfMostRecentVsample;               /**< for zero-crossing detection */
Polarities polarityConfirmed;                         /**< for zero-crossing detection */
Polarities polarityConfirmedOfLastSampleV;            /**< for zero-crossing detection */

// For a mechanism to check the continuity of the sampling sequence
constexpr int32_t CONTINUITY_CHECK_MAXCOUNT{250}; /**< mains cycles */
int16_t sampleCount_forContinuityChecker;         /**< for checking the stability of acquisition */
int16_t sampleSetsDuringThisMainsCycle;           /**< # of sample sets during this cycle */
int16_t lowestNoOfSampleSetsPerMainsCycle;        /**< lowest # of sample sets during this cycle */

// Calibration values
//-------------------
// Two calibration values are used: powerCal and phaseCal.
// A full explanation of each of these values now follows:
//
// powerCal is a floating point variable which is used for converting the
// product of voltage and current samples into Watts.
//
// The correct value of powerCal is dependent on the hardware that is
// in use. For best resolution, the hardware should be configured so that the
// voltage and current waveforms each span most of the ADC's usable range. For
// many systems, the maximum power that will need to be measured is around 3kW.
//
// My sketch "RawSamplesTool_2chan.ino" provides a one-shot visual display of the
// voltage and current waveforms. This provides an easy way for the user to be
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
// output range. Stated more formally, the conversion rate of the overall system
// for measuring VOLTAGE is likely to be around 1 ADC-step per Volt (RMS).
//
// In the case of AC current, however, the situation is very different. At
// mains voltage, a power of 3kW corresponds to an RMS current of 12.5A which
// has a peak-to-peak range of 35A. This is smaller than the output signal by
// around a factor of twenty. The conversion rate of the overall system for
// measuring CURRENT is therefore likely to be around 20 ADC-steps per Amp.
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
constexpr float powerCal_grid{0.0435};     /**< for CT1 */
constexpr float powerCal_diverted{0.0435}; /**< for CT2 */

// For datalogging purposes, f_voltageCal has been added too. Because the range of ADC values is
// similar to the actual range of volts, the optimal value for this cal factor is likely to be
// close to unity.
constexpr float f_voltageCal{1.03f}; /**< compared with Fluke 77 meter */

constexpr int32_t capacityOfEnergyBucket_long{(int32_t)WORKING_RANGE_IN_JOULES * CYCLES_PER_SECOND * (1 / powerCal_grid)}; /**< depends on powerCal, frequency & the 'sweetzone' size. */
constexpr int32_t nominalEnergyThreshold{capacityOfEnergyBucket_long * 0.5};

int32_t workingEnergyThreshold_upper{nominalEnergyThreshold}; /**< initial value */
int32_t workingEnergyThreshold_lower{nominalEnergyThreshold}; /**< initial value */

constexpr int32_t IEU_per_Wh{(int32_t)JOULES_PER_WATT_HOUR * CYCLES_PER_SECOND * (1 / powerCal_diverted)}; /**< depends on powerCal, frequency & the 'sweetzone' size. */
constexpr int32_t antiCreepLimit_inIEUperMainsCycle{(float)ANTI_CREEP_LIMIT * (1 / powerCal_grid)};        /**< to avoid the diverted energy accumulator 'creeping' when the load is not active */

// for this go-faster sketch, the phaseCal logic has been removed. If required, it can be
// found in most of the standard Mk2_bothDisplay_n versions

// Various settings for the 4-digit display, which needs to be refreshed every few mS
constexpr uint8_t noOfDigitLocations{4};

//  The two versions of the hardware require different logic.
#ifdef PIN_SAVING_HARDWARE

constexpr uint8_t noOfPossibleCharacters{22};

#define DRIVER_CHIP_DISABLED HIGH
#define DRIVER_CHIP_ENABLED LOW

constexpr uint8_t tempSensorPin{xx};         /**< <- a free pin must be choosen for temperatur sensor

// the primary segments are controlled by a pair of logic chips
constexpr uint8_t noOfDigitSelectionLines{4}; /**< <- for the 74HC4543 7-segment display driver */
constexpr uint8_t noOfDigitLocationLines{2}; /**< <- for the 74HC138 2->4 line demultiplexer */

constexpr uint8_t enableDisableLine{5}; /**< <- affects the primary 7 segments only (not the DP) */
constexpr uint8_t decimalPointLine{14}; /**< <- this line has to be individually controlled. */

constexpr uint8_t digitLocationLine[noOfDigitLocationLines]{16, 15};
constexpr uint8_t digitSelectionLine[noOfDigitSelectionLines]{7, 9, 8, 6};

// The final column of digitValueMap[] is for the decimal point status. In this version,
// the decimal point has to be treated differently than the other seven segments, so
// a convenient means of accessing this column is provided.
//
constexpr uint8_t digitValueMap[noOfPossibleCharacters][noOfDigitSelectionLines + 1]{
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
constexpr uint8_t DPstatus_columnID{noOfDigitSelectionLines};

constexpr uint8_t digitLocationMap[noOfDigitLocations][noOfDigitLocationLines]{
    LOW, LOW,   // Digit 1
    LOW, HIGH,  // Digit 2
    HIGH, LOW,  // Digit 3
    HIGH, HIGH, // Digit 4
};

#else // PIN_SAVING_HARDWARE

#define ON HIGH
#define OFF LOW

constexpr uint8_t tempSensorPin{15}; /**< the only free pin left in thins case (pin1 of IC4) */

constexpr uint8_t noOfSegmentsPerDigit{8}; /**< includes one for the decimal point */

constexpr uint8_t noOfPossibleCharacters{23};

enum class DigitEnableStates : uint8_t
{
  DIGIT_ENABLED,
  DIGIT_DISABLED
};

constexpr uint8_t digitSelectorPin[noOfDigitLocations]{16, 10, 13, 11};
constexpr uint8_t segmentDrivePin[noOfSegmentsPerDigit]{2, 5, 12, 6, 7, 9, 8, 14};

// The final column of segMap[] is for the decimal point status. In this version,
// the decimal point is treated just like all the other segments, so there is
// no need to access this column specifically.
//
constexpr uint8_t segMap[noOfPossibleCharacters][noOfSegmentsPerDigit]{
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
    OFF, OFF, OFF, OFF, OFF, OFF, OFF, ON,  // '.' <- element 21
    ON, ON, OFF, OFF, OFF, ON, ON, OFF      // '°' <- element 22
};
#endif // PIN_SAVING_HARDWARE

uint8_t charsForDisplay[noOfDigitLocations]{20, 20, 20, 20}; /**< all blank */

bool EDD_isActive{false}; /**< energy divertion detection */

constexpr int32_t requiredExportPerMainsCycle_inIEU{(int32_t)REQUIRED_EXPORT_IN_WATTS * (1 / powerCal_grid)};

#ifdef TEMP_SENSOR
// For temperature sensing
OneWire oneWire(tempSensorPin);
#endif

/**
 * @brief Called once during startup.
 * @details This function initializes a couple of variables we cannot init at compile time and
 *          sets a couple of parameters for runtime.
 * 
 */
void setup()
{
  //  pinMode(outOfRangeIndication, OUTPUT);
  //  digitalWriteFast (outOfRangeIndication, LED_OFF);

  pinMode(physicalLoad_0_pin, OUTPUT); // driver pin for the primary load
  pinMode(physicalLoad_1_pin, OUTPUT); // driver pin for an additional load
  //
  for (int16_t i = 0; i < NO_OF_DUMPLOADS; ++i) // re-using the logic from my multiLoad code
  {
    logicalLoadState[i] = LoadStates::LOAD_OFF;
    physicalLoadState[i] = LoadStates::LOAD_OFF;
  }
  //
  digitalWrite(physicalLoad_0_pin, (uint8_t)physicalLoadState[0]);  // the primary load is active low.
  digitalWrite(physicalLoad_1_pin, !(uint8_t)physicalLoadState[1]); // the additional load is active high.

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
  for (int16_t i = 0; i < noOfDigitSelectionLines; ++i)
    pinMode(digitSelectionLine[i], OUTPUT);

  // an enable line is required for the 74HC4543 7-seg display driver
  pinMode(enableDisableLine, OUTPUT); // for the 74HC4543 7-seg display driver
  digitalWrite(enableDisableLine, DRIVER_CHIP_DISABLED);

  // set up the control lines for the 74HC138 2->4 demux
  for (int16_t i = 0; i < noOfDigitLocationLines; ++i)
    pinMode(digitLocationLine[i], OUTPUT);
#else
  for (int16_t i = 0; i < noOfSegmentsPerDigit; ++i)
    pinMode(segmentDrivePin[i], OUTPUT);

  for (int16_t i = 0; i < noOfDigitLocations; ++i)
    pinMode(digitSelectorPin[i], OUTPUT);

  for (int16_t i = 0; i < noOfDigitLocations; ++i)
    digitalWrite(digitSelectorPin[i], (uint8_t)DigitEnableStates::DIGIT_DISABLED);

  for (int16_t i = 0; i < noOfSegmentsPerDigit; ++i)
    digitalWrite(segmentDrivePin[i], OFF);
#endif

  // When using integer maths, calibration values that have supplied in floating point
  // form need to be rescaled.

  // When using integer maths, the SIZE of the ENERGY BUCKET is altered to match the
  // scaling of the energy detection mechanism that is in use. This avoids the need
  // to re-scale every energy contribution, thus saving processing time. This process
  // is described in more detail in the function, allGeneralProcessing(), just before
  // the energy bucket is updated at the start of each new cycle of the mains.
  //
  // An electricity meter has a small range over which energy can ebb and flow without
  // penalty. This has been termed its "sweet-zone". For optimal performance, the energy
  // bucket of a PV Router should match this value. The sweet-zone value is therefore
  // included in the calculation below.
  //
  // For the flow of energy at the 'grid' connection point (CT1)

  // For recording the accumulated amount of diverted energy data (using CT2), a similar
  // calibration mechanism is required. Rather than a bucket with a fixed capacity, the
  // accumulator for diverted energy just needs to be scaled correctly. As soon as its
  // value exceeds 1 Wh, an associated WattHour register is incremented, and the
  // accumulator's value is decremented accordingly. The calculation below is to determine
  // the scaling for this accumulator.

  // Define operating limits for the LP filter which identifies DC offset in the voltage
  // sample stream. By limiting the output range, the filter always should start up
  // correctly.

  Serial.println("ADC mode:       free-running");

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

#ifdef TEMP_SENSOR
  convertTemperature(); // start initial temperature conversion
#endif
}

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
  static unsigned char sample_index{0};
  int16_t rawSample;
  int32_t sampleIminusDC;
  int32_t filtV_div4;
  int32_t filtI_div4;
  int32_t instP;
  int32_t inst_Vsquared;

  switch (sample_index)
  {
  case 0:
    rawSample = ADC;                       // store the ADC value (this one is for Voltage)
    ADMUX = 0x40 + currentSensor_diverted; // set up the next conversion, which is for Diverted Current
    ++sample_index;                        // increment the control flag

    sampleVminusDC_long = ((int32_t)rawSample << 8) - DCoffset_V_long;
    polarityOfMostRecentVsample = (sampleVminusDC_long > 0) ? Polarities::POSITIVE : Polarities::NEGATIVE;
    confirmPolarity();
    //
    processRawSamples(); // deals with aspects that only occur at particular stages of each mains cycle
                         //
    // for the Vrms calculation (for datalogging only)
    filtV_div4 = sampleVminusDC_long >> 2;   // reduce to 16-bits (now x64, or 2^6)
    inst_Vsquared = filtV_div4 * filtV_div4; // 32-bits (now x4096, or 2^12)
    inst_Vsquared >>= 12;                    // scaling is now x1 (V_ADC x I_ADC)
    sum_Vsquared += inst_Vsquared;           // cumulative V^2 (V_ADC x I_ADC)
    ++sampleSetsDuringThisDatalogPeriod_long;
    //
    // store items for use during next loop
    cumVdeltasThisCycle_long += sampleVminusDC_long;    // for use with LP filter
    polarityConfirmedOfLastSampleV = polarityConfirmed; // for identification of half cycle boundaries
    ++sampleSetsDuringThisMainsCycle;                   // for real power calculations

    refreshDisplay();
    break;
  case 1:
    rawSample = ADC;              // store the ADC value (this one is for Grid Current)
    ADMUX = 0x40 + voltageSensor; // set up the next conversion, which is for Voltage
    ++sample_index;               // increment the control flag
    //
    // remove most of the DC offset from the current sample (the precise value does not matter)
    sampleIminusDC = ((int32_t)(rawSample - DCoffset_I)) << 8;
    //
    // calculate the "real power" in this sample pair and add to the accumulated sum
    filtV_div4 = sampleVminusDC_long >> 2; // reduce to 16-bits (now x64, or 2^6)
    filtI_div4 = sampleIminusDC >> 2;      // reduce to 16-bits (now x64, or 2^6)
    instP = filtV_div4 * filtI_div4;       // 32-bits (now x4096, or 2^12)
    instP >>= 12;                          // scaling is now x1, as for Mk2 (V_ADC x I_ADC)
    sumP_forEnergyBucket += instP;         // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
    sumP_atSupplyPoint += instP;
    break;
  case 2:
    rawSample = ADC;                   // store the ADC value (this one is for Diverted Current)
    ADMUX = 0x40 + currentSensor_grid; // set up the next conversion, which is for Grid Current
    sample_index = 0;                  // reset the control flag
    //
    // remove most of the DC offset from the current sample (the precise value does not matter)
    sampleIminusDC = ((int32_t)(rawSample - DCoffset_I)) << 8;
    //
    // calculate the "real power" in this sample pair and add to the accumulated sum
    filtV_div4 = sampleVminusDC_long >> 2; // reduce to 16-bits (now x64, or 2^6)
    filtI_div4 = sampleIminusDC >> 2;      // reduce to 16-bits (now x64, or 2^6)
    instP = filtV_div4 * filtI_div4;       // 32-bits (now x4096, or 2^12)
    instP >>= 12;                          // scaling is now x1, as for Mk2 (V_ADC x I_ADC)
    sumP_diverted += instP;                // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
    break;
  default:
    sample_index = 0; // to prevent lockup (should never get here)
  }
} // end of ISR

/* -----------------------------------------------------------
   Start of various helper functions which are used by the ISR
*/

/*!
*  @defgroup TimeCritical Time critical functions Group
*  Functions used by the ISR
*/

/**
 * @brief This routine is called by the ISR when a pair of V & I sample becomes available.
 * 
 * @ingroup TimeCritical
 */
void processRawSamples()
{
  if (polarityConfirmed == Polarities::POSITIVE)
  {
    if (polarityConfirmedOfLastSampleV != Polarities::POSITIVE)
    {
      // This is the start of a new +ve half cycle (just after the zero-crossing point)
      if (beyondStartUpPhase)
        processLatestContribution();
      else
        processStartUp();
    } // end of processing that is specific to the first Vsample in each +ve half cycle

    // still processing samples where the voltage is Polarities::POSITIVE ...
    if (beyondStartUpPhase && (sampleSetsDuringThisMainsCycle == 5)) // to distribute the workload within each mains cycle
      postProcessPlusHalfCycle();
  }    // end of processing that is specific to samples where the voltage is Polarities::POSITIVE
  else // the polatity of this sample is Polarities::NEGATIVE
  {
    if (polarityConfirmedOfLastSampleV != Polarities::NEGATIVE)
      processMinusHalfCycle();
    // end of processing that is specific to the first Vsample in each -ve half cycle

    if (sampleSetsDuringNegativeHalfOfMainsCycle == 5)
      postProcessMinusHalfCycle();

    ++sampleSetsDuringNegativeHalfOfMainsCycle;
  } // end of processing that is specific to samples where the voltage is Polarities::NEGATIVE
}

/**
 * @brief Process the startup period for the router.
 * 
 * @ingroup TimeCritical
 */
void processStartUp()
{
  // wait until the DC-blocking filters have had time to settle
  if (millis() <= (delayBeforeSerialStarts + startUpPeriod))
    return; // still settling, do nothing

  // the DC-blocking filters have had time to settle
  beyondStartUpPhase = true;
  sumP_forEnergyBucket = 0;
  sumP_atSupplyPoint = 0;
  sumP_diverted = 0;
  sampleSetsDuringThisMainsCycle = 0;   // not yet dealt with for this cycle
  sampleCount_forContinuityChecker = 1; // opportunity has been missed for this cycle
  lowestNoOfSampleSetsPerMainsCycle = 999;
  sampleSetsDuringThisDatalogPeriod_long = 0;
  // can't say "Go!" here 'cos we're in an ISR!
}

/**
 * @brief Process the start of a new +ve half cycle, just after the zero-crossing point.
 * 
 * @ingroup TimeCritical
 */
void processLatestContribution()
{
  b_newMainsCycle = true; // <--  a 50 Hz 'tick' for use by the main code

  // a simple routine for checking the performance of this new ISR structure
  if (sampleSetsDuringThisMainsCycle < lowestNoOfSampleSetsPerMainsCycle)
    lowestNoOfSampleSetsPerMainsCycle = sampleSetsDuringThisMainsCycle;

  // Calculate the real power and energy during the last whole mains cycle.
  //
  // sumP contains the sum of many individual calculations of instantaneous power. In
  // order to obtain the average power during the relevant period, sumP must first be
  // divided by the number of samples that have contributed to its value.
  //
  // The next stage would normally be to apply a calibration factor so that real power
  // can be expressed in Watts. That's fine for floating point maths, but it's not such
  // a good idea when integer maths is being used. To keep the numbers large, and also
  // to save time, calibration of power is omitted at this stage. Real Power (stored as
  // a 'int32_t') is therefore (1/powerCal) times larger than the actual power in Watts.
  //
  int32_t realPower_grid{sumP_forEnergyBucket / sampleSetsDuringThisMainsCycle}; // proportional to Watts
  int32_t realPower_diverted{sumP_diverted / sampleSetsDuringThisMainsCycle};    // proportional to Watts

  realPower_grid -= requiredExportPerMainsCycle_inIEU; // <- useful for PV simulation

  // Next, the energy content of this power rating needs to be determined. Energy is
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
  int32_t realEnergy_grid{realPower_grid};
  int32_t realEnergy_diverted{realPower_diverted};

  // Energy contributions from the grid connection point (CT1) are summed in an
  // accumulator which is known as the energy bucket. The purpose of the energy bucket
  // is to mimic the operation of the supply meter. The range over which energy can
  // pass to and fro without loss or charge to the user is known as its 'sweet-zone'.
  // The capacity of the energy bucket is set to this same value within setup().
  //
  // The latest contribution can now be added to this energy bucket
  energyInBucket_long += realEnergy_grid;

  // Apply max and min limits to bucket's level. This is to ensure correct operation
  // when conditions change, i.e. when import changes to export, and vici versa.
  //
  //bool endOfRangeEncountered{false};
  if (energyInBucket_long > capacityOfEnergyBucket_long)
  {
    energyInBucket_long = capacityOfEnergyBucket_long;
    //endOfRangeEncountered = true;
  }
  else if (energyInBucket_long < 0)
  {
    energyInBucket_long = 0;
    //endOfRangeEncountered = true;
  }

  /*
        if (endOfRangeEncountered) {
          digitalWriteFast (outOfRangeIndication , LED_ON); }
        else {
          digitalWriteFast (outOfRangeIndication , LED_OFF); }*/

  if (EDD_isActive) // Energy Diversion Display
  {
    // For diverted energy, the latest contribution needs to be added to an
    // accumulator which operates with maximum precision.

    if (realEnergy_diverted < antiCreepLimit_inIEUperMainsCycle)
      realEnergy_diverted = 0;

    divertedEnergyRecent_IEU += realEnergy_diverted;

    // Whole kWhours are then recorded separately
    if (divertedEnergyRecent_IEU > IEU_per_Wh)
    {
      divertedEnergyRecent_IEU -= IEU_per_Wh;
      ++divertedEnergyTotal_Wh;
    }
  }

  // continuity checker
  ++sampleCount_forContinuityChecker;
  if (sampleCount_forContinuityChecker >= CONTINUITY_CHECK_MAXCOUNT)
  {
    sampleCount_forContinuityChecker = 0;
    //Serial.println(lowestNoOfSampleSetsPerMainsCycle);
    lowestNoOfSampleSetsPerMainsCycle = 999;
  }

  /* At the end of each datalogging period, copies are made of the relevant variables
   * for use by the main code.  These variable are then reset for use during the next 
   * datalogging period.
   */
  if (++cycleCountForDatalogging_long >= DATALOG_PERIOD_IN_MAINS_CYCLES)
  {
    cycleCountForDatalogging_long = 0;

    copyOf_sumP_atSupplyPoint = sumP_atSupplyPoint;
    copyOf_divertedEnergyTotal_Wh = divertedEnergyTotal_Wh;
    copyOf_sum_Vsquared = sum_Vsquared;
    copyOf_sampleSetsDuringThisDatalogPeriod = sampleSetsDuringThisDatalogPeriod_long; // (for diags only)
    copyOf_lowestNoOfSampleSetsPerMainsCycle = lowestNoOfSampleSetsPerMainsCycle;      // (for diags only)

    for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
    {
      copyOf_countLoadON[i] = countLoadON[i];
      countLoadON[i] = 0;
    }

    sumP_atSupplyPoint = 0;
    sum_Vsquared = 0;
    lowestNoOfSampleSetsPerMainsCycle = 999;
    sampleSetsDuringThisDatalogPeriod_long = 0;
    b_datalogEventPending = true;
  }

  // clear the per-cycle accumulators for use in this new mains cycle.
  sampleSetsDuringThisMainsCycle = 0;
  sumP_forEnergyBucket = 0;
  sumP_diverted = 0;
  sampleSetsDuringNegativeHalfOfMainsCycle = 0;
}

/**
 * @brief Post processing after the start of a new +ve half cycle, just after the zero-crossing point.
 * 
 * @ingroup TimeCritical
 */
void postProcessPlusHalfCycle()
{
  // Restrictions apply for the period immediately after a load has been switched.
  // Here the b_recentTransition flag is checked and updated as necessary.
  b_recentTransition &= (++postTransitionCount < POST_TRANSITION_MAX_COUNT);

  if (!b_recentTransition)
    return;

  // During the post-transition period, any change in the energy level is noted.
  if (energyInBucket_long > workingEnergyThreshold_upper)
  {
    workingEnergyThreshold_lower = nominalEnergyThreshold; // reset the opposite threshold
    workingEnergyThreshold_upper = energyInBucket_long;

    // the energy thresholds must remain within range
    if (workingEnergyThreshold_upper > capacityOfEnergyBucket_long)
      workingEnergyThreshold_upper = capacityOfEnergyBucket_long;
  }
  else if (energyInBucket_long < workingEnergyThreshold_lower)
  {
    workingEnergyThreshold_upper = nominalEnergyThreshold; // reset the opposite threshold
    workingEnergyThreshold_lower = energyInBucket_long;

    // the energy thresholds must remain within range
    if (workingEnergyThreshold_lower < 0)
      workingEnergyThreshold_lower = 0;
  }
}

/**
 * @brief Process the start of a new -ve half cycle, just after the zero-crossing point.
 * 
 * @ingroup TimeCritical
 */
void processMinusHalfCycle()
{
  // This is the start of a new -ve half cycle (just after the zero-crossing point)
  // which is a convenient point to update the Low Pass Filter for DC-offset removal
  //  The portion which is fed back into the integrator is approximately one percent
  // of the average offset of all the Vsamples in the previous mains cycle.
  //
  int32_t previousOffset{DCoffset_V_long};
  DCoffset_V_long = previousOffset + (cumVdeltasThisCycle_long >> 12);
  cumVdeltasThisCycle_long = 0;

  // To ensure that the LPF will always start up correctly when 240V AC is available, its
  // output value needs to be prevented from drifting beyond the likely range of the
  // voltage signal. This avoids the need to use a HPF as was done for initial Mk2 builds.
  //
  if (DCoffset_V_long < DCoffset_V_min)
    DCoffset_V_long = DCoffset_V_min;
  else if (DCoffset_V_long > DCoffset_V_max)
    DCoffset_V_long = DCoffset_V_max;

  // The average power that has been measured during the first half of this mains cycle can now be used
  // to predict the energy state at the end of this mains cycle. That prediction will be used to alter
  // the state of the load as necessary. The arming signal for the triac can't be set yet - that must
  // wait until the voltage has advanced further beyond the -ve going zero-crossing point.
  //
  int32_t averagePower{sumP_forEnergyBucket / sampleSetsDuringThisMainsCycle}; // for 1st half of this mains cycle only
  //
  // To avoid repetitive and unnecessary calculations, the increase in energy during each mains cycle is
  // deemed to be numerically equal to the average power. The predicted value for the energy state at the
  // end of this mains cycle will therefore be the known energy state at its start plus the average power
  // as measured. Although the average power has been determined over only half a mains cycle, the correct
  // number of contributing sample sets has been used so the result can be expected to be a true measurement
  // of average power, not half of it.
  // However, it can be shown that the average power during the first half of any mains cycle after the
  // load has changed state will alway be under-recorded so its value should now be increased by 30%. This
  // arbitrary looking adjustment gives good test results with differening amounts of surplus power and it
  // only affects the predicted value of the energy state at the end of the current mains cycle; it does
  // not affect the value in the main energy bucket. This complication is a fundamental consequence
  // of the floating CTs that we use.
  //
  if (loadHasJustChangedState)
    averagePower *= 1.3;

  energyInBucket_prediction = energyInBucket_long + averagePower; // at end of this mains cycle
}

/**
 * @brief Post processing after the start of a new -ve half cycle, just after the zero-crossing point.
 * 
 * @ingroup TimeCritical
 */
void postProcessMinusHalfCycle()
{
  // the zero-crossing trigger device(s) can now be reliably armed
  if (!beyondStartUpPhase)
    return;

  /* Determining whether any of the loads need to be changed is is a 3-stage process:
  * - change the LOGICAL load states as necessary to maintain the energy level
  * - update the PHYSICAL load states according to the logical -> physical mapping 
  * - update the driver lines for each of the loads.
  */

  loadHasJustChangedState = false; // clear the predictive algorithm's flag

  if (energyInBucket_prediction > workingEnergyThreshold_upper)
    proceedHighEnergyLevel();
  else if (energyInBucket_prediction < workingEnergyThreshold_lower)
    proceedLowEnergyLevel();

  updatePhysicalLoadStates(); // allows the logical-to-physical mapping to be changed

  // update each of the physical loads
  digitalWrite(physicalLoad_0_pin, (uint8_t)physicalLoadState[0]);  // active low for trigger
  digitalWrite(physicalLoad_1_pin, !(uint8_t)physicalLoadState[1]); // active high for additional load

  // update the Energy Diversion Detector
  if (physicalLoadState[0] == LoadStates::LOAD_ON)
  {
    absenceOfDivertedEnergyCount = 0;
    EDD_isActive = true;
  }
  else
    ++absenceOfDivertedEnergyCount;
}

/**
 * @brief Process the case of high energy level, some action may be required.
 * 
 * @ingroup TimeCritical
 */
void proceedHighEnergyLevel()
{
  // the predicted energy state is high so an extra load may need to be added
  bool OK_toAddLoad{true}; // default state of flag

  auto tempLoad{nextLogicalLoadToBeAdded()};
  if (tempLoad >= NO_OF_DUMPLOADS)
    return;

  // a load which is now OFF has been identified for potentially being switched ON
  if (b_recentTransition)
    OK_toAddLoad = (tempLoad == activeLoad);

  if (OK_toAddLoad)
  {
    // tempLoad will be either the active load, or the next load in the priority sequence
    logicalLoadState[tempLoad] = LoadStates::LOAD_ON;
    activeLoad = tempLoad;
    postTransitionCount = 0;
    b_recentTransition = true;
    loadHasJustChangedState = true;
  }
}

/**
 * @brief Process the case of low energy level, some action may be required.
 * 
 * @ingroup TimeCritical
 */
void proceedLowEnergyLevel()
{
  // the predicted energy state is low so some load may need to be removed
  bool OK_toRemoveLoad{true}; // default state of flag

  auto tempLoad{nextLogicalLoadToBeRemoved()};
  if (tempLoad >= NO_OF_DUMPLOADS)
    return;

  // a load which is now ON has been identified for potentially being switched OFF
  if (b_recentTransition)
    OK_toRemoveLoad = (tempLoad == activeLoad);

  if (OK_toRemoveLoad)
  {
    // tempLoad will be either the active load, or the next load in the priority sequence
    logicalLoadState[tempLoad] = LoadStates::LOAD_OFF;
    activeLoad = tempLoad;
    postTransitionCount = 0;
    b_recentTransition = true;
    loadHasJustChangedState = true;
  }
}

/**
 * @brief This routine prevents a zero-crossing point from being declared until a certain number
 *        of consecutive samples in the 'other' half of the waveform have been encountered.
 * 
 * @ingroup TimeCritical
 */
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
    if (logicalLoadState[index] == LoadStates::LOAD_OFF)
      return (index);

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
    if (logicalLoadState[index] == LoadStates::LOAD_ON)
      return (index);
  } while (0 != index);

  return (NO_OF_DUMPLOADS);
}

/**
 * @brief This function provides the link between the logical and physical loads.
 * @details The array, logicalLoadState[], contains the on/off state of all logical loads, with
 *          element 0 being for the one with the highest priority. The array,
 *          physicalLoadState[], contains the on/off state of all physical loads.
 * 
 *          The association between the physical and logical loads is 1:1. By default, numerical
 *          equivalence is maintained, so logical(N) maps to physical(N). If physical load 1 is set 
 *          to have priority, rather than physical load 0, the logical-to-physical association for 
 *          loads 0 and 1 are swapped.
 * 
 *          Any other mapping relationships could be configured here.
 * 
 * @ingroup TimeCritical
 */
void updatePhysicalLoadStates()
{
  for (int16_t i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    if (logicalLoadState[i] == LoadStates::LOAD_ON)
      ++countLoadON[i];
    physicalLoadState[i] = logicalLoadState[i];
  }
}

/**
 * @brief This routine keeps track of which digit is being displayed and checks when its display 
 *        time has expired. It then makes the necessary adjustments for displaying the next digit.
 *        The two versions of the hardware require different logic.
 * 
 * @ingroup TimeCritical
 */
void refreshDisplay()
{
  static uint8_t displayTime_count{0};
  static uint8_t digitLocationThatIsActive{0};

  if (++displayTime_count <= MAX_DISPLAY_TIME_COUNT)
    return;

  displayTime_count = 0;

#ifdef PIN_SAVING_HARDWARE
  // With this version of the hardware, care must be taken that all transitory states
  // are masked out. Note that the enableDisableLine only masks the seven primary
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

  uint8_t lineState;

  // 1. disable the Decimal Point driver line;
  digitalWrite(decimalPointLine, LOW);

  // 2. disable the driver chip while changes are taking place
  digitalWrite(enableDisableLine, DRIVER_CHIP_DISABLED);

  // 3. determine the next digit location to be active
  ++digitLocationThatIsActive;
  if (digitLocationThatIsActive >= noOfDigitLocations)
    digitLocationThatIsActive = 0;

  // 4. set up the digit location drivers for the new active location
  for (uint8_t line = 0; line < noOfDigitLocationLines; ++line)
  {
    lineState = digitLocationMap[digitLocationThatIsActive][line];
    digitalWrite(digitLocationLine[line], lineState);
  }

  // 5. determine the character to be displayed at this new location
  // (which includes the decimal point information)
  uint8_t digitVal{charsForDisplay[digitLocationThatIsActive]};

  // 6. configure the 7-segment driver for the character to be displayed
  for (uint8_t line = 0; line < noOfDigitSelectionLines; ++line)
  {
    lineState = digitValueMap[digitVal][line];
    digitalWrite(digitSelectionLine[line], lineState);
  }

  // 7. set up the Decimal Point driver line;
  digitalWrite(decimalPointLine, digitValueMap[digitVal][DPstatus_columnID]);

  // 8. enable the 7-segment driver chip
  digitalWrite(enableDisableLine, DRIVER_CHIP_ENABLED);

#else // PIN_SAVING_HARDWARE

  // This version is more straightforward because the digit-enable lines can be
  // used to mask out all of the transitory states, including the Decimal Point.
  // The sequence is:
  //
  // 1. de-activate the digit-enable line that was previously active
  // 2. determine the next location which is to be active
  // 3. determine the relevant character for the new active location
  // 4. set up the segment drivers for the character to be displayed (includes the DP)
  // 5. activate the digit-enable line for the new active location

  // 1. de-activate the location which is currently being displayed
  digitalWrite(digitSelectorPin[digitLocationThatIsActive], (uint8_t)DigitEnableStates::DIGIT_DISABLED);

  // 2. determine the next digit location which is to be displayed
  ++digitLocationThatIsActive;
  if (digitLocationThatIsActive >= noOfDigitLocations)
    digitLocationThatIsActive = 0;

  // 3. determine the relevant character for the new active location
  uint8_t digitVal{charsForDisplay[digitLocationThatIsActive]};

  // 4. set up the segment drivers for the character to be displayed (includes the DP)
  for (uint8_t segment = 0; segment < noOfSegmentsPerDigit; ++segment)
  {
    uint8_t segmentState{segMap[digitVal][segment]};
    digitalWrite(segmentDrivePin[segment], segmentState);
  }

  // 5. activate the digit-enable line for the new active location
  digitalWrite(digitSelectorPin[digitLocationThatIsActive], (uint8_t)DigitEnableStates::DIGIT_ENABLED);

#endif // PIN_SAVING_HARDWARE
} // end of refreshDisplay()

/* End of helper functions which are used by the ISR
   -------------------------------------------------
*/

/**
 * @brief Called infrequently, to update the characters to be displayed
 * 
 * @param bToggleDisplayTemp true for temperature, false for diverted energy 
 */
void configureValueForDisplay(const bool bToggleDisplayTemp)
{
  static uint8_t locationOfDot{0};

#ifdef TEMP_SENSOR
  // leave the temperature in case of zero diversion instead of "walking dots"
  if (bToggleDisplayTemp || !EDD_isActive)
  {
    // we want to display the temperature
    uint16_t val{tx_data.temperature_times100};

    uint8_t thisDigit{val / 1000};
    charsForDisplay[0] = thisDigit;
    val -= 1000 * thisDigit;

    thisDigit = val / 100;
    charsForDisplay[1] = thisDigit + 10; // dec point after 2nd digit
    val -= 100 * thisDigit;

    thisDigit = val / 10;
    charsForDisplay[2] = thisDigit;
    val -= 10 * thisDigit;

#ifndef PIN_SAVING_HARDWARE
    // In case we use the BCD converter, we cannot display this character :-(
    charsForDisplay[3] = 22; // we skip the last character, display '°' instead
#endif

    return;
  }
#endif

  if (!EDD_isActive)
  {
    // "walking dots" display
    charsForDisplay[locationOfDot] = 20; // blank

    ++locationOfDot;
    if (locationOfDot >= noOfDigitLocations)
      locationOfDot = 0;

    charsForDisplay[locationOfDot] = 21; // dot

    return;
  }

  uint16_t val{divertedEnergyTotal_Wh};
  bool energyValueExceeds10kWh;

  if (val < 10000)
    energyValueExceeds10kWh = false; // no need to re-scale (display to 3 DPs)
  else
  {
    // re-scale is needed (display to 2 DPs)
    energyValueExceeds10kWh = true;
    val = val / 10;
  }

  uint8_t thisDigit{val / 1000};
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
    charsForDisplay[1] += 10; // dec point after 2nd digit
  else
    charsForDisplay[0] += 10; // dec point after 1st digit
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
  static bool bToggleDisplayTemp{false};
  static uint16_t displayCyclingTimer{0};

  if (b_newMainsCycle)
  {
    b_newMainsCycle = false;
    ++perSecondTimer;

    if (perSecondTimer >= CYCLES_PER_SECOND)
    {
      perSecondTimer = 0;
      ++displayCyclingTimer;

      // After a pre-defined period of inactivity, the 4-digit display needs to
      // close down in readiness for the next's day's data.
      //
      if (absenceOfDivertedEnergyCount > displayShutdown_inMainsCycles)
      {
        // Clear the accumulators for diverted energy.  These are the "genuine"
        // accumulators that are used by ISR rather than the copies that are
        // regularly made available for use by the main code.
        //
        divertedEnergyTotal_Wh = 0;
        divertedEnergyRecent_IEU = 0;
        EDD_isActive = false; // energy diversion detector is now inactive
      }

      configureValueForDisplay(bToggleDisplayTemp); // this timing is not critical so does not need to be in the ISR
    }
  }

  if (displayCyclingTimer >= displayCyclingInSeconds)
  {
    displayCyclingTimer = 0;
    bToggleDisplayTemp = !bToggleDisplayTemp;
  }

  if (b_datalogEventPending)
  {
    b_datalogEventPending = false;

    tx_data.powerAtSupplyPoint_Watts = copyOf_sumP_atSupplyPoint * powerCal_grid / copyOf_sampleSetsDuringThisDatalogPeriod;
    tx_data.powerAtSupplyPoint_Watts *= -1; // to match the OEM convention (import is =ve; export is -ve)
    tx_data.divertedEnergyTotal_Wh = copyOf_divertedEnergyTotal_Wh;
    tx_data.Vrms_times100 = (int16_t)(100 * f_voltageCal * sqrt(copyOf_sum_Vsquared / copyOf_sampleSetsDuringThisDatalogPeriod));

#ifdef TEMP_SENSOR
    tx_data.temperature_times100 = readTemperature();
#endif

#ifdef DATALOG_OUTPUT
    Serial.print("datalog event: grid power ");
    Serial.print(tx_data.powerAtSupplyPoint_Watts);
    Serial.print(", diverted energy (Wh) ");
    Serial.print(tx_data.divertedEnergyTotal_Wh);
    Serial.print(", Vrms ");
    Serial.print((float)tx_data.Vrms_times100 / 100);
    for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
    {
      Serial.print(", #");
      Serial.print(i);
      Serial.print(" ");
      Serial.print((float)(100 * copyOf_countLoadON[i]) / DATALOG_PERIOD_IN_MAINS_CYCLES);
      Serial.print("%");
    }
#ifdef TEMP_SENSOR
    Serial.print(", temperature ");
    Serial.print((float)tx_data.temperature_times100 / 100);
    Serial.print("°C ");
#endif
    Serial.print(",  (minSampleSets/MC ");
    Serial.print(copyOf_lowestNoOfSampleSetsPerMainsCycle);
    Serial.print(",  #ofSampleSets ");
    Serial.print(copyOf_sampleSetsDuringThisDatalogPeriod);
    Serial.println(')');
#endif

#ifdef TEMP_SENSOR
    convertTemperature(); // for use next time around
#endif
  }
} // end of loop()

#ifdef TEMP_SENSOR
/**
 * @brief Convert the internal value read from the sensor to a value in °C.
 * 
 */
void convertTemperature()
{
  oneWire.reset();
  oneWire.write(SKIP_ROM);
  oneWire.write(CONVERT_TEMPERATURE);
}

/**
 * @brief Read the temperature.
 * 
 * @return The temperature in °C. 
 */
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

int16_t freeRam()
{
  extern int16_t __heap_start, *__brkval;
  int16_t v;
  return (int16_t)&v - (__brkval == 0 ? (int16_t)&__heap_start : (int16_t)__brkval);
}
