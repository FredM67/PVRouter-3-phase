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

#ifndef __PROCESSING_H__
#define __PROCESSING_H__

#include "config.h"

// for interaction between the main processor and the ISR
inline volatile uint32_t absenceOfDivertedEnergyCount{ 0 }; /**< number of main cycles without diverted energy */
inline volatile bool b_datalogEventPending{ false };        /**< async trigger to signal datalog is available */
inline volatile bool b_newMainsCycle{ false };              /**< async trigger to signal start of new main cycle based on first phase */
inline volatile bool b_overrideLoadOn[NO_OF_DUMPLOADS];     /**< async trigger to force specific load(s) to ON */
inline volatile bool b_reOrderLoads{ false };               /**< async trigger for loads re-ordering */
inline volatile bool b_diversionOff{ false };               /**< async trigger to stop diversion */

// since there's no real locking feature for shared variables, a couple of data
// generated from inside the ISR are copied from time to time to be passed to the
// main processor. When the data are available, the ISR signals it to the main processor.
inline volatile int32_t copyOf_sumP_atSupplyPoint[NO_OF_PHASES];   /**< copy of cumulative power per phase */
inline volatile int32_t copyOf_sum_Vsquared[NO_OF_PHASES];         /**< copy of for summation of V^2 values during datalog period */
inline volatile float copyOf_energyInBucket_main;                  /**< copy of main energy bucket (over all phases) */
inline volatile uint8_t copyOf_lowestNoOfSampleSetsPerMainsCycle;  /**<  */
inline volatile uint16_t copyOf_sampleSetsDuringThisDatalogPeriod; /**< copy of for counting the sample sets during each datalogging period */
inline volatile uint16_t copyOf_countLoadON[NO_OF_DUMPLOADS];      /**< copy of number of cycle the load was ON (over 1 datalog period) */

#ifdef TEMP_SENSOR_PRESENT
inline PayloadTx_struct< NO_OF_PHASES, size(sensorAddrs) > tx_data; /**< logging data */
#else
inline PayloadTx_struct< NO_OF_PHASES > tx_data; /**< logging data */
#endif

void initializeProcessing();
void initializeOptionalPins();
void updatePhysicalLoadStates();
void updatePortsStates();
void printParamsForSelectedOutputMode();

void processCurrentRawSample(const uint8_t phase, const int16_t rawSample);
void processVoltageRawSample(const uint8_t phase, const int16_t rawSample);

#endif  // __PROCESSING_H__
