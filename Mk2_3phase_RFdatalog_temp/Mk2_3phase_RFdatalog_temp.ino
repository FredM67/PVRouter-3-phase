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
 * @author Fred Metrich
 * @copyright Copyright (c) 2022
 *
 */
static_assert(__cplusplus >= 201703L, "**** Please define 'gnu++17' in 'platform.txt' ! ****");

#include <Arduino.h> // may not be needed, but it's probably a good idea to include this

//--------------------------------------------------------------------------------------------------
// #define TEMP_SENSOR ///< this line must be commented out if the temperature sensor is not present
// #define RF_PRESENT ///< this line must be commented out if the RFM12B module is not present

// #define PRIORITY_ROTATION ///< this line must be commented out if you want fixed priorities
// #define OFF_PEAK_TARIFF ///< this line must be commented out if there's only one single tariff each day
// #define FORCE_PIN_PRESENT ///< this line must be commented out if there's no force pin

// Output messages
#define DEBUGGING   ///< enable this line to include debugging print statements
#define SERIALPRINT ///< include 'human-friendly' print statement for commissioning - comment this line to exclude.

// #define EMONESP ///< Uncomment if an ESP WiFi module is used
// #define SERIALOUT ///< Uncomment if a wired serial connection is used
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// constants which must be set individually for each system
//
constexpr uint8_t NO_OF_PHASES{3};    /**< number of phases of the main supply. */
constexpr uint8_t NO_OF_DUMPLOADS{3}; /**< number of dump loads connected to the diverter */

constexpr uint8_t DATALOG_PERIOD_IN_SECONDS{5}; /**< Period of datalogging in seconds */

//
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
constexpr float f_powerCal[NO_OF_PHASES]{0.05322f, 0.05592f, 0.05460f};
//
// f_phaseCal is used to alter the phase of the voltage waveform relative to the current waveform.
// The algorithm interpolates between the most recent pair of voltage samples according to the value of f_phaseCal.
//
//    With f_phaseCal = 1, the most recent sample is used.
//    With f_phaseCal = 0, the previous sample is used
//    With f_phaseCal = 0.5, the mid-point (average) value in used
//
// NB. Any tool which determines the optimal value of f_phaseCal must have a similar
// scheme for taking sample values as does this sketch.
//
constexpr float f_phaseCal{1}; /**< Nominal values only */
//
// When using integer maths, calibration values that have been supplied in floating point form need to be rescaled.
constexpr int16_t i_phaseCal{256}; /**< to avoid the need for floating-point maths (f_phaseCal * 256) */
constexpr uint8_t p_phaseCal{8};   /**< to speed up math (i_phaseCal = 1 << p_phaseCal) */
//
// For datalogging purposes, f_voltageCal has been added too. Because the range of ADC values is
// similar to the actual range of volts, the optimal value for this cal factor is likely to be
// close to unity.
constexpr float f_voltageCal[NO_OF_PHASES]{0.803f, 0.803f, 0.803f}; /**< compared with Fluke 77 meter */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// other system constants
constexpr uint8_t SUPPLY_FREQUENCY{50};          /**< number of cycles/s of the grid power supply */
constexpr int8_t REQUIRED_EXPORT_IN_WATTS{30};   /**< when set to a negative value, this acts as a PV generator */
constexpr uint16_t WORKING_ZONE_IN_JOULES{3600}; /**< number of joule for 1Wh */
//--------------------------------------------------------------------------------------------------

// In this sketch, the ADC is free-running with a cycle time of ~104uS.

// ----------------
// general literals
constexpr uint16_t DATALOG_PERIOD_IN_MAINS_CYCLES{DATALOG_PERIOD_IN_SECONDS * SUPPLY_FREQUENCY}; /**< Period of datalogging in cycles */

#ifdef TEMP_SENSOR
#include <OneWire.h> // for temperature sensing
// Dallas DS18B20 commands
constexpr uint8_t SKIP_ROM{0xcc};
constexpr uint8_t CONVERT_TEMPERATURE{0x44};
constexpr uint8_t READ_SCRATCHPAD{0xbe};
constexpr int16_t UNUSED_TEMPERATURE{30000};     /**< this value (300C) is sent if no sensor has ever been detected */
constexpr int16_t OUTOFRANGE_TEMPERATURE{30200}; /**< this value (302C) is sent if the sensor reports < -55C or > +125C */
constexpr int16_t BAD_TEMPERATURE{30400};        /**< this value (304C) is sent if no sensor is present or the checksum is bad (corrupted data) */
constexpr int16_t TEMP_RANGE_LOW{-5500};
constexpr int16_t TEMP_RANGE_HIGH{12500};
#endif // #ifdef TEMP_SENSOR

#ifndef OFF_PEAK_TARIFF
#ifdef PRIORITY_ROTATION

constexpr uint32_t ROTATION_AFTER_CYCLES{8 * 3600 * SUPPLY_FREQUENCY}; /**< rotates load priorities after this period of inactivity */
volatile uint32_t absenceOfDivertedEnergyCount{0};                     /**< number of main cycles without diverted energy */

#endif // #ifdef PRIORITY_ROTATION
#else  // #ifndef OFF_PEAK_TARIFF
constexpr uint8_t ul_OFF_PEAK_DURATION{8}; /**< Duration of the off-peak period in hours */

/** @brief Config parameters for forcing a load
 *  @details This class allows the user to define when and how long a load will be forced at
 *           full power during off-peak period.
 *
 *           For each load, the user defines a pair of values: pairForceLoad => { offset, duration }.
 *           The load will be started with full power at ('start_offpeak' + 'offset') for a duration of 'duration'
 *             - all values are in hours (if between -24 and 24) or in minutes.
 *             - if the offset is negative, it's calculated from the end of the off-peak period (ie -3 means 3 hours
 * back from the end).
 *             - to leave the load at full power till the end of the off-peak period, set the duration to 'UINT16_MAX'
 * (somehow infinite time)
 */
