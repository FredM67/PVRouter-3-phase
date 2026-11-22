/**
 * @file adc_chain.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The six ADC conversions of the 3-phase board, and the ADMUX pipeline of the free-running ADC
 * @version 1.0
 * @date 2026-11-22
 *
 * @copyright Copyright (c) 2026-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details The ADC converts six channels in a fixed chain, V1, I1, V2, I2, V3, I3, each about
 *          every 104 us (13 ADC clocks at F_CPU / 128). The analog input of a conversion is its
 *          place in the chain: A0 to A5.
 *
 *          The ADC runs free: when the interrupt of a finished conversion runs, the next
 *          conversion has already started with the previous ADMUX. The ADMUX written now
 *          applies to the conversion after next, so after the conversion of a channel the
 *          interrupt writes the channel two steps ahead in the chain.
 *
 *          Pure C++, no Arduino: the native tests include it. The ADMUX bit numbers are those of
 *          the ATmega328P (processing.cpp checks them against avr/io.h).
 */

#ifndef ADC_CHAIN_H
#define ADC_CHAIN_H

#include <stdint.h>

#include "config_types.h"  // PcbVersion

namespace AdcChain
{
/** The six conversions, in the order of the chain: a V then an I, for each phase */
enum class Channel : uint8_t
{
  V1, /**< voltage of phase 1 */
  I1, /**< current of phase 1 */
  V2, /**< voltage of phase 2 */
  I2, /**< current of phase 2 */
  V3, /**< voltage of phase 3 */
  I3  /**< current of phase 3 */
};

inline constexpr uint8_t NO_OF_CONVERSIONS{ 6 }; /**< conversions in the chain: a V and an I per phase */

// ADMUX bit numbers of the ATmega328P, as plain constants (avr/io.h defines REFS1... as macros)
inline constexpr uint8_t BIT_REFS1{ 7 }; /**< REFS1: with REFS0, the internal 1.1 V reference */
inline constexpr uint8_t BIT_REFS0{ 6 }; /**< REFS0: alone, the AVcc reference */
inline constexpr uint8_t BIT_ADLAR{ 5 }; /**< ADLAR: left-adjusted result */

/**
 * @brief The analog input of a channel (A0 = 0).
 *
 * @param ch The channel.
 * @return V1 0, I1 1, V2 2, I2 3, V3 4, I3 5: the place in the chain.
 */
constexpr uint8_t pinOf(const Channel ch)
{
  return static_cast< uint8_t >(ch);
}

/**
 * @brief The reference bits of ADMUX on a board.
 *
 * @param board The board.
 * @return AVcc (REFS0) on the old board, the internal 1.1 V (REFS1 and REFS0) on NEW.
 */
constexpr uint8_t refBits(const PcbVersion board)
{
  return board == PcbVersion::NEW ? static_cast< uint8_t >((1U << BIT_REFS1) | (1U << BIT_REFS0))
                                  : static_cast< uint8_t >(1U << BIT_REFS0);
}

/**
 * @brief The ADMUX value that converts a channel: reference, left-adjusted result, analog input.
 *
 * @param board The board.
 * @param ch The channel.
 * @return The ADMUX value.
 */
constexpr uint8_t admuxOf(const PcbVersion board, const Channel ch)
{
  return static_cast< uint8_t >(refBits(board) | (1U << BIT_ADLAR) | pinOf(ch));
}

/**
 * @brief The channel two steps after another one in the chain V1, I1, V2, I2, V3, I3.
 *
 * @param ch The channel.
 * @return The channel after next: V1 gives V2... V3 gives V1, I3 gives I1.
 */
constexpr Channel channelAfterNext(const Channel ch)
{
  return static_cast< Channel >((static_cast< uint8_t >(ch) + 2) % NO_OF_CONVERSIONS);
}

/**
 * @brief The ADMUX value the interrupt writes after the conversion of a channel.
 *
 * @details The conversion of the next channel has already started: the value written applies
 *          to the conversion after next, which is the channel after next.
 *
 * @param board The board.
 * @param converted The channel just converted.
 * @return The ADMUX value to write.
 */
constexpr uint8_t admuxWrittenAfter(const PcbVersion board, const Channel converted)
{
  return admuxOf(board, channelAfterNext(converted));
}

/**
 * @brief The channel round @p round converts, once the pipeline is full.
 *
 * @param round The round (0, 1, 2...), from the first conversion of V1.
 * @return V1, I1, V2, I2, V3, I3, V1...
 */
constexpr Channel channelConverted(const uint8_t round)
{
  return static_cast< Channel >(round % NO_OF_CONVERSIONS);
}
}  // namespace AdcChain

#endif  // ADC_CHAIN_H
