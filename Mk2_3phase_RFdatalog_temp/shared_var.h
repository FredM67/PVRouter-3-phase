#ifndef SHARED_VAR_H
#define SHARED_VAR_H

#include <Arduino.h>

// Shared variables - carefully managed between ISR and loop
namespace Shared
{
// for interaction between the main processor and the ISR
inline volatile bool b_datalogEventPending{ false };      /**< async trigger to signal datalog is available */
inline volatile bool b_newMainsCycle{ false };            /**< async trigger to signal start of new main cycle based on first phase */
inline volatile bool b_overrideLoadOn[NO_OF_DUMPLOADS]{}; /**< async trigger to force specific load(s) to ON */
inline volatile bool b_reOrderLoads{ false };             /**< async trigger for loads re-ordering */
inline volatile bool b_diversionEnabled{ true };          /**< async trigger to stop diversion */

inline volatile uint16_t absenceOfDivertedEnergyCountInSeconds{ 0 }; /**< number of seconds without diverted energy */

// since there's no real locking feature for shared variables, a couple of data
// generated from inside the ISR are copied from time to time to be passed to the
// main processor. When the data are available, the ISR signals it to the main processor.
inline volatile int32_t copyOf_sumP_atSupplyPoint[NO_OF_PHASES];   /**< copy of cumulative power per phase */
inline volatile int32_t copyOf_sum_Vsquared[NO_OF_PHASES];         /**< copy of for summation of V^2 values during datalog period */
inline volatile float copyOf_energyInBucket_main;                  /**< copy of main energy bucket (over all phases) */
inline volatile uint8_t copyOf_lowestNoOfSampleSetsPerMainsCycle;  /**< copy of a mechanism to check the integrity of this code structure */
inline volatile uint16_t copyOf_sampleSetsDuringThisDatalogPeriod; /**< copy of for counting the sample sets during each datalogging period */
inline volatile uint16_t copyOf_countLoadON[NO_OF_DUMPLOADS];      /**< copy of number of cycle the load was ON (over 1 datalog period) */
}

#endif /* SHARED_VAR_H */
