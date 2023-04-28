
#ifndef MOVINGAVG_H
#define MOVINGAVG_H

#include <Arduino.h>

#include "type_traits.hpp"

template< typename T, uint8_t N = 10 >
class movingAvg
{
public:
  void clear()
  {
    _idx = 0;

    if constexpr (is_floating_point< T >::value)
      _sum = 0.0;
    else
      _sum = 0;

    uint8_t i{ N };
    do
    {
      --i;
      if constexpr (is_floating_point< T >::value)
        _ar[i] = 0.0;
      else
        _ar[i] = 0;
    } while (i);
  }

  void addValue(const T& _value)
  {
    _sum -= _ar[_idx];
    _ar[_idx] = _value;
    _sum += _value;
    ++_idx;

    if (_idx == N)
    {
      _idx = 0;  // faster than %
    }
  }

  void fillValue(const T& _value)
  {
    _idx = 0;
    _sum = N * _value;

    uint8_t i{ N };
    do
    {
      _ar[--i] = _value;
    } while (i);
  }

  auto getAverage() const
  {
    return _sum / N;
  }

  auto getElement(uint8_t idx) const
  {
    if (idx >= N)
    {
      if constexpr (is_floating_point< T >::value)
        return 0.0;
      else
        return 0;
    }

    return _ar[idx];
  }

  constexpr uint8_t getSize() const
  {
    return N;
  }

private:
  uint8_t _idx{ 0 };
  typename conditional< is_floating_point< T >::value, T, int32_t >::type _sum;

  T _ar[N]{};
};

#endif
