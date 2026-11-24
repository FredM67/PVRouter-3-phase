/**
 * @file sample_sets.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The counter of the sample sets of a datalog period, as wide as the period needs
 * @version 1.0
 * @date 2026-11-24
 *
 * @copyright Copyright (c) 2026-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details A sample set is one conversion of each channel of the ADC chain (adc_chain.h). The
 *          number of sets of a datalog period is the divisor of the datalog power and voltage.
 *          Its counter is chosen at compile time, as the V^2 scaling: 16 bits while the largest
 *          count fits, 32 bits above, so that the default 5 s period keeps a 16-bit counter
 *          (config_derived.h says up to which period).
 *
 *          Pure C++, no Arduino: the native tests include it.
 */

#ifndef SAMPLE_SETS_H
#define SAMPLE_SETS_H

#include <stdint.h>

#include "type_traits/conditional.hpp"

namespace SampleSets
{
/**
 * @brief The largest number of sample sets in a datalog period.
 *
 * @param mainsCycles The mains cycles of the period (DATALOG_PERIOD_IN_MAINS_CYCLES).
 * @param maxPerCycle The most sample sets in one mains cycle (N_MAX: expected + margin).
 * @return The largest count.
 */
constexpr uint32_t maxPerDatalog(const uint16_t mainsCycles, const uint8_t maxPerCycle)
{
  return static_cast< uint32_t >(mainsCycles) * maxPerCycle;
}

/** @brief The counter for a largest count: uint16_t when it fits, uint32_t above. */
template< uint32_t MaxCount >
using Counter = typename conditional< (MaxCount <= UINT16_MAX), uint16_t, uint32_t >::type;
}  // namespace SampleSets

#endif  // SAMPLE_SETS_H
