/**
 * @file main.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Function prototypes declaration
 * @version 0.1
 * @date 2023-02-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _MAIN_H
#define _MAIN_H

#include <Arduino.h>

void processStartUp(uint8_t phase);
void processStartNewCycle();
uint8_t nextLogicalLoadToBeAdded();
uint8_t nextLogicalLoadToBeRemoved();
void processMinusHalfCycle(uint8_t phase);
void processPlusHalfCycle(uint8_t phase);

void proceedHighEnergyLevel();
void proceedLowEnergyLevel();
void processDataLogging();
bool proceedLoadPrioritiesAndForcing(int16_t currentTemperature_x100);
void sendResults(bool bOffPeak);
void printConfiguration();
void printOffPeakConfiguration();
void send_rf_data();

inline void setPinState(uint8_t pin, bool bState);

void processCurrentRawSample(uint8_t phase, uint16_t rawSample);
void processVoltageRawSample(uint8_t phase, uint16_t rawSample);
void processRawSamples(uint8_t phase);

void processVoltage(uint8_t phase);

void processPolarity(uint8_t phase, uint16_t rawSample);
void confirmPolarity(uint8_t phase);

void updatePhysicalLoadStates();

void printParamsForSelectedOutputMode();
#endif
