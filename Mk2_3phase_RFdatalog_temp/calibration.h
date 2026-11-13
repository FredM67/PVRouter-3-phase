/**
 * @file calibration.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Calibration values definition
 * @version 0.1
 * @date 2026-11-13
 *
 * @copyright Copyright (c) 2023-2026
 *
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "config.h"

//
// Calibration values
//-------------------
// Each board has its own set below: the router uses the one of PCB_VERSION (config.h).
// f_powerCal converts the products of voltage and current samples into watts, for the power the
// router diverts. f_voltageCal converts the voltage samples into volts, for the voltage shown in
// the datalog only. To calibrate, compare with a reference meter:
//   new value = old value x reference reading / router reading
//
// ---- Old board (PcbVersion::OLD) ----
//
// When calculating real power, which is what this code does, the individual
// conversion rates for voltage and current are not of importance. It is
// only the conversion rate for POWER which is important. This is the
// product of the individual conversion rates for voltage and current. It
// therefore has the units of ADC-steps squared per Watt. Most systems will
// have a power conversion rate of around 20 (ADC-steps squared per Watt).
//
// powerCal is the RECIPROCAL of the power conversion rate. A good value
// to start with is therefore 1/20 = 0.05 (Watts per ADC-step squared)
//
inline constexpr float f_powerCalOldBoard[NO_OF_PHASES]{ 0.05000F, 0.05000F, 0.05000F };
//
// For datalogging purposes, f_voltageCal has been added too. Because the range of ADC values is
// similar to the actual range of volts, the optimal value for this cal factor is likely to be
// close to unity.
inline constexpr float f_voltageCalOldBoard[NO_OF_PHASES]{ 0.8151F, 0.8184F, 0.8195F }; /**< compared with Sentron PAC 4200 */
//
// ---- New board (PcbVersion::NEW, rev. 6.0+) ----
//
// f_powerCal is given per amp of CT rating: f_powerCal = f_powerCalPerAmp x f_ctRating, so
// changing a CT only means entering its rating. The default values suit 333 mV voltage-output CTs;
// they come from the design of the board (see docs/calibration.md). With a current-output CT and a
// burden resistor R (YHDC SCT-013-000, 2000:1), f_ctRating = 666 / R: the current giving 333 mV
// across the burden.
inline constexpr float f_ctRating[NO_OF_PHASES]{ 50.0F, 50.0F, 50.0F };                      /**< rated current of each CT, in A */
inline constexpr float f_powerCalPerAmp[NO_OF_PHASES]{ 0.0029225F, 0.0029225F, 0.0029225F }; /**< f_powerCal per A of CT rating */
inline constexpr float f_voltageCalNewBoard[NO_OF_PHASES]{ 0.8952F, 0.8952F, 0.8952F };      /**< volts per step */
//
// ---- Both boards ----
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
inline constexpr float f_phaseCal{ 1 }; /**< Nominal values only */

inline constexpr float lpf_gain{ 0 }; /**< setting this to 0 disables this extra processing */
inline constexpr float alpha{ 0.002 };
//--------------------------------------------------------------------------------------------------
// Nothing to change below this line.

static_assert(NO_OF_PHASES == 3, "f_powerCal and f_voltageCal below list 3 phases");

/** Whether the router runs with the set of the new board */
inline constexpr bool NEW_BOARD_CALIBRATION{ PCB_VERSION == PcbVersion::NEW };

/** f_powerCal of each phase, as the router uses it */
inline constexpr float f_powerCal[NO_OF_PHASES]{ NEW_BOARD_CALIBRATION ? f_powerCalPerAmp[0] * f_ctRating[0] : f_powerCalOldBoard[0],
                                                 NEW_BOARD_CALIBRATION ? f_powerCalPerAmp[1] * f_ctRating[1] : f_powerCalOldBoard[1],
                                                 NEW_BOARD_CALIBRATION ? f_powerCalPerAmp[2] * f_ctRating[2] : f_powerCalOldBoard[2] };

/** f_voltageCal of each phase, as the router uses it */
inline constexpr float f_voltageCal[NO_OF_PHASES]{ NEW_BOARD_CALIBRATION ? f_voltageCalNewBoard[0] : f_voltageCalOldBoard[0],
                                                   NEW_BOARD_CALIBRATION ? f_voltageCalNewBoard[1] : f_voltageCalOldBoard[1],
                                                   NEW_BOARD_CALIBRATION ? f_voltageCalNewBoard[2] : f_voltageCalOldBoard[2] };

#endif  // CALIBRATION_H
