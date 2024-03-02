/**
 * @file utils_temp.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions for temperature sensor(s)
 * @version 0.1
 * @date 2023-02-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef _UTILS_TEMP_H
#define _UTILS_TEMP_H

#include <Arduino.h>

#include "constants.h"

#ifdef TEMP_ENABLED
inline constexpr bool TEMP_SENSOR_PRESENT{ true }; /**< set it to 'true' if temperature sensing is needed */
#include <OneWire.h>                               // for temperature sensing
#else
inline constexpr bool TEMP_SENSOR_PRESENT{ false }; /**< set it to 'true' if temperature sensing is needed */
#endif

template< uint8_t N >
class TemperatureSensing
{
  using ScratchPad = uint8_t[9];

  struct DeviceAddress
  {
    uint8_t addr[8];
  };

public:
  constexpr TemperatureSensing(uint8_t pin, const DeviceAddress (&ref)[N])
    : sensorPin{ pin }, sensorAddrs(ref)
  {
  }

  /**
   * @brief Request temperature for all sensors
   *
   */
  void requestTemperatures()
  {
#ifdef TEMP_ENABLED
    oneWire.reset();
    oneWire.skip();
    oneWire.write(CONVERT_TEMPERATURE);
#endif
  }

  /**
   * @brief Initialize the Dallas sensors
   *
   */
  void initTemperatureSensors()
  {
#ifdef TEMP_ENABLED
    oneWire.begin(sensorPin);
    requestTemperatures();
#endif
  }

  constexpr auto get_size() const
  {
    return N;
  }

  constexpr auto get_pin() const
  {
    return sensorPin;
  }

  /**
   * @brief Read temperature of a specific device
   *
   * @param deviceAddress The address of the device
   * @return int16_t Temperature * 100
   */
  int16_t readTemperature(const uint8_t idx)
  {
    static ScratchPad buf;

#ifdef TEMP_ENABLED
    if (!oneWire.reset())
    {
      return DEVICE_DISCONNECTED_RAW;
    }
    oneWire.select(sensorAddrs[idx].addr);
    oneWire.write(READ_SCRATCHPAD);

    for (auto &buf_elem : buf)
    {
      buf_elem = oneWire.read();
    }

    if (!oneWire.reset())
    {
      return DEVICE_DISCONNECTED_RAW;
    }
    if (oneWire.crc8(buf, 8) != buf[8])
    {
      return DEVICE_DISCONNECTED_RAW;
    }

#endif

    // result is temperature x16, multiply by 6.25 to convert to temperature x100
    int16_t result = (buf[1] << 8) | buf[0];
    result = (result * 6) + (result >> 2);
    if (result <= TEMP_RANGE_LOW || result >= TEMP_RANGE_HIGH)
    {
      return OUTOFRANGE_TEMPERATURE;  // return value ('Out of range')
    }

    return result;
  }

private:
  const uint8_t sensorPin;

  const DeviceAddress sensorAddrs[N];

#ifdef TEMP_ENABLED
  static inline OneWire oneWire; /**< For temperature sensing */
#endif
};

#endif  // _UTILS_TEMP_H
