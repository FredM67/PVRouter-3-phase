/**
 * @file calibration.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Calibration values definition
 * @version 0.1
 * @date 2026-11-08
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
// f_voltageCal converts the voltage samples into volts, f_powerCal converts the products of
// voltage and current samples into watts. A compact explanation of each value follows.
//
// f_voltageCal: volts per ADC step. Theory gives about 0.895 for the new board (rev. 6.0+,
// 1.1 V reference) and about 0.82 for the old board (5 V reference). The voltage reference of
// each ATmega328P differs by a few percent, so compare with a reference meter and correct:
//   new f_voltageCal = old f_voltageCal x reference voltage / router voltage
inline constexpr float f_voltageCal[NO_OF_PHASES]{ 0.8952F, 0.8952F, 0.8952F }; /**< theory for the new board, to be calibrated */
//
// f_powerCal: watts per (voltage ADC step x current ADC step), that is, volts per step x amps
// per step.
//
// New board (PcbVersion::NEW), voltage-output CTs (333 mV at their rated current):
// f_powerCal is calculated from f_voltageCal and the rating of each CT. Calibrate the voltage
// first, since f_powerCal follows it. Then correct the power with f_currentCal:
//   new f_currentCal = old f_currentCal x reference power / router power
inline constexpr float f_ctRating[NO_OF_PHASES]{ 50.0F, 50.0F, 50.0F };         /**< rated current of each CT, in A */
inline constexpr float f_currentCal[NO_OF_PHASES]{ 1.0000F, 1.0000F, 1.0000F }; /**< correction of each current channel, 1 = theory */
//
// Old board (PcbVersion::OLD): f_powerCal is set directly. A good value to start with is 0.05.
//   new value = old value x reference power / router power
inline constexpr float f_powerCalOldBoard[NO_OF_PHASES]{ 0.05000F, 0.05000F, 0.05000F };
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

/**
 * @brief Current channel of the new board, for a voltage-output CT.
 *
 * The CT feeds the ADC through C12 (10 uF) into the VREF/2 bias network (10k / 10k / 100 nF),
 * which passes 98.81 % of the CT voltage at 50 Hz (98.88 % at 60 Hz), the CT output impedance
 * being negligible against 5 kOhm. Board-to-board differences (internal reference, CT ratio)
 * go into f_currentCal.
 */
namespace CurrentChannel
{
inline constexpr float ADC_REFERENCE_V{ 1.1F };  /**< internal reference, nominal */
inline constexpr float ADC_STEPS{ 1024.0F };     /**< 10-bit ADC */
inline constexpr float CT_OUTPUT_V{ 0.333F };    /**< CT output at its rated current, RMS */
inline constexpr float BIAS_COUPLING{ 0.9881F }; /**< share of the CT voltage reaching the ADC */

/** Amps per current ADC step for a phase */
constexpr float ampsPerStep(const uint8_t phase)
{
  return f_ctRating[phase] / CT_OUTPUT_V * ADC_REFERENCE_V / ADC_STEPS / BIAS_COUPLING * f_currentCal[phase];
}

/** f_powerCal of a phase, for the board in use */
constexpr float powerCal(const uint8_t phase)
{
  return PCB_VERSION == PcbVersion::NEW ? f_voltageCal[phase] * ampsPerStep(phase) : f_powerCalOldBoard[phase];
}
}  // namespace CurrentChannel

static_assert(NO_OF_PHASES == 3, "f_powerCal below lists 3 phases");

inline constexpr float f_powerCal[NO_OF_PHASES]{ CurrentChannel::powerCal(0), CurrentChannel::powerCal(1), CurrentChannel::powerCal(2) };

#endif  // CALIBRATION_H