class pairForceLoad
{
public:
  constexpr pairForceLoad() = default;
  constexpr pairForceLoad(int16_t _iStartOffset, uint16_t _uiDuration = UINT16_MAX)
      : iStartOffset(_iStartOffset), uiDuration(_uiDuration)
  {
  }

  constexpr int16_t getStartOffset() const
  {
    return iStartOffset;
  }
  constexpr uint16_t getDuration() const
  {
    return uiDuration;
  }

private:
  int16_t iStartOffset{0};         /**< the start offset from the off-peak begin in hours or minutes */
  uint16_t uiDuration{UINT16_MAX}; /**< the duration for forcing the load in hours or minutes */
};

constexpr uint8_t uiTemperature{100}; /**< the temperature threshold to stop forcing in °C */

constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS] = {{-3, 2},    /**< force config for load #1 */
                                                         {-3, 120},  /**< force config for load #2 */
                                                         {-180, 2}}; /**< force config for load #3 */
#endif // #ifndef OFF_PEAK_TARIFF

// -------------------------------
// definitions of enumerated types

/** Polarities */
enum class Polarities : uint8_t
{
  NEGATIVE, /**< polarity is negative */
  POSITIVE  /**< polarity is positive */
};

/** Output modes */
enum class OutputModes : uint8_t
{
  ANTI_FLICKER, /**< Anti-flicker mode */
  NORMAL        /**< Normal mode */
};

/** Load state (for use if loads are active high (Rev 2 PCB)) */
enum class LoadStates : uint8_t
{
  LOAD_OFF, /**< load is OFF */
  LOAD_ON   /**< load is ON */
};
// enum loadStates {LOAD_ON, LOAD_OFF}; /**< for use if loads are active low (original PCB) */

constexpr uint8_t loadStateOnBit{0x80U}; /**< bit mask for load state ON */
constexpr uint8_t loadStateMask{0x7FU};  /**< bit mask for masking load state */

LoadStates physicalLoadState[NO_OF_DUMPLOADS]; /**< Physical state of the loads */
uint16_t countLoadON[NO_OF_DUMPLOADS];         /**< Number of cycle the load was ON (over 1 datalog period) */

constexpr OutputModes outputMode{OutputModes::NORMAL}; /**< Output mode to be used */

// Load priorities at startup
uint8_t loadPrioritiesAndState[NO_OF_DUMPLOADS]{0, 1, 2}; /**< load priorities and states. */

//--------------------------------------------------------------------------------------------------
#ifdef EMONESP
#undef SERIALPRINT // Must not corrupt serial output to emonHub with 'human-friendly' printout
#undef SERIALOUT
#undef DEBUGGING
#include <ArduinoJson.h>
#endif

#ifdef SERIALOUT
#undef EMONESP
#undef SERIALPRINT // Must not corrupt serial output to emonHub with 'human-friendly' printout
#undef DEBUGGING
#endif
//--------------------------------------------------------------------------------------------------

/* --------------------------------------
   RF configuration (for the RFM12B module)
   frequency options are RF12_433MHZ, RF12_868MHZ or RF12_915MHZ
*/
#ifdef RF_PRESENT
#define RF69_COMPAT 0 // for the RFM12B
// #define RF69_COMPAT 1 // for the RF69
#include <JeeLib.h>

#define FREQ RF12_868MHZ

constexpr int nodeID{10};        /**<  RFM12B node ID */
constexpr int networkGroup{210}; /**< wireless network group - needs to be same for all nodes */
constexpr int UNO{1};            /**< for when the processor contains the UNO bootloader. */
#endif                           // #ifdef RF_PRESENT

/** @brief container for datalogging
 *  @details This class is used for datalogging.
 */
class PayloadTx_struct
{
public:
  int16_t power;                     /**< main power, import = +ve, to match OEM convention */
  int16_t power_L[NO_OF_PHASES];     /**< power for phase #, import = +ve, to match OEM convention */
  int16_t Vrms_L_x100[NO_OF_PHASES]; /**< average voltage over datalogging period (in 100th of Volt)*/
#ifdef TEMP_SENSOR
  int16_t temperature_x100{UNUSED_TEMPERATURE}; /**< temperature in 100th of °C */
#endif
};

PayloadTx_struct tx_data; /**< logging data */

// ----------- Pinout assignments  -----------
//
// digital pins:
// D0 & D1 are reserved for the Serial i/f
// D2 is for the RFM12B
#ifdef OFF_PEAK_TARIFF
constexpr uint8_t offPeakForcePin{3}; /**< for 3-phase PCB, off-peak trigger */
#endif

#ifdef FORCE_PIN_PRESENT
constexpr uint8_t forcePin{4};
#endif

#ifdef TEMP_SENSOR
constexpr uint8_t tempSensorPin{/*4*/}; /**< for 3-phase PCB, sensor pin */
#endif
constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS]{5, 6, 7}; /**< for 3-phase PCB, Load #1/#2/#3 (Rev 2 PCB) */
// D8 is not in use
constexpr uint8_t watchDogPin{9};
// D10 is for the RFM12B
// D11 is for the RFM12B
// D12 is for the RFM12B
// D13 is for the RFM12B

// analogue input pins
constexpr uint8_t sensorV[NO_OF_PHASES]{0, 2, 4}; /**< for 3-phase PCB, voltage measurement for each phase */
constexpr uint8_t sensorI[NO_OF_PHASES]{1, 3, 5}; /**< for 3-phase PCB, current measurement for each phase */

// --------------  general global variables -----------------
//
// Some of these variables are used in multiple blocks so cannot be static.
// For integer maths, some variables need to be 'int32_t'
//
bool beyondStartUpPeriod{false};        /**< start-up delay, allows things to settle */
constexpr uint16_t initialDelay{3000};  /**< in milli-seconds, to allow time to open the Serial monitor */
constexpr uint16_t startUpPeriod{3000}; /**< in milli-seconds, to allow LP filter to settle */

