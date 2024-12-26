// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License

#pragma once

#include <stddef.h>  // size_t

template <typename T>
struct decay {
  using type = T;
};

template <typename T>
struct decay<T&> : decay<T> {};

template <typename T>
struct decay<T&&> : decay<T> {};

template <typename T>
struct decay<T[]> : decay<T*> {};

template <typename T, size_t N>
struct decay<T[N]> : decay<T*> {};

template <typename T>
using decay_t = typename decay<T>::type;
