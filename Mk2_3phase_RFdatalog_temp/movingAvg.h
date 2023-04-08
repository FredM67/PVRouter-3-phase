
#ifndef MOVINGAVG_H
#define MOVINGAVG_H

#include <Arduino.h>

#include "type_traits.hpp"

template <typename T, uint8_t N = 10>
class movingAvg
{
public:
  void clear()
  {
    _idx = 0;
    _sum = 0;
    for (int i = 0; i < N; ++i)
      _ar[i] = 0.0; // needed to keep addValue simple
  }

  void addValue(T _value)
  {
    _sum -= _ar[_idx];
    _ar[_idx] = _value;
    _sum += _ar[_idx];
    ++_idx;

    if (_idx == N)
      _idx = 0; // faster than %
  }

  void fillValue(T _value)
  {
    _idx = 0;
    _sum = N * _value;

    for (int i = 0; i < N; i++)
    {
      _ar[_idx] = _value;
    }
  }

  float getAverage() const
  {
    return _sum / N;
  }

  auto getElement(uint8_t idx) const
  {
    if (idx >= N)
      return 0;

    return _ar[idx];
  }

  constexpr uint8_t getSize()
  {
    return N;
  }

private:
  uint8_t _idx{0};
  typename conditional<is_floating_point<T>::value, T, int32_t>::type _sum;

  T _ar[N]{0};
};

#endif
