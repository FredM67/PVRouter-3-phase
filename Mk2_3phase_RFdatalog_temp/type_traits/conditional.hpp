// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License

#pragma once

template< bool Condition, class TrueType, class FalseType >
struct conditional {
  using type = TrueType;
};

template< class TrueType, class FalseType >
struct conditional<false, TrueType, FalseType> {
  using type = FalseType;
};

template <bool Condition, class TrueType, class FalseType>
using conditional_t =
    typename conditional<Condition, TrueType, FalseType>::type;

