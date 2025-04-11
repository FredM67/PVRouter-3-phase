/**
 * @file processing.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Public functions/variables of processing engine
 * @version 0.1
 * @date 2021-10-04
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef PROCESSING_H
#define PROCESSING_H

#include "config.h"

// analogue input pins
inline constexpr uint8_t sensorV[NO_OF_PHASES]{ 0, 2, 4 }; /**< for 3-phase PCB, voltage measurement for each phase */
inline constexpr uint8_t sensorI[NO_OF_PHASES]{ 1, 3, 5 }; /**< for 3-phase PCB, current measurement for each phase */
// ------------------------------------------

inline uint8_t loadPrioritiesAndState[NO_OF_DUMPLOADS]; /**< load priorities */

inline constexpr uint8_t PERSISTENCE_FOR_POLARITY_CHANGE{ 1 }; /**< allows polarity changes to be confirmed */

inline constexpr uint16_t initialDelay{ 3000 };  /**< in milli-seconds, to allow time to open the Serial monitor */
inline constexpr uint16_t startUpPeriod{ 3000 }; /**< in milli-seconds, to allow LP filter to settle */

// for interaction between the main processor and the ISR
inline volatile uint16_t absenceOfDivertedEnergyCount{ 0 }; /**< number of main cycles without diverted energy */
inline volatile bool b_datalogEventPending{ false };        /**< async trigger to signal datalog is available */
inline volatile bool b_newMainsCycle{ false };              /**< async trigger to signal start of new main cycle based on first phase */
inline volatile bool b_overrideLoadOn[NO_OF_DUMPLOADS]{};     /**< async trigger to force specific load(s) to ON */
inline volatile bool b_reOrderLoads{ false };               /**< async trigger for loads re-ordering */
inline volatile bool b_diversionOff{ false };               /**< async trigger to stop diversion */
inline volatile bool EDD_isIdle{ true };                    /**< energy diversion detection */

// since there's no real locking feature for shared variables, a couple of data
// generated from inside the ISR are copied from time to time to be passed to the
// main processor. When the data are available, the ISR signals it to the main processor.
inline volatile int32_t copyOf_sumP_atSupplyPoint[NO_OF_PHASES];   /**< copy of cumulative power per phase */
inline volatile int32_t copyOf_sum_Vsquared[NO_OF_PHASES];         /**< copy of for summation of V^2 values during datalog period */
inline volatile float copyOf_energyInBucket_main;                  /**< copy of main energy bucket (over all phases) */
inline volatile uint8_t copyOf_lowestNoOfSampleSetsPerMainsCycle;  /**< copy of a mechanism to check the integrity of this code structure */
inline volatile uint16_t copyOf_sampleSetsDuringThisDatalogPeriod; /**< copy of for counting the sample sets during each datalogging period */
inline volatile uint16_t copyOf_countLoadON[NO_OF_DUMPLOADS];      /**< copy of number of cycle the load was ON (over 1 datalog period) */

#ifdef TEMP_ENABLED
inline PayloadTx_struct< NO_OF_PHASES, temperatureSensing.get_size() > tx_data; /**< logging data */
#else
inline PayloadTx_struct< NO_OF_PHASES > tx_data; /**< logging data */
#endif

void printParamsForSelectedOutputMode();

void processCurrentRawSample(const uint8_t phase, const int16_t rawSample);
void processVoltageRawSample(const uint8_t phase, const int16_t rawSample);

#if defined(__DOXYGEN__)
void initializeProcessing();
inline void processStartUp(uint8_t phase);
inline void processStartNewCycle();
inline void processPlusHalfCycle(uint8_t phase);
inline void processMinusHalfCycle(uint8_t phase);
inline void processRawSamples(const uint8_t phase);
inline void processVoltage(uint8_t phase);
inline void processPolarity(uint8_t phase, int16_t rawSample);
inline void confirmPolarity(uint8_t phase);
inline void proceedLowEnergyLevel();
inline void proceedHighEnergyLevel();
inline uint8_t nextLogicalLoadToBeAdded();
inline uint8_t nextLogicalLoadToBeRemoved();
inline void processLatestContribution(uint8_t phase);
inline void processDataLogging();
inline void updatePortsStates();
inline void updatePhysicalLoadStates();
#else
void initializeProcessing() __attribute__((optimize("-O3")));
inline void processStartUp(uint8_t phase) __attribute__((always_inline));
inline void processStartNewCycle() __attribute__((always_inline));
inline void processPlusHalfCycle(uint8_t phase) __attribute__((always_inline));
inline void processMinusHalfCycle(uint8_t phase) __attribute__((always_inline));
inline void processRawSamples(const uint8_t phase) __attribute__((always_inline));
inline void processVoltage(uint8_t phase) __attribute__((always_inline));;
inline void processPolarity(uint8_t phase, int16_t rawSample) __attribute__((always_inline));
inline void confirmPolarity(uint8_t phase) __attribute__((always_inline));
inline void proceedLowEnergyLevel() __attribute__((always_inline));
inline void proceedHighEnergyLevel() __attribute__((always_inline));
inline uint8_t nextLogicalLoadToBeAdded() __attribute__((always_inline, optimize("-O3")));
inline uint8_t nextLogicalLoadToBeRemoved() __attribute__((always_inline, optimize("-O3")));
inline void processLatestContribution(uint8_t phase) __attribute__((always_inline));
inline void processDataLogging() __attribute__((always_inline, optimize("-O3")));
inline void updatePortsStates() __attribute__((optimize("-O3")));
inline void updatePhysicalLoadStates() __attribute__((always_inline));
#endif

#endif /* PROCESSING_H */
