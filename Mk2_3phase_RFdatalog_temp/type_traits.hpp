/**
 * @file type_traits.hpp
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some useful but missing stl functions templates
 * @version 0.1
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2023-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License

#pragma once

#include <stdint.h>

#include "type_traits/conditional.hpp"
#include "type_traits/decay.hpp"
#include "type_traits/enable_if.hpp"
#include "type_traits/function_traits.hpp"
#include "type_traits/index_sequence.hpp"
#include "type_traits/integral_constant.hpp"
#include "type_traits/is_array.hpp"
#include "type_traits/is_base_of.hpp"
#include "type_traits/is_class.hpp"
#include "type_traits/is_const.hpp"
#include "type_traits/is_convertible.hpp"
#include "type_traits/is_enum.hpp"
#include "type_traits/is_floating_point.hpp"
#include "type_traits/is_integral.hpp"
#include "type_traits/is_pointer.hpp"
#include "type_traits/is_same.hpp"
#include "type_traits/is_signed.hpp"
#include "type_traits/is_unsigned.hpp"
#include "type_traits/make_unsigned.hpp"
#include "type_traits/remove_const.hpp"
#include "type_traits/remove_reference.hpp"
#include "type_traits/void_t.hpp"

/**
 * @brief A compile-time number of minutes, as an integral_constant.
 *
 * @details One syntax for the durations of config.h: the sliding average of the relays
 *          (RelayEngine takes it as a template parameter) and the scheduled boosts (Minutes).
 *          Usage: MINUTES(RELAY_FILTER_DELAY), MINUTES(90).
 */
#define MINUTES(value) \
  integral_constant< uint16_t, (value) > {}

/**
 * @brief A compile-time number of hours, as MINUTES().
 *
 * @details Usage: HOURS(2). Too many hours fail to compile instead of wrapping around: the
 *          product overflows the 16-bit int of the AVR (from HOURS(547) on), and is a narrowing
 *          conversion to uint16_t on a native build (beyond 65535 minutes).
 */
#define HOURS(value) MINUTES((value) * 60)
