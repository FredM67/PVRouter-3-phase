/**
 * @file main.h
 * @author your name (you@domain.com)
 * @brief Header file for the main application.
 *
 * This file contains declarations for core functions used in the main application.
 * It includes inline functions for power and voltage updates, temperature processing,
 * and load priority management.
 *
 * @version 0.1
 * @date 2025-04-04
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>

#if !defined(__DOXYGEN__)
inline void updatePowerAndVoltageData() __attribute__((always_inline));
inline void processTemperatureData() __attribute__((always_inline));
inline void handlePerSecondTasks() __attribute__((always_inline));
inline bool proceedLoadPrioritiesAndOverriding(const int16_t& currentTemperature_x100) __attribute__((always_inline));
inline bool proceedLoadPrioritiesAndOverridingDualTariff(const int16_t& currentTemperature_x100) __attribute__((always_inline));
inline void sendResults(bool bOffPeak) __attribute__((always_inline));
#endif

#endif /* MAIN_H */
