// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License

#pragma once

template< typename T >
struct remove_cv {
  using type = T;
};
template< typename T >
struct remove_cv<const T> {
  using type = T;
};
template< typename T >
struct remove_cv<volatile T> {
  using type = T;
};
template <typename T>
struct remove_cv<const volatile T> {
  using type = T;
};
template< typename T >
using remove_cv_t = typename remove_cv<T>::type;