#ifdef OFF_PEAK_TARIFF
uint32_t ul_TimeOffPeak; /**< 'timestamp' for start of off-peak period */

/**
 * @brief Template class for Load-Forcing
 * @details The array is initialized at compile time so it can be read-only and
 *          the performance and code size are better
 *
 * @tparam N # of loads
 */
template <uint8_t N>
class _rg_OffsetForce
{
public:
  constexpr _rg_OffsetForce()
      : _rg()
  {
    constexpr uint16_t uiPeakDurationInSec{ul_OFF_PEAK_DURATION * 3600};
    // calculates offsets for force start and stop of each load
    for (uint8_t i = 0; i != N; ++i)
    {
      const bool bOffsetInMinutes{rg_ForceLoad[i].getStartOffset() > 24 || rg_ForceLoad[i].getStartOffset() < -24};
      const bool bDurationInMinutes{rg_ForceLoad[i].getDuration() > 24 && UINT16_MAX != rg_ForceLoad[i].getDuration()};

      _rg[i][0] = ((rg_ForceLoad[i].getStartOffset() >= 0) ? 0 : uiPeakDurationInSec) + rg_ForceLoad[i].getStartOffset() * (bOffsetInMinutes ? 60ul : 3600ul);
      _rg[i][0] *= 1000ul; // convert in milli-seconds

      if (UINT8_MAX == rg_ForceLoad[i].getDuration())
        _rg[i][1] = rg_ForceLoad[i].getDuration();
      else
        _rg[i][1] = _rg[i][0] + rg_ForceLoad[i].getDuration() * (bDurationInMinutes ? 60ul : 3600ul) * 1000ul;
    }
  }
  const uint32_t (&operator[](uint8_t i) const)[2]
  {
    return _rg[i];
  }

private:
  uint32_t _rg[N][2];
};
constexpr auto rg_OffsetForce = _rg_OffsetForce<NO_OF_DUMPLOADS>(); /**< start & stop offsets for each load */
#endif

int32_t l_DCoffset_V[NO_OF_PHASES]; /**< <--- for LPF */

// Define operating limits for the LP filters which identify DC offset in the voltage
// sample streams. By limiting the output range, these filters always should start up
// correctly.
constexpr int32_t l_DCoffset_V_min{(512L - 100L) * 256L}; /**< mid-point of ADC minus a working margin */
constexpr int32_t l_DCoffset_V_max{(512L + 100L) * 256L}; /**< mid-point of ADC plus a working margin */
constexpr int32_t l_DCoffset_I_nom{512L};                 /**< nominal mid-point value of ADC @ x1 scale */

/**< main energy bucket for 3-phase use, with units of Joules * SUPPLY_FREQUENCY */
constexpr float f_capacityOfEnergyBucket_main{(float)(WORKING_ZONE_IN_JOULES * SUPPLY_FREQUENCY)};
/**< for resetting flexible thresholds */
constexpr float f_midPointOfEnergyBucket_main{f_capacityOfEnergyBucket_main * 0.5f};
/**< threshold in anti-flicker mode - must not exceed 0.4 */
constexpr float f_offsetOfEnergyThresholdsInAFmode{0.1f};

/**
 * @brief set default threshold at compile time so the variable can be read-only
 *
 * @param lower True to set the lower threshold, false for higher
 * @return the corresponding threshold
 */
constexpr float initThreshold(const bool lower)
{
  return lower
             ? f_capacityOfEnergyBucket_main * (0.5f - ((OutputModes::ANTI_FLICKER == outputMode) ? f_offsetOfEnergyThresholdsInAFmode : 0))
             : f_capacityOfEnergyBucket_main * (0.5f + ((OutputModes::ANTI_FLICKER == outputMode) ? f_offsetOfEnergyThresholdsInAFmode : 0));
}

constexpr float f_lowerThreshold_default{initThreshold(true)};  /**< lower default threshold set accordingly to the output mode */
constexpr float f_upperThreshold_default{initThreshold(false)}; /**< upper default threshold set accordingly to the output mode */

float f_energyInBucket_main{0}; /**< main energy bucket (over all phases) */
float f_lowerEnergyThreshold;   /**< dynamic lower threshold */
float f_upperEnergyThreshold;   /**< dynamic upper threshold */

// for improved control of multiple loads
bool b_recentTransition{false};                 /**< a load state has been recently toggled */
uint8_t postTransitionCount;                    /**< counts the number of cycle since last transition */
constexpr uint8_t POST_TRANSITION_MAX_COUNT{3}; /**< allows each transition to take effect */
// constexpr uint8_t POST_TRANSITION_MAX_COUNT{50}; /**< for testing only */
uint8_t activeLoad{NO_OF_DUMPLOADS}; /**< current active load */

int32_t l_sumP[NO_OF_PHASES];                /**< cumulative power per phase */
int32_t l_sampleVminusDC[NO_OF_PHASES];      /**< for the phaseCal algorithm */
int32_t l_lastSampleVminusDC[NO_OF_PHASES];  /**< for the phaseCal algorithm */
int32_t l_cumVdeltasThisCycle[NO_OF_PHASES]; /**< for the LPF which determines DC offset (voltage) */
int32_t l_sumP_atSupplyPoint[NO_OF_PHASES];  /**< for summation of 'real power' values during datalog period */
int32_t l_sum_Vsquared[NO_OF_PHASES];        /**< for summation of V^2 values during datalog period */

uint8_t
    n_samplesDuringThisMainsCycle[NO_OF_PHASES]; /**< number of sample sets for each phase during each mains cycle */
