// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2023, Benoit BLANCHON
// MIT License

#pragma once

template< bool Condition, class TrueType, class FalseType >
struct conditional
{
  typedef TrueType type;
};

template< class TrueType, class FalseType >
struct conditional< false, TrueType, FalseType >
{
  typedef FalseType type;
};
