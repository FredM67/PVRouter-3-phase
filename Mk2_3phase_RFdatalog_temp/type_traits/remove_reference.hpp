// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2023, Benoit BLANCHON
// MIT License

#pragma once

// A meta-function that return the type T without the reference modifier.
template< typename T >
struct remove_reference
{
  typedef T type;
};
template< typename T >
struct remove_reference< T& >
{
  typedef T type;
};