uint16_t i_sampleSetsDuringThisDatalogPeriod;    /**< number of sample sets during each datalogging period */
uint8_t n_cycleCountForDatalogging{0};           /**< for counting how often datalog is updated */

uint8_t n_lowestNoOfSampleSetsPerMainsCycle; /**< For a mechanism to check the integrity of this code structure */

// for interaction between the main processor and the ISR
volatile bool b_datalogEventPending{false};   /**< async trigger to signal datalog is available */
volatile bool b_newMainsCycle{false};         /**< async trigger to signal start of new main cycle based on first phase */
volatile bool b_forceLoadOn[NO_OF_DUMPLOADS]; /**< async trigger to force specific load(s) to ON */
#ifdef PRIORITY_ROTATION
volatile bool b_reOrderLoads{false}; /**< async trigger for loads re-ordering */
#endif

// since there's no real locking feature for shared variables, a couple of data
// generated from inside the ISR are copied from time to time to be passed to the
// main processor. When the data are available, the ISR signals it to the main processor.
volatile int32_t copyOf_sumP_atSupplyPoint[NO_OF_PHASES];   /**< copy of cumulative power per phase */
volatile int32_t copyOf_sum_Vsquared[NO_OF_PHASES];         /**< copy of for summation of V^2 values during datalog period */
volatile float copyOf_energyInBucket_main;                  /**< copy of main energy bucket (over all phases) */
volatile uint8_t copyOf_lowestNoOfSampleSetsPerMainsCycle;  /**<  */
volatile uint16_t copyOf_sampleSetsDuringThisDatalogPeriod; /**< copy of for counting the sample sets during each
                                                               datalogging period */
volatile uint16_t
    copyOf_countLoadON[NO_OF_DUMPLOADS]; /**< copy of number of cycle the load was ON (over 1 datalog period) */

#ifdef TEMP_SENSOR
// For temperature sensing
OneWire oneWire(tempSensorPin);
#endif

// For an enhanced polarity detection mechanism, which includes a persistence check
constexpr uint8_t PERSISTENCE_FOR_POLARITY_CHANGE{2};    /**< allows polarity changes to be confirmed */
Polarities polarityOfMostRecentSampleV[NO_OF_PHASES];    /**< for zero-crossing detection */
Polarities polarityConfirmed[NO_OF_PHASES];              /**< for zero-crossing detection */
Polarities polarityConfirmedOfLastSampleV[NO_OF_PHASES]; /**< for zero-crossing detection */

static void processStartNewCycle();
static uint8_t nextLogicalLoadToBeAdded();
static uint8_t nextLogicalLoadToBeRemoved();
static void proceedHighEnergyLevel();
static void proceedLowEnergyLevel();
static void processDataLogging();
static bool proceedLoadPrioritiesAndForcing(const int16_t currentTemperature_x100);
static void sendResults(bool bOffPeak);
static void printConfiguration();
static void send_rf_data();

/**
 * @brief update the control ports for each of the physical loads
 *
 */
void updatePortsStates()
{
  uint8_t i{NO_OF_DUMPLOADS};

  do
  {
    --i;
    // update the local load's state.
    if (LoadStates::LOAD_OFF == physicalLoadState[i])
      setPinState(physicalLoadPin[i], false);
    else
    {
      ++countLoadON[i];
      setPinState(physicalLoadPin[i], true);
    }
  } while (i);
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
  static uint8_t sample_index{0};
  int16_t rawSample;

  switch (sample_index)
  {
  case 0:
    rawSample = ADC;           // store the ADC value (this one is for Voltage L1)
    ADMUX = bit(REFS0) + sensorV[1]; // the conversion for I1 is already under way
    ++sample_index;            // increment the control flag
    //
    processVoltageRawSample(0, rawSample);
    break;
  case 1:
    rawSample = ADC;           // store the ADC value (this one is for Current L1)
    ADMUX = bit(REFS0) + sensorI[1]; // the conversion for V2 is already under way
    ++sample_index;            // increment the control flag
    //
    processCurrentRawSample(0, rawSample);
    break;
  case 2:
    rawSample = ADC;           // store the ADC value (this one is for Voltage L2)
    ADMUX = bit(REFS0) + sensorV[2]; // the conversion for I2 is already under way
    ++sample_index;            // increment the control flag
    //
    processVoltageRawSample(1, rawSample);
    break;
  case 3:
    rawSample = ADC;           // store the ADC value (this one is for Current L2)
    ADMUX = bit(REFS0) + sensorI[2]; // the conversion for V3 is already under way
    ++sample_index;            // increment the control flag
    //
    processCurrentRawSample(1, rawSample);
    break;
  case 4:
    rawSample = ADC;           // store the ADC value (this one is for Voltage L3)
    ADMUX = bit(REFS0) + sensorV[0]; // the conversion for I3 is already under way
    ++sample_index;            // increment the control flag
    //
    processVoltageRawSample(2, rawSample);
    break;
  case 5:
    rawSample = ADC;           // store the ADC value (this one is for Current L3)
    ADMUX = bit(REFS0) + sensorI[0]; // the conversion for V1 is already under way
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

/*!
 *  @defgroup TimeCritical Time critical functions Group
 *  Functions used by the ISR
 */

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
  // remove most of the DC offset from the current sample (the precise value does not matter)
  int32_t sampleIminusDC = ((int32_t)(rawSample - l_DCoffset_I_nom)) << 8;
  //
  // phase-shift the voltage waveform so that it aligns with the grid current waveform
  int32_t phaseShiftedSampleVminusDC = l_lastSampleVminusDC[phase] + (((l_sampleVminusDC[phase] - l_lastSampleVminusDC[phase]) << p_phaseCal) >> 8);
  //
  // calculate the "real power" in this sample pair and add to the accumulated sum
  int32_t filtV_div4 = phaseShiftedSampleVminusDC >> 2; // reduce to 16-bits (now x64, or 2^6)
  int32_t filtI_div4 = sampleIminusDC >> 2;             // reduce to 16-bits (now x64, or 2^6)
  int32_t instP = filtV_div4 * filtI_div4;              // 32-bits (now x4096, or 2^12)
  instP >>= 12;                                         // scaling is now x1, as for Mk2 (V_ADC x I_ADC)

  l_sumP[phase] += instP;               // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
  l_sumP_atSupplyPoint[phase] += instP; // cumulative power, scaling as for Mk2 (V_ADC x I_ADC)
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
  processRawSamples(phase); // deals with aspects that only occur at particular stages of each mains cycle
  //
  processVoltage(phase);

  if (phase == 0)
    ++i_sampleSetsDuringThisDatalogPeriod;
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
  l_lastSampleVminusDC[phase] = l_sampleVminusDC[phase]; // required for phaseCal algorithm
  // remove DC offset from each raw voltage sample by subtracting the accurate value
  // as determined by its associated LP filter.
  l_sampleVminusDC[phase] = (((int32_t)rawSample) << 8) - l_DCoffset_V[phase];
  polarityOfMostRecentSampleV[phase] = (l_sampleVminusDC[phase] > 0) ? Polarities::POSITIVE : Polarities::NEGATIVE;
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

  ++count[phase];
  if (count[phase] > PERSISTENCE_FOR_POLARITY_CHANGE)
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
  int32_t filtV_div4 = l_sampleVminusDC[phase] >> 2; // reduce to 16-bits (now x64, or 2^6)
  int32_t inst_Vsquared = filtV_div4 * filtV_div4;   // 32-bits (now x4096, or 2^12)
  inst_Vsquared >>= 12;                              // scaling is now x1 (V_ADC x I_ADC)
  l_sum_Vsquared[phase] += inst_Vsquared;            // cumulative V^2 (V_ADC x I_ADC)
  //
  // store items for use during next loop
  l_cumVdeltasThisCycle[phase] += l_sampleVminusDC[phase];          // for use with LP filter
  polarityConfirmedOfLastSampleV[phase] = polarityConfirmed[phase]; // for identification of half cycle boundaries
  ++n_samplesDuringThisMainsCycle[phase];                           // for real power calculations
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
    if (beyondStartUpPeriod && (0 == phase) && (2 == n_samplesDuringThisMainsCycle[0])) // lower value for larger sample set
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
    return; // still settling, do nothing

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
    l_DCoffset_V[phase] = l_DCoffset_V_min;
  else if (l_DCoffset_V[phase] > l_DCoffset_V_max)
    l_DCoffset_V[phase] = l_DCoffset_V_max;
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
    if (0x00 == (loadPrioritiesAndState[index] & loadStateOnBit))
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
    if (loadPrioritiesAndState[index] & loadStateOnBit)
      return (index);
  } while (index);

  return (NO_OF_DUMPLOADS);
}

