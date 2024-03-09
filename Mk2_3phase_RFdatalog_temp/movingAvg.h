/**
 * @file movingAvg.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Code for sliding-window average
 * @version 0.1
 * @date 2023-06-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef MOVINGAVG_H
#define MOVINGAVG_H

#include <Arduino.h>

#include "type_traits.hpp"

/**
 * @brief Template class for implementing a sliding average
 * 
 * @tparam T Type of values to be stored
 * @tparam DURATION_IN_MINUTES Size of main array
 * @tparam VALUES_PER_MINUTE Size of sub array
 * 
 * @note
 *    Since the Arduino RAM is tiny, we need to store a few values as possible.
 *    Suppose, you want a sliding window of 10 minutes and receive a value every 5 seconds.
 *    With one single array, 10 x 12 values would be necessary to store.
 *    Using 2 arrays, one to store incoming values over 1 minute and one to store values every minutes,
 *    we need only 12 + 10 values.
 *    How it works :
 *      - the average is calculated over 1 minute with incoming values
 *      - this sub-average is added to the main array
 *      - the average is calculated on the main array
 * 
 *    Drawback of this method: the average is updated only every minutes !
 */
template< typename T, uint8_t DURATION_IN_MINUTES = 10, uint8_t VALUES_PER_MINUTE = 10 >
class movingAvg
{
public:
  /**
   * @brief Reset everything
   * 
   */
  void clear()
  {
    _idx = 0;

    if constexpr (is_floating_point< T >::value)
    {
      _sum = 0.0F;
    }
    else
    {
      _sum = 0;
    }

    uint8_t i{ DURATION_IN_MINUTES };
    do {
      --i;
      if constexpr (is_floating_point< T >::value)
      {
        _ar[i] = 0.0F;
      }
      else
      {
        _ar[i] = 0;
      }
    } while (i);

    _clear_sub();
  }

  /**
   * @brief Add a value
   * 
   * @param _value Value to be added
   */
  void addValue(const T& _value)
  {
    _sub_sum -= _sub_ar[_sub_idx];
    _sub_ar[_sub_idx] = _value;
    _sub_sum += _value;
    ++_sub_idx;

    if (_sub_idx == VALUES_PER_MINUTE)
    {
      _sub_idx = 0;  // faster than %
      _addValue(_getAverage());
    }
  }

  void fillValue(const T& _value)
  {
    _idx = 0;
    _sum = DURATION_IN_MINUTES * _value;

    uint8_t i{ DURATION_IN_MINUTES };
    do
    {
      _ar[--i] = _value;
    } while (i);
  }

  /**
   * @brief Get the sliding average
   * 
   * @return auto The sliding average
   * 
   * @note This value is updated every minute, except for the special case of a duration of
   *   ONE minute. In this case, it is updated for each new input value.
   */
  auto getAverage() const
  {
    if constexpr (DURATION_IN_MINUTES == 1)
      return _getAverage();
    else if constexpr (DURATION_IN_MINUTES == 2)
      return _sum >> 1;
    else if constexpr (DURATION_IN_MINUTES == 4)
      return _sum >> 2;
    else if constexpr (DURATION_IN_MINUTES == 8)
      return _sum >> 3;
    else if constexpr (DURATION_IN_MINUTES == 16)
      return _sum >> 4;
    else if constexpr (DURATION_IN_MINUTES == 32)
      return _sum >> 5;
    else
      return _sum * invN;
  }

  auto getElement(uint8_t idx) const
  {
    if (idx >= DURATION_IN_MINUTES)
    {
      if constexpr (is_floating_point< T >::value)
        return 0.0F;
      else
        return (T)0;
    }

    return _ar[idx];
  }

  constexpr uint8_t getSize() const
  {
    return DURATION_IN_MINUTES;
  }

private:
  void _clear_sub()
  {
    _sub_idx = 0;

    if constexpr (is_floating_point< T >::value)
      _sub_sum = 0.0F;
    else
      _sub_sum = 0;

    uint8_t i{ VALUES_PER_MINUTE };
    do {
      --i;
      if constexpr (is_floating_point< T >::value)
        _sub_ar[i] = 0.0F;
      else
        _sub_ar[i] = 0;
    } while (i);
  }

  void _addValue(const T& _value)
  {
    _sum -= _ar[_idx];
    _ar[_idx] = _value;
    _sum += _value;
    ++_idx;

    if (_idx == DURATION_IN_MINUTES)
    {
      _idx = 0;  // faster than %
    }
  }

  auto _getAverage() const
  {
    return _sub_sum * invD;
  }

private:
  uint8_t _idx{ 0 };
  uint8_t _sub_idx{ 0 };
  typename conditional< is_floating_point< T >::value, T, int32_t >::type _sum{ 0 };
  typename conditional< is_floating_point< T >::value, T, int32_t >::type _sub_sum{ 0 };

  T _sub_ar[VALUES_PER_MINUTE]{};
  T _ar[DURATION_IN_MINUTES]{};

  static constexpr float invD{ 1.0 / VALUES_PER_MINUTE };
  static constexpr float invN{ 1.0 / DURATION_IN_MINUTES };
};

#endif
