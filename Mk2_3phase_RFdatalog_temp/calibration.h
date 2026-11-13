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
// The router converts its ADC samples with two factors per phase: f_voltageCal (volts per step,
// for the voltage shown in the datalog only) and f_powerCal (watts per voltage step x current
// step, for the power the router diverts). How you set them depends on the board in config.h.
//
// New board (PcbVersion::NEW, rev. 6.0+), with voltage-output CTs (333 mV at their rated
// current): both factors are calculated from the design of the board. Enter the rated current of
// each CT in f_ctRating.
// The corrections are optional, 1 = the calculated value. The internal ADC reference differs from
// one ATmega328P to another (1.0 to 1.2 V): the three phases share it, so it never unbalances them,
// but the watts and volts shown can be off by a few percent. The corrections fix that, and are
// independent: the voltage one only changes the voltage shown, the power one only the power.
//   new correction = old correction x reference reading / router reading
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
 * @brief The factors of the new board (rev. 6.0+), calculated from its design
 *
 * Voltage: mains -> 6 x 20k (0.1 %) -> ZMPT101K 1:1 -> 150R burden (0.1 %), loaded through C10
 * (10 uF) by the VREF/2 bias network (10k / 10k / 100 nF): 145.73 Ohm seen by the ZMPT101K, of
 * which 98.81 % reaches the ADC, so 1.2000 mV at the ADC per mains volt. With the internal
 * reference at its nominal 1.1 V: 1.1 V / 1024 steps / 1.2000 mV = 0.8952 V per step.
 *
 * Current: a 333 mV voltage-output CT -> C12 (10 uF) -> the same bias network: 98.81 % of the
 * CT voltage reaches the ADC. For 1 A of CT rating: 1 A / 0.333 V x 1.1 V / 1024 / 0.9881 =
 * 3.2647 mA per step, so 0.8952 x 3.2647e-3 = 0.0029225 W per (voltage step x current step).
 *
 * At 50 Hz; 60 Hz differs by 0.06 %. The reference differs from one ATmega328P to another (1.0
 * to 1.2 V): the phases share it, so it never unbalances them, and the corrections above fix
 * the values shown.
 */
namespace Calibration
{
inline constexpr float NEW_BOARD_VOLTAGE_CAL{ 0.8952F };          /**< volts per step */
inline constexpr float NEW_BOARD_POWER_CAL_PER_AMP{ 0.0029225F }; /**< f_powerCal per A of CT rating */

/** f_voltageCal of a phase: new board, calculated and corrected, or the old board's value */
constexpr float voltageCal(const bool newBoard, const float correction, const float oldBoardValue)
{
  return newBoard ? NEW_BOARD_VOLTAGE_CAL * correction : oldBoardValue;
}

/** f_powerCal of a phase: new board, calculated from the CT rating and corrected, or the old board's value */
constexpr float powerCal(const bool newBoard, const float ctRating, const float correction, const float oldBoardValue)
{
  return newBoard ? NEW_BOARD_POWER_CAL_PER_AMP * ctRating * correction : oldBoardValue;
}

/** Whether two factors are equal to 1 ppm */
constexpr bool sameFactor(const float a, const float b)
{
  return (a - b) < 1e-6F * b && (b - a) < 1e-6F * b;
}

// The calculation, checked at every build
static_assert(sameFactor(voltageCal(true, 1.0F, 0.8F), 0.8952F), "new board: 0.8952 V per step without correction");
static_assert(sameFactor(powerCal(true, 50.0F, 1.0F, 0.05F), 0.146125F), "new board: 0.146125 with a 50 A CT");
static_assert(sameFactor(powerCal(true, 100.0F, 1.0F, 0.05F), 2 * powerCal(true, 50.0F, 1.0F, 0.05F)), "new board: f_powerCal follows the CT rating");
static_assert(sameFactor(powerCal(true, 50.0F, 1.02F, 0.05F), 1.02F * powerCal(true, 50.0F, 1.0F, 0.05F)), "new board: the power correction scales the power");
static_assert(sameFactor(voltageCal(true, 1.02F, 0.8F), 1.02F * NEW_BOARD_VOLTAGE_CAL), "new board: the voltage correction scales the voltage");
static_assert(sameFactor(voltageCal(false, 1.02F, 0.8151F), 0.8151F) && sameFactor(powerCal(false, 50.0F, 1.02F, 0.05F), 0.05F), "old board: its values, untouched");
}  // namespace Calibration

static_assert(NO_OF_PHASES == 3, "f_voltageCal and f_powerCal below list 3 phases");

inline constexpr bool NEW_BOARD{ PCB_VERSION == PcbVersion::NEW };

inline constexpr float f_voltageCal[NO_OF_PHASES]{ Calibration::voltageCal(NEW_BOARD, f_voltageCorrection[0], f_voltageCalOldBoard[0]),
                                                   Calibration::voltageCal(NEW_BOARD, f_voltageCorrection[1], f_voltageCalOldBoard[1]),
                                                   Calibration::voltageCal(NEW_BOARD, f_voltageCorrection[2], f_voltageCalOldBoard[2]) };
inline constexpr float f_powerCal[NO_OF_PHASES]{ Calibration::powerCal(NEW_BOARD, f_ctRating[0], f_powerCorrection[0], f_powerCalOldBoard[0]),
                                                 Calibration::powerCal(NEW_BOARD, f_ctRating[1], f_powerCorrection[1], f_powerCalOldBoard[1]),
                                                 Calibration::powerCal(NEW_BOARD, f_ctRating[2], f_powerCorrection[2], f_powerCalOldBoard[2]) };

#endif  // CALIBRATION_H
