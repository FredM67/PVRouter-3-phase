/**
 * @file index_sequence.hpp
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief std::index_sequence, which avr-libc lacks, on 8-bit indexes
 * @version 0.1
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2026-2026
 *
 */

#pragma once

#include <stdint.h>

/** @brief The indexes I..., as a type. */
template< uint8_t... I >
struct index_sequence
{
};

/** @cond */
template< uint8_t N, uint8_t... I >
struct make_index_sequence_impl : make_index_sequence_impl< N - 1, N - 1, I... >
{
};

template< uint8_t... I >
struct make_index_sequence_impl< 0, I... >
{
  using type = index_sequence< I... >;
};
/** @endcond */

/** @brief index_sequence< 0, 1, ..., N - 1 >. */
template< uint8_t N >
using make_index_sequence = typename make_index_sequence_impl< N >::type;
