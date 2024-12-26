// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License

#pragma once

#include "integral_constant.hpp"

// A meta-function that return the type T without the const modifier
template< typename T >
struct is_const : false_type
{
};

template< typename T >
struct is_const< const T > : true_type
{
};