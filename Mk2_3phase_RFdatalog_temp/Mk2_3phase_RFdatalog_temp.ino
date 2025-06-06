/**
 * @file Mk2_3phase_RFdatalog_temp.ino
 * @author Robin Emley (www.Mk2PVrouter.co.uk)
 * @author Frederic Metrich (frederic.metrich@live.fr)
 * @brief A photovoltaic energy diverter.
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
 *
 * __May 2025: changes:__
 * - add delayed start of the diversion
 *
 * __April 2025: changes:__
 * - add telemetry feature for HomeAssistant connectivity
 * - some tiny fixes
 * - even more documentation
 *
 * __March 2024: changes:__
 * - multi-relay feature
 * - add DEMA and TEMA sliding average
 * - some tiny fixes
 * - even more documentation
 *
 * __February 2024: changes:__
 * - refactoring of 'temperature feature'
 * - refactoring of 'relay feature'
 * - new sliding average (EWMA)
 * - more documentation
 *
 * __June 2023: changes:__
 * - heavy refactoring (again)
 * - stl add-ons
 * - add relay-output feature
 *
 * __February 2023: changes:__
 * - heavy refactoring
 * - compile-time validation for pin assignment and a couple of values
 * - project can now be used with both Arduino IDE and PlatformIO (Visual Studio Code).
 * - a couple of pre-defined PlatformIO configs added
 *
 * __January 2023: changes:__
 * - the datalogging accumulator for Vsquared has been rescaled to 1/16 of its previous value 
 *   to avoid the risk of overflowing during a 20-second datalogging period.
 *
 * __November 2021, changes:__
 * - heavy refactoring/restructuring of the sketch
 * - calibration values moved to the dedicated file 'calibration.h'
 * - user-specific values (pins, ...) are now in 'config.h' all other files should/must remain unchanged
 * - added support of temperature sensors (virtually no limit of sensors count)
 * - added support for emonESP (see https://github.com/openenergymonitor/EmonESP)
 *
 * __April 2021, renamed as Mk2_3phase_RFdatalog_temp with these changes:__
 * - since this sketch is under source control, no need to write the version in the name
 * - rename function 'checkLoadPrioritySelection' to function's job
 * - made forcePin presence configurable
 * - added WatchDog LED (blink 1s ON/ 1s OFF)
 * - code enhanced to support 6 loads
 *
 * __February 2021, changes:__
 * - Added temperature threshold for off-peak period
 *
 * __January 2021, changes:__
 * - Further optimization
 * - now it's possible to specify the override period in minutes and hours
 * - initialization of runtime parameters for override period at compile-time
 *
 * __October 2020, changes:__
 * - Moving some part around (calibration values toward beginning of the sketch)
 * - renaming some preprocessor defines
 * - system/user specific data moved toward beginning of the sketch
 *
 * __June 2020, changes:__
 * - Add force pin for full power through overwrite switch
 * - Add priority rotation for single tariff
 *
 * __May 2020, changes:__
 * - Fix a bug in the initialization of off-peak offsets
 * - added detailed configuration on start-up with build timestamp
 *
 * __April 2020, changes:__
 * - Fix a bug in the load level calculation
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
 * __October 2019, renamed as Mk2_3phase_RFdatalog_temp with these changes:__
 * - This sketch has been restructured in order to make better use of the ISR.
 * - All of the time-critical code is now contained within the ISR and its helper functions.
 * - Values for datalogging are transferred to the main code using a flag-based handshake mechanism.
 * - The diversion of surplus power can no longer be affected by slower
 *   activities which may be running in the main code such as Serial statements and RF.
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
 * @author Fred Metrich
 * @copyright Copyright (c) 2025
 *
 * __February 2020: updated to Mk2_3phase_RFdatalog_3a with these changes:__
 * - removal of some redundant code in the logic for determining the next load state.
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
 * __January 2016, renamed as Mk2_3phase_RFdatalog_2 with these changes:__
 * - Improved control of multiple loads has been imported from the
 *     equivalent 1-phase sketch, Mk2_multiLoad_wired_6.ino
 * - the ISR has been upgraded to fix a possible timing anomaly
 * - variables to store ADC samples are now declared as "volatile"
 * - for RF69 RF module is now supported
 * - a performance check has been added with the result being sent to the Serial port
 * - control signals for loads are now active-high to suit the latest 3-phase PCB
 *
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
 * @authors Robin Emley
 *      www.Mk2PVrouter.co.uk *
 */