/**
 * @brief Process the case of high energy level, some action may be required.
 *
 * @ingroup TimeCritical
 */
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
    f_upperEnergyThreshold = f_energyInBucket_main;

    // the energy thresholds must remain within range
    if (f_upperEnergyThreshold > f_capacityOfEnergyBucket_main)
      f_upperEnergyThreshold = f_capacityOfEnergyBucket_main;

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
  bool bOK_toRemoveLoad{true};
  auto tempLoad{nextLogicalLoadToBeRemoved()};

  if (tempLoad >= NO_OF_DUMPLOADS)
    return;

  // a load which is now ON has been identified for potentially being switched OFF
  if (b_recentTransition)
  {
    // During the post-transition period, any decrease in the energy level is noted.
    f_lowerEnergyThreshold = f_energyInBucket_main;

    // the energy thresholds must remain within range
    if (f_lowerEnergyThreshold < 0)
      f_lowerEnergyThreshold = 0;

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
 * @brief Process the lastest contribution after each phase specific new cycle
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
    b_newMainsCycle = true;                            //  a 50 Hz 'tick' for use by the main code
    f_energyInBucket_main -= REQUIRED_EXPORT_IN_WATTS; // energy scale is Joules x 50
  }
  // Applying max and min limits to the main accumulator's level
  // is deferred until after the energy related decisions have been taken
  //
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
  processLatestContribution(phase); // runs at 6.6 ms intervals

  // A performance check to monitor and display the minimum number of sets of
  // ADC samples per mains cycle, the expected number being 20ms / (104us * 6) = 32.05
  //
  if (0 == phase)
  {
    if (n_samplesDuringThisMainsCycle[phase] < n_lowestNoOfSampleSetsPerMainsCycle)
      n_lowestNoOfSampleSetsPerMainsCycle = n_samplesDuringThisMainsCycle[phase];

    processDataLogging();
  }

  l_sumP[phase] = 0;
  n_samplesDuringThisMainsCycle[phase] = 0;
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
#ifdef PRIORITY_ROTATION
  if (b_reOrderLoads)
  {
    uint8_t i{NO_OF_DUMPLOADS - 1};
    const auto temp{loadPrioritiesAndState[i]};
    do
    {
      --i;
      loadPrioritiesAndState[i + 1] = loadPrioritiesAndState[i];
    } while (i);
    loadPrioritiesAndState[i] = temp;

    b_reOrderLoads = false;
  }

#ifndef OFF_PEAK_TARIFF
  if (0x00 == (loadPrioritiesAndState[0] & loadStateOnBit))
    ++absenceOfDivertedEnergyCount;
  else
    absenceOfDivertedEnergyCount = 0;
#endif // OFF_PEAK_TARIFF

