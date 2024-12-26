// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License

#pragma once

template <typename Sig>
struct function_traits;

template <typename ReturnType, typename Arg1>
struct function_traits<ReturnType (*)(Arg1)> {
  using return_type = ReturnType;
  using arg1_type = Arg1;
};

template <typename ReturnType, typename Arg1, typename Arg2>
struct function_traits<ReturnType (*)(Arg1, Arg2)> {
  using return_type = ReturnType;
  using arg1_type = Arg1;
  using arg2_type = Arg2;
};