/*!
 *  @defgroup TimeCritical Time-critical functions
 *  @brief Functions used by the ISR.
 *  
 *  @details This group contains functions that are executed within the Interrupt Service Routine (ISR) 
 *           or are closely tied to time-sensitive operations. These functions are optimized for speed 
 *           and efficiency to ensure the system operates without delays or interruptions.
 *  
 *  @note These functions handle tasks such as ADC sampling, zero-crossing detection, and real-time data processing.
 */

/*!
 *  @defgroup RelayDiversion Relay diversion feature
 *  @brief Functions used for managing relay-based energy diversion.
 *  
 *  @details This group includes functions that control the diversion of surplus photovoltaic energy 
 *           to resistive loads using relays. It supports multiple loads, load prioritization, and 
 *           features like forced full-power operation and load rotation.
 *  
 *  @note These functions ensure efficient energy usage by dynamically adjusting relay states based on 
 *        surplus energy availability and user-defined priorities.
 */

/*!
 *  @defgroup TemperatureSensing Temperature sensing feature
 *  @brief Functions used for temperature monitoring and processing.
 *  
 *  @details This group contains functions that handle temperature sensing using DS18B20 sensors. 
 *           It includes reading temperature data, filtering invalid readings, and updating telemetry 
 *           with valid temperature values. The temperature data can also influence load diversion decisions.
 *  
 *  @note The system supports multiple sensors and stores temperature values as integers multiplied by 100 for precision.
 */

/*!
 *  @defgroup DualTariff Dual tariff feature
 *  @brief Functions used for managing dual tariff periods.
 *  
 *  @details This group includes functions that detect and handle changes between off-peak and on-peak tariff periods. 
 *           It adjusts load priorities and diversion strategies based on the current tariff state. 
 *           Features like forced load operation during off-peak periods are also supported.
 *  
 *  @note The dual tariff feature helps optimize energy usage and cost savings by prioritizing energy diversion 
 *        during off-peak periods.
 */

/*!
 *  @defgroup RF RF feature
 *  @brief Functions used for RF communication.
 *  
 *  @details This group contains functions that enable wireless communication using RF modules (e.g., RFM12B or RF69). 
 *           It supports telemetry transmission, remote control commands, and runtime configuration of RF settings. 
 *           The system can operate with or without an RF module.
 *  
 *  @note RF communication is optional and can be disabled at compile time if not required.
 */

/*!
 *  @defgroup Telemetry Telemetry feature
 *  @brief Functions used for data logging and external system communication.
 *  
 *  @details This group includes functions that manage telemetry data collection and transmission. 
 *           It supports integration with external systems like HomeAssistant, providing real-time data 
 *           on power, voltage, temperature, and system status. Telemetry also includes diagnostic information.
 *  
 *  @note The telemetry feature ensures that the system's performance and status can be monitored remotely.
 */

/**
 * @defgroup GeneralProcessing General Processing
 * @brief Functions and routines that handle general system processing tasks.
 *
 * This group includes functions that are not time-critical but are essential
 * for the overall operation of the system. These tasks include monitoring
 * system states, managing load priorities, and handling diversion logic.
 *
 * @details
 * - Functions in this group are typically called from the main loop or other
 *   non-interrupt contexts.
 * - They ensure the system operates correctly and adapts to changing conditions.
 */

/**
 * @defgroup Initialization Initialization
 * @brief Functions and classes responsible for system setup and configuration.
 *
 * This group includes functions and classes that handle the initialization of
 * hardware components, system parameters, and other configurations required
 * for the proper operation of the system.
 *
 * @details
 * - Ensures that all components are properly configured before the system starts.
 * - Includes setup routines for relays, pins, ADCs, and other peripherals.
 * - Provides default configurations and validations to ensure system stability.
 */

static_assert(__cplusplus >= 201703L, "**** Please define 'gnu++17' in 'platform.txt' ! ****");
static_assert(__cplusplus >= 201703L, "See also : https://github.com/FredM67/PVRouter-3-phase/blob/main/Mk2_3phase_RFdatalog_temp/Readme.md");

// The active code can be found in the other cpp/h files