#endif // PRIORITY_ROTATION

  uint8_t i{NO_OF_DUMPLOADS};
  do
  {
    --i;
    const auto iLoad{loadPrioritiesAndState[i] & loadStateMask};
    physicalLoadState[iLoad] = (loadPrioritiesAndState[i] & loadStateOnBit) || b_forceLoadOn[iLoad]
                                   ? LoadStates::LOAD_ON
                                   : LoadStates::LOAD_OFF;
  } while (i);
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
  if (++n_cycleCountForDatalogging < DATALOG_PERIOD_IN_MAINS_CYCLES)
    return; // data logging period not yet reached

  n_cycleCountForDatalogging = 0;

  uint8_t phase{NO_OF_PHASES};
  do
  {
    --phase;
    copyOf_sumP_atSupplyPoint[phase] = l_sumP_atSupplyPoint[phase];
    l_sumP_atSupplyPoint[phase] = 0;

    copyOf_sum_Vsquared[phase] = l_sum_Vsquared[phase];
    l_sum_Vsquared[phase] = 0;
  } while (phase);

  uint8_t i{NO_OF_DUMPLOADS};
  do
  {
    --i;
    copyOf_countLoadON[i] = countLoadON[i];
    countLoadON[i] = 0;
  } while (i);

  copyOf_sampleSetsDuringThisDatalogPeriod = i_sampleSetsDuringThisDatalogPeriod; // (for diags only)
  copyOf_lowestNoOfSampleSetsPerMainsCycle = n_lowestNoOfSampleSetsPerMainsCycle; // (for diags only)
  copyOf_energyInBucket_main = f_energyInBucket_main;                             // (for diags only)

  n_lowestNoOfSampleSetsPerMainsCycle = UINT8_MAX;
  i_sampleSetsDuringThisDatalogPeriod = 0;

  // signal the main processor that logging data are available
  b_datalogEventPending = true;
}
/* End of helper functions which are used by the ISR
   -------------------------------------------------
*/

/**
 * @brief Prints data logs to the Serial output in text or json format
 *
 * @param bOffPeak true if off-peak tariff is active
 */
void sendResults(bool bOffPeak)
{
  uint8_t phase;

#ifdef RF_PRESENT
  send_rf_data(); // *SEND RF DATA*
#endif

#if defined SERIALOUT && !defined EMONESP
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

#ifdef TEMP_SENSOR
  Serial.print(", temperature ");
  Serial.print((float)tx_data.temperature_x100 / 100);
#endif
  Serial.println(F(")"));
#endif // if defined SERIALOUT && !defined EMONESP

#if defined EMONESP && !defined SERIALOUT
  StaticJsonDocument<200> doc;
  char strPhase[]{"L0"};
  char strLoad[]{"LOAD_0"};

  for (phase = 0; phase < NO_OF_PHASES; ++phase)
  {
    doc[strPhase] = tx_data.power_L[phase];
    ++strPhase[1];
  }

  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    doc[strLoad] = (100 * copyOf_countLoadON[i]) / DATALOG_PERIOD_IN_MAINS_CYCLES;
    ++strLoad[5];
  }

#ifdef OFF_PEAK_TARIFF
  doc["OFF_PEAK_TARIFF"] = bOffPeak ? true : false;
#endif

  // Generate the minified JSON and send it to the Serial port.
  //
  serializeJson(doc, Serial);

  // Start a new line
  Serial.println();
  delay(50);
#endif // if defined EMONESP && !defined SERIALOUT

#if defined SERIALPRINT && !defined EMONESP
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

#ifdef TEMP_SENSOR
  Serial.print(", temperature ");
  Serial.print((float)tx_data.temperature_x100 / 100);
#endif // TEMP_SENSOR
  Serial.print(F(", (minSampleSets/MC "));
  Serial.print(copyOf_lowestNoOfSampleSetsPerMainsCycle);
  Serial.print(F(", #ofSampleSets "));
  Serial.print(copyOf_sampleSetsDuringThisDatalogPeriod);
#ifndef OFF_PEAK_TARIFF
#ifdef PRIORITY_ROTATION
  Serial.print(F(", NoED "));
  Serial.print(absenceOfDivertedEnergyCount);
#endif // PRIORITY_ROTATION
#endif // OFF_PEAK_TARIFF
  Serial.println(F(")"));
#endif // if defined SERIALPRINT && !defined EMONESP
}

/**
 * @brief Prints the load priorities to the Serial output.
 *
 */
void logLoadPriorities()
{
#ifdef DEBUGGING
  Serial.println(F("loadPriority: "));
  for (const auto loadPrioAndState : loadPrioritiesAndState)
  {
    Serial.print(F("\tload "));
    Serial.println(loadPrioAndState);
  }
#endif
}

/**
 * @brief This function set all 3 loads to full power.
 *
 * @return true if loads are forced
 * @return false
 */
