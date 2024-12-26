// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License

#pragma once

#include "integral_constant.hpp"

template< typename T >
struct is_pointer : false_type {};

template< typename T >
struct is_pointer<T*> : true_type {};

