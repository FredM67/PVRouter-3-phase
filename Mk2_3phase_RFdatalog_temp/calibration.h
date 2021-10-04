/**
 * @file calibration.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Calibration values definition
 * @version 0.1
 * @date 2021-10-04
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef __CALIBRATION_H__
#define __CALIBRATION_H__

#include "config.h"

//
// Calibration values
//-------------------
// Three calibration values are used in this sketch: f_powerCal, f_phaseCal and f_voltageCal.
// With most hardware, the default values are likely to work fine without
// need for change. A compact explanation of each of these values now follows:

// When calculating real power, which is what this code does, the individual
// conversion rates for voltage and current are not of importance. It is
// only the conversion rate for POWER which is important. This is the
// product of the individual conversion rates for voltage and current. It
// therefore has the units of ADC-steps squared per Watt. Most systems will
// have a power conversion rate of around 20 (ADC-steps squared per Watt).
//
// powerCal is the RECIPR0CAL of the power conversion rate. A good value
// to start with is therefore 1/20 = 0.05 (Watts per ADC-step squared)
//
inline constexpr float f_powerCal[NO_OF_PHASES]{0.05484f, 0.05469f, 0.05385f};
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
inline constexpr float f_phaseCal{1}; /**< Nominal values only */
//
// When using integer maths, calibration values that have been supplied in floating point form need to be rescaled.
inline constexpr int16_t i_phaseCal{256}; /**< to avoid the need for floating-point maths (f_phaseCal * 256) */
inline constexpr int16_t p_phaseCal{8};   /**< to speed up math (i_phaseCal = 1 << p_phaseCal) */
//
// For datalogging purposes, f_voltageCal has been added too. Because the range of ADC values is
// similar to the actual range of volts, the optimal value for this cal factor is likely to be
// close to unity.
inline constexpr float f_voltageCal[NO_OF_PHASES]{0.985f, 1.003f, 0.987f} /*{1.03f, 1.03f, 1.03f}*/; /**< compared with Fluke 77 meter */
//--------------------------------------------------------------------------------------------------

#endif // __CALIBRATION_H__