bool forceFullPower()
{
#ifdef FORCE_PIN_PRESENT
  const uint8_t pinState{!!(PIND & bit(forcePin))};

  for (auto &bForceLoad : b_forceLoadOn)
    bForceLoad = !pinState;

  return !pinState;
#else
  return false;
#endif
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
bool proceedLoadPrioritiesAndForcing(const int16_t currentTemperature_x100)
{
#ifdef OFF_PEAK_TARIFF
  uint8_t i;
  constexpr int16_t iTemperatureThreshold_x100{uiTemperature * 100};
  static uint8_t pinOffPeakState{HIGH};
  const uint8_t pinNewState{!!(PIND & bit(offPeakForcePin))};

  if (pinOffPeakState && !pinNewState)
  {
// we start off-peak period
#ifndef NO_OUTPUT
    Serial.println(F("Change to off-peak period!"));
#endif // NO_OUTPUT
    ul_TimeOffPeak = millis();

#ifdef PRIORITY_ROTATION
    b_reOrderLoads = true;

    // waits till the priorities have been rotated from inside the ISR
    do
    {
      delay(10);
    } while (b_reOrderLoads);
#endif // PRIORITY_ROTATION

    // prints the (new) load priorities
    logLoadPriorities();
  }
  else
  {
    const auto ulElapsedTime{(uint32_t)(millis() - ul_TimeOffPeak)};

    for (i = 0; i < NO_OF_DUMPLOADS; ++i)
    {
      // for each load, if we're inside off-peak period and within the 'force period', trigger the ISR to turn the
      // load ON
      if (!pinOffPeakState && !pinNewState && (ulElapsedTime >= rg_OffsetForce[i][0]) && (ulElapsedTime < rg_OffsetForce[i][1]))
        b_forceLoadOn[i] = currentTemperature_x100 <= iTemperatureThreshold_x100;
      else
        b_forceLoadOn[i] = false;
    }
  }
// end of off-peak period
#ifndef NO_OUTPUT
  if (!pinOffPeakState && pinNewState)
    Serial.println(F("Change to peak period!"));
#endif // NO_OUTPUT

  pinOffPeakState = pinNewState;

  return (LOW == pinOffPeakState);
#else // OFF_PEAK_TARIFF
#ifdef PRIORITY_ROTATION
  if (ROTATION_AFTER_CYCLES < absenceOfDivertedEnergyCount)
  {
    b_reOrderLoads = true;
    absenceOfDivertedEnergyCount = 0;

    // waits till the priorities have been rotated from inside the ISR
    do
    {
      delay(10);
    } while (b_reOrderLoads);

    // prints the (new) load priorities
    logLoadPriorities();
  }
#endif // PRIORITY_ROTATION
  return false;
#endif // OFF_PEAK_TARIFF
}

/**
 * @brief Print the configuration during start
 *
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

    Serial.print(F("\tf_voltageCal, for Vrms_L"));
    Serial.print(phase + 1);
    Serial.print(F(" =    "));
    Serial.println(f_voltageCal[phase], 5);
  }

  Serial.print(F("\tf_phaseCal for all phases =     "));
  Serial.println(f_phaseCal);

  Serial.print(F("\tExport rate (Watts) = "));
  Serial.println(REQUIRED_EXPORT_IN_WATTS);

  Serial.print(F("\tzero-crossing persistence (sample sets) = "));
  Serial.println(PERSISTENCE_FOR_POLARITY_CHANGE);

  printParamsForSelectedOutputMode();

  Serial.print("Temperature capability ");
#ifdef TEMP_SENSOR
  Serial.println(F("is present"));
#else
  Serial.println(F("is NOT present"));
#endif

  Serial.print("Dual-tariff capability ");
#ifdef OFF_PEAK_TARIFF
  Serial.println(F("is present"));
  printOffPeakConfiguration();
#else
  Serial.println(F("is NOT present"));
#endif

  Serial.print("Load rotation feature ");
#ifdef PRIORITY_ROTATION
  Serial.println(F("is present"));
#else
  Serial.println(F("is NOT present"));
#endif

  Serial.print("RF capability ");
#ifdef RF_PRESENT
  Serial.print(F("IS present, Freq = "));
  if (FREQ == RF12_433MHZ)
    Serial.println(F("433 MHz"));
  else if (FREQ == RF12_868MHZ)
    Serial.println(F("868 MHz"));
  rf12_initialize(nodeID, FREQ, networkGroup); // initialize RF
#else
  Serial.println(F("is NOT present"));
#endif

  Serial.print("Datalogging capability ");
#ifdef SERIALPRINT
  Serial.println(F("is present"));
#else
  Serial.println(F("is NOT present"));
#endif
}

#ifdef OFF_PEAK_TARIFF
/**
 * @brief Print the settings for off-peak period
 *
 */
void printOffPeakConfiguration()
{
  Serial.print(F("\tDuration of off-peak period is "));
  Serial.print(ul_OFF_PEAK_DURATION);
  Serial.println(F(" hours."));

  Serial.print(F("\tTemperature threshold is "));
  Serial.print(uiTemperature);
  Serial.println(F("°C."));

  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
  {
    Serial.print(F("\tLoad #"));
    Serial.print(i + 1);
    Serial.println(F(":"));

    Serial.print(F("\t\tStart "));
    if (rg_ForceLoad[i].getStartOffset() >= 0)
    {
      Serial.print(rg_ForceLoad[i].getStartOffset());
      Serial.print(F(" hours/minutes after begin of off-peak period "));
    }
    else
    {
      Serial.print(-rg_ForceLoad[i].getStartOffset());
      Serial.print(F(" hours/minutes before the end of off-peak period "));
    }
    if (rg_ForceLoad[i].getDuration() == UINT16_MAX)
      Serial.println(F("till the end of the period."));
    else
    {
      Serial.print(F("for a duration of "));
      Serial.print(rg_ForceLoad[i].getDuration());
      Serial.println(F(" hour/minute(s)."));
    }
    Serial.print(F("\t\tCalculated offset in seconds: "));
    Serial.println(rg_OffsetForce[i][0] / 1000);
    Serial.print(F("\t\tCalculated duration in seconds: "));
    Serial.println(rg_OffsetForce[i][1] / 1000);
  }
}
#endif

/**
 * @brief Print the settings used for the selected output mode.
 *
 */
void printParamsForSelectedOutputMode()
{
#ifndef NO_OUTPUT
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
#endif
}

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
 * @return The temperature in °C (x100).
 */
int16_t readTemperature()
{
  uint8_t buf[9];

  if (oneWire.reset())
  {
    oneWire.reset();
    oneWire.write(SKIP_ROM);
    oneWire.write(READ_SCRATCHPAD);
    for (auto &buf_elem : buf)
      buf_elem = oneWire.read();

    if (oneWire.crc8(buf, 8) != buf[8])
      return BAD_TEMPERATURE;

    // result is temperature x16, multiply by 6.25 to convert to temperature x100
    int16_t result = (buf[1] << 8) | buf[0];
    result = (result * 6) + (result >> 2);
    if (result <= TEMP_RANGE_LOW || result >= TEMP_RANGE_HIGH)
      return OUTOFRANGE_TEMPERATURE; // return value ('Out of range')

    return result;
  }
  return BAD_TEMPERATURE;
}
#endif // #ifdef TEMP_SENSOR

#ifdef RF_PRESENT
/**
 * @brief Send the logging data through RF.
 * @details For better performance, the RFM12B needs to remain in its
 *          active state rather than being periodically put to sleep.
 *
 */
void send_rf_data()
{
  // check whether it's ready to send, and an exit route if it gets stuck
  uint8_t i = 10;
  do
  {
    rf12_recvDone();
  } while (!rf12_canSend() && --i)

      rf12_sendNow(0, &tx_data, sizeof tx_data);
}
#endif // #ifdef RF_PRESENT

/**
 * @brief Get the available RAM during setup
 *
 * @return int The amount of free RAM
 */
int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
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

  Serial.begin(9600); // initialize Serial interface, Do NOT set greater than 9600

#if !defined SERIALOUT && !defined EMONESP
  // On start, always display config info in the serial monitor
  printConfiguration();
#endif

  // initializes all loads to OFF at startup
  uint8_t i{NO_OF_DUMPLOADS};
  do
  {
    --i;
    if (physicalLoadPin[i] < 8)
      DDRD |= bit(physicalLoadPin[i]); // driver pin for Load #n
    else
      DDRB |= bit(physicalLoadPin[i] - 8); // driver pin for Load #n

    loadPrioritiesAndState[i] &= loadStateMask;
  } while (i);

  updatePhysicalLoadStates(); // allows the logical-to-physical mapping to be changed

  updatePortsStates(); // updates output pin states

#ifdef OFF_PEAK_TARIFF
  DDRD &= ~bit(offPeakForcePin);                     // set as input
  PORTD |= bit(offPeakForcePin);                     // enable the internal pullup resistor
  delay(100);                                        // allow time to settle
  uint8_t pinState{!!(PIND & bit(offPeakForcePin))}; // initial selection and

  ul_TimeOffPeak = millis();
#endif

#ifdef FORCE_PIN_PRESENT
  DDRD &= ~bit(forcePin); // set as input
  PORTD |= bit(forcePin); // enable the internal pullup resistor
  delay(100);             // allow time to settle
#endif

  DDRB |= bit(watchDogPin - 8);    // set as output
  setPinState(watchDogPin, false); // set to off

  for (auto &bForceLoad : b_forceLoadOn)
    bForceLoad = false;

  for (auto &DCoffset_V : l_DCoffset_V)
    DCoffset_V = 512L * 256L; // nominal mid-point value of ADC @ x256 scale

  // First stop the ADC
  bitClear(ADCSRA, ADEN);

  // Activation du free-running mode
  ADCSRB = 0x00;

  // Set up the ADC to be free-running
  bitSet(ADCSRA, ADPS0); // Set the ADC's clock to system clock / 128
  bitSet(ADCSRA, ADPS1);
  bitSet(ADCSRA, ADPS2);

  bitSet(ADCSRA, ADATE); // set the Auto Trigger Enable bit in the ADCSRA register. Because
  // bits ADTS0-2 have not been set (i.e. they are all zero), the
  // ADC's trigger source is set to "free running mode".

  bitSet(ADCSRA, ADIE); // set the ADC interrupt enable bit. When this bit is written
  // to one and the I-bit in SREG is set, the
  // ADC Conversion Complete Interrupt is activated.

  bitSet(ADCSRA, ADEN); // Enable the ADC

  bitSet(ADCSRA, ADSC); // start ADC manually first time

  sei(); // Enable Global Interrupts

  logLoadPriorities();

#ifdef TEMP_SENSOR
  convertTemperature(); // start initial temperature conversion
#endif

#ifdef DEBUGGING
  Serial.print(F(">>free RAM = "));
  Serial.println(freeRam()); // a useful value to keep an eye on
  Serial.println(F("----"));
#endif
}

