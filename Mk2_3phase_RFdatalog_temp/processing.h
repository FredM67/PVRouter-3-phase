/**
 * @file processing.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Public functions/variables of processing engine
 * @version 0.1
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2021-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef PROCESSING_H
#define PROCESSING_H

#include "config_derived.h"

inline uint8_t loadPrioritiesAndState[NO_OF_DUMPLOADS]; /**< load priorities */

inline constexpr uint8_t PERSISTENCE_FOR_POLARITY_CHANGE{ 1 }; /**< allows polarity changes to be confirmed */

inline constexpr uint16_t initialDelay{ 3000 };  /**< in milli-seconds, to allow time to open the Serial monitor */
inline constexpr uint16_t startUpPeriod{ 3000 }; /**< in milli-seconds, to allow LP filter to settle */

// one temperature per sensor, only with temperature sensing (TEMP_SENSOR_PRESENT is a constexpr, not a macro)
inline PayloadTx_struct< NO_OF_PHASES, TEMP_SENSOR_PRESENT ? tempSensors.size() : 0 > tx_data; /**< logging data */

void printParamsForSelectedOutputMode();

#if defined(__DOXYGEN__)
void initializeProcessing();
inline void processStartUp(uint8_t phase);
inline void processStartNewCycle();
inline void processVoltageRawSample(const uint8_t phase, const uint16_t rawSample);
inline void processCurrentRawSample(const uint8_t phase, const uint16_t rawSample);
inline void processPlusHalfCycle(uint8_t phase);
inline void processMinusHalfCycle(uint8_t phase);
inline void processRawSamples(const uint8_t phase);
inline void processVoltage(uint8_t phase);
inline void processPolarity(uint8_t phase, uint16_t rawSample);
inline void confirmPolarity(uint8_t phase);
inline void processLatestContribution(uint8_t phase);
inline void processDataLogging();
inline void updatePortsStates();
inline void updatePhysicalLoadStates();
#else
// -O3 on a function that is a call of its own: here 50 bytes smaller than with -Os
void initializeProcessing() __attribute__((optimize("-O3")));
inline void processStartUp(uint8_t phase) __attribute__((always_inline));
inline void processStartNewCycle() __attribute__((always_inline));
inline void processVoltageRawSample(const uint8_t phase, const uint16_t rawSample) __attribute__((always_inline));
inline void processCurrentRawSample(const uint8_t phase, const uint16_t rawSample) __attribute__((always_inline));
inline void processPlusHalfCycle(uint8_t phase) __attribute__((always_inline));
inline void processMinusHalfCycle(uint8_t phase) __attribute__((always_inline));
inline void processRawSamples(const uint8_t phase) __attribute__((always_inline));
inline void processVoltage(uint8_t phase) __attribute__((always_inline));
inline void processPolarity(uint8_t phase, uint16_t rawSample) __attribute__((always_inline));
inline void confirmPolarity(uint8_t phase) __attribute__((always_inline));
inline void processLatestContribution(uint8_t phase) __attribute__((always_inline));
inline void processDataLogging() __attribute__((always_inline));
// An inlined function is compiled with the options of the ISR it lands in: an optimize() attribute
// on it changes nothing (same firmware, to the byte). updatePortsStates() is a call: -O3 unrolls
// its load loop into constant masks (grid simulator: 38 cycles less on average, 88 less at most).
inline void updatePortsStates() __attribute__((optimize("-O3")));
// not inlined on purpose: inlined, it takes registers that the rest of the ISR then spills to a
// stack frame on every call (grid simulator: 30 cycles more on average, 65 more at most)
inline void updatePhysicalLoadStates() __attribute__((noinline));
#endif

#endif /* PROCESSING_H */
