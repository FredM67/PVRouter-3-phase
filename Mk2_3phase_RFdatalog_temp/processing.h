/**
 * @file processing.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Public functions/variables of processing engine
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2021-2026
 *
 */

#ifndef PROCESSING_H
#define PROCESSING_H

#include "config.h"

inline uint8_t loadPrioritiesAndState[NO_OF_DUMPLOADS]; /**< load priorities */

inline constexpr uint8_t PERSISTENCE_FOR_POLARITY_CHANGE{ 1 }; /**< allows polarity changes to be confirmed */

inline constexpr uint16_t initialDelay{ 3000 };  /**< in milli-seconds, to allow time to open the Serial monitor */
inline constexpr uint16_t startUpPeriod{ 3000 }; /**< in milli-seconds, to allow LP filter to settle */

#ifdef TEMP_ENABLED
inline PayloadTx_struct< NO_OF_PHASES, temperatureSensing.size() > tx_data; /**< logging data */
#else
inline PayloadTx_struct< NO_OF_PHASES > tx_data; /**< logging data */
#endif

void printParamsForSelectedOutputMode();

#if defined(__DOXYGEN__)
void initializeProcessing();
inline void processStartUp(uint8_t phase);
inline void processStartNewCycle(float f_energy);
inline void processVoltageRawSample(const uint8_t phase, const uint16_t rawSample);
inline void processCurrentRawSample(const uint8_t phase, const uint16_t rawSample);
inline void processPlusHalfCycle(uint8_t phase);
inline void processMinusHalfCycle(uint8_t phase);
inline void processRawSamples(const uint8_t phase);
inline void processVoltage(uint8_t phase);
inline void processPolarity(uint8_t phase, uint16_t rawSample);
inline void confirmPolarity(uint8_t phase);
inline void proceedLowEnergyLevel(float f_energy);
inline void proceedHighEnergyLevel(float f_energy);
inline uint8_t nextLogicalLoadToBeAdded();
inline uint8_t nextLogicalLoadToBeRemoved();
inline void processLatestContribution(uint8_t phase);
inline float predictEnergyInBucket();
inline void processDataLogging();
inline void updatePortsStates();
inline void updatePhysicalLoadStates();
#else
void initializeProcessing() __attribute__((optimize("-O3")));
inline void processStartUp(uint8_t phase) __attribute__((always_inline));
inline void processStartNewCycle(float f_energy) __attribute__((always_inline));
inline void processVoltageRawSample(const uint8_t phase, const uint16_t rawSample) __attribute__((always_inline));
inline void processCurrentRawSample(const uint8_t phase, const uint16_t rawSample) __attribute__((always_inline));
inline void processPlusHalfCycle(uint8_t phase) __attribute__((always_inline));
inline void processMinusHalfCycle(uint8_t phase) __attribute__((always_inline));
inline void processRawSamples(const uint8_t phase) __attribute__((always_inline));
inline void processVoltage(uint8_t phase) __attribute__((always_inline));
inline void processPolarity(uint8_t phase, uint16_t rawSample) __attribute__((always_inline));
inline void confirmPolarity(uint8_t phase) __attribute__((always_inline));
inline void proceedLowEnergyLevel(float f_energy) __attribute__((always_inline));
inline void proceedHighEnergyLevel(float f_energy) __attribute__((always_inline));
inline uint8_t nextLogicalLoadToBeAdded() __attribute__((always_inline, optimize("-O3")));
inline uint8_t nextLogicalLoadToBeRemoved() __attribute__((always_inline, optimize("-O3")));
inline void processLatestContribution(uint8_t phase) __attribute__((always_inline));
inline float predictEnergyInBucket() __attribute__((always_inline));
inline void processDataLogging() __attribute__((always_inline, optimize("-O3")));
inline void updatePortsStates() __attribute__((optimize("-O3")));
inline void updatePhysicalLoadStates() __attribute__((always_inline));
#endif

#endif /* PROCESSING_H */
