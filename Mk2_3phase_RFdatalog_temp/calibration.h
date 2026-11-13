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
// f_powerCal converts the products of voltage and current samples into watts, for the power the
// router diverts. It is given per amp of CT rating: f_powerCal = f_powerCalPerAmp x f_ctRating,
// so changing a CT only means entering its rating. f_voltageCal converts the voltage samples into
// volts, for the voltage shown in the datalog only.
//
// The default values suit the new board (rev. 6.0+) with 333 mV voltage-output CTs; they come from
// its design (see docs/calibration.md). With a current-output CT and a burden resistor R (YHDC
// SCT-013-000, 2000:1), f_ctRating = 666 / R: the current giving 333 mV across the burden.
// Old board: f_ctRating = 1, f_powerCalPerAmp = your former f_powerCal, about 0.82 for f_voltageCal.
//
// To calibrate, compare with a reference meter:
//   new value = old value x reference reading / router reading
inline constexpr float f_ctRating[NO_OF_PHASES]{ 50.0F, 50.0F, 50.0F };                      /**< rated current of each CT, in A */
inline constexpr float f_powerCalPerAmp[NO_OF_PHASES]{ 0.0029225F, 0.0029225F, 0.0029225F }; /**< f_powerCal per A of CT rating */
inline constexpr float f_voltageCal[NO_OF_PHASES]{ 0.8952F, 0.8952F, 0.8952F };              /**< volts per step */
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

static_assert(NO_OF_PHASES == 3, "f_powerCal below lists 3 phases");

/** f_powerCal of each phase, as the router uses it */
inline constexpr float f_powerCal[NO_OF_PHASES]{ f_powerCalPerAmp[0] * f_ctRating[0], f_powerCalPerAmp[1] * f_ctRating[1], f_powerCalPerAmp[2] * f_ctRating[2] };

#endif  // CALIBRATION_H