/**
 * @brief Toggle the watchdog LED
 *
 */
static void toggleWatchDogLED()
{
  PINB = bit(watchDogPin - 8); // toggle pin
}

/**
 * @brief Set the Pin state for the specified pin
 *
 * @param pin pin to change [2..13]
 * @param bState state to be set
 */
inline void setPinState(const uint8_t pin, bool bState)
{
  if (bState)
  {
    if (pin < 8)
      PORTD |= bit(pin);
    else
      PORTB |= bit(pin - 8);
  }
  else
  {
    if (pin < 8)
      PORTD &= ~bit(pin);
    else
      PORTB &= ~bit(pin - 8);
  }
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

      toggleWatchDogLED();

      if (!forceFullPower())
        bOffPeak = proceedLoadPrioritiesAndForcing(iTemperature_x100); // called every second
    }
  }

  if (b_datalogEventPending)
  {
    b_datalogEventPending = false;

    tx_data.power = 0;
    uint8_t phase{NO_OF_PHASES};
    do
    {
      --phase;

      tx_data.power_L[phase] =
          copyOf_sumP_atSupplyPoint[phase] / copyOf_sampleSetsDuringThisDatalogPeriod * f_powerCal[phase];
      tx_data.power_L[phase] *= -1;

      tx_data.power += tx_data.power_L[phase];

      tx_data.Vrms_L_x100[phase] =
          (int32_t)(100 * f_voltageCal[phase] * sqrt(copyOf_sum_Vsquared[phase] / copyOf_sampleSetsDuringThisDatalogPeriod));
    } while (phase);

#ifdef TEMP_SENSOR
    iTemperature_x100 = readTemperature();
    tx_data.temperature_x100 = iTemperature_x100;
#endif

    sendResults(bOffPeak);

#ifdef TEMP_SENSOR
    convertTemperature(); // for use next time around
#endif
  }
} // end of loop()
