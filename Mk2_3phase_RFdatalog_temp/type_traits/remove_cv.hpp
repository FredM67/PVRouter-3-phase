// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2023, Benoit BLANCHON
// MIT License

#pragma once

template< typename T >
struct remove_cv
{
  typedef T type;
};
template< typename T >
struct remove_cv< const T >
{
  typedef T type;
};
template< typename T >
struct remove_cv< volatile T >
{
  typedef T type;
};
template< typename T >
struct remove_cv< const volatile T >
{
  typedef T type;
};