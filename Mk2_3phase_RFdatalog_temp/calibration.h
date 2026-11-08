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
// The router converts its ADC samples with two factors per phase: f_voltageCal (volts per step,
// for the voltage shown in the datalog only) and f_powerCal (watts per voltage step x current
// step, for the power the router diverts). How you set them depends on the board in config.h.
//
// New board (PcbVersion::NEW, rev. 6.0+), with voltage-output CTs (333 mV at their rated
// current): both factors are calculated from the design of the board. Enter:
// - f_adcReference: the internal ADC reference of your ATmega328P, measured with a multimeter on
//   TP2 (VREF). It differs from one chip to another (1.0 to 1.2 V), and the power follows its square.
// - f_ctRating: the rated current of each CT, in A.
// The corrections are optional, 1 = the calculated value. They are independent: the voltage one
// only changes the voltage shown, the power one only the power.
//   new correction = old correction x reference reading / router reading
inline constexpr float f_adcReference{ 1.100F };                                       /**< measured on TP2, in V */
inline constexpr float f_ctRating[NO_OF_PHASES]{ 50.0F, 50.0F, 50.0F };                /**< rated current of each CT, in A */
inline constexpr float f_powerCorrection[NO_OF_PHASES]{ 1.0000F, 1.0000F, 1.0000F };   /**< correction of P1, P2, P3 */
inline constexpr float f_voltageCorrection[NO_OF_PHASES]{ 1.0000F, 1.0000F, 1.0000F }; /**< correction of V1, V2, V3 */
//
// Old board (PcbVersion::OLD): both factors are set directly. A good value to start with is
// 0.05 for the power and 0.82 for the voltage.
//   new value = old value x reference reading / router reading
inline constexpr float f_powerCalOldBoard[NO_OF_PHASES]{ 0.05000F, 0.05000F, 0.05000F };
inline constexpr float f_voltageCalOldBoard[NO_OF_PHASES]{ 0.8151F, 0.8184F, 0.8195F };
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
 * @brief Input stages of the new board (rev. 6.0+), at 50 Hz (60 Hz differs by 0.06 %)
 *
 * Voltage: mains -> 6 x 20k (0.1 %) -> ZMPT101K 1:1 -> 150R burden (0.1 %), loaded through C10
 * (10 uF) by the VREF/2 bias network (10k / 10k / 100 nF): 145.73 Ohm seen by the ZMPT101K, of
 * which 98.81 % reaches the ADC. So 145.73 x 0.9881 / 120k = 1.2000 mV at the ADC per mains volt.
 *
 * Current: CT -> C12 (10 uF) -> the same bias network: 98.81 % of the CT voltage reaches the ADC,
 * the CT output impedance being negligible against 5 kOhm.
 */
namespace Calibration
{
inline constexpr float ADC_STEPS{ 1024.0F };                /**< 10-bit ADC */
inline constexpr float VOLTAGE_GAIN{ 1.2000e-3F };          /**< ADC volts per mains volt */
inline constexpr float CT_OUTPUT_V{ 0.333F };               /**< CT output at its rated current, RMS */
inline constexpr float CURRENT_COUPLING{ 0.9881F };         /**< share of the CT voltage reaching the ADC */
inline constexpr float VOLTAGE_STAGE_CORRECTION{ 1.0000F }; /**< measured difference with the calculation, the same on every board: to be confirmed on a second board */

/** Volts per voltage step, as calculated */
inline constexpr float VOLTS_PER_STEP{ f_adcReference / ADC_STEPS / VOLTAGE_GAIN * VOLTAGE_STAGE_CORRECTION };

/** Amps per current step of a phase, as calculated */
constexpr float ampsPerStep(const uint8_t phase)
{
  return f_ctRating[phase] / CT_OUTPUT_V * f_adcReference / ADC_STEPS / CURRENT_COUPLING;
}

/** f_voltageCal of a phase, for the board in use */
constexpr float voltageCal(const uint8_t phase)
{
  return PCB_VERSION == PcbVersion::NEW ? VOLTS_PER_STEP * f_voltageCorrection[phase] : f_voltageCalOldBoard[phase];
}

/** f_powerCal of a phase, for the board in use */
constexpr float powerCal(const uint8_t phase)
{
  return PCB_VERSION == PcbVersion::NEW ? VOLTS_PER_STEP * ampsPerStep(phase) * f_powerCorrection[phase] : f_powerCalOldBoard[phase];
}
}  // namespace Calibration

static_assert(NO_OF_PHASES == 3, "f_voltageCal and f_powerCal below list 3 phases");

inline constexpr float f_voltageCal[NO_OF_PHASES]{ Calibration::voltageCal(0), Calibration::voltageCal(1), Calibration::voltageCal(2) };
inline constexpr float f_powerCal[NO_OF_PHASES]{ Calibration::powerCal(0), Calibration::powerCal(1), Calibration::powerCal(2) };

#endif  // CALIBRATION_H
