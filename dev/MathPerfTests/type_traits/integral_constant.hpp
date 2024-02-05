// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2023, Benoit BLANCHON
// MIT License

#pragma once

template< typename T, T v >
struct integral_constant
{
  static const T value = v;
};

typedef integral_constant< bool, true > true_type;
typedef integral_constant< bool, false > false_type;