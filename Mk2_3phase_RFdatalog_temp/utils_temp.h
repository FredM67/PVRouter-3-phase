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

#include "config.h"
#include "constants.h"
#include "dualtariff.h"
#include "processing.h"

inline int16_t readTemperature(const DeviceAddress &deviceAddress);
inline void requestTemperatures();
inline void initTemperatureSensors();

#ifdef TEMP_ENABLED
#include <OneWire.h>  // for temperature sensing

inline constexpr bool TEMP_SENSOR_PRESENT{ true };

inline OneWire oneWire(tempSensorPin); /**< For temperature sensing */

/**
 * @brief Read temperature of a specific device
 *
 * @param deviceAddress The address of the device
 * @return int16_t Temperature * 100
 */
int16_t readTemperature(const DeviceAddress &deviceAddress)
{
  static ScratchPad buf;

  if (!oneWire.reset())
  {
    return DEVICE_DISCONNECTED_RAW;
  }
  oneWire.select(deviceAddress);
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

  // result is temperature x16, multiply by 6.25 to convert to temperature x100
  int16_t result = (buf[1] << 8) | buf[0];
  result = (result * 6) + (result >> 2);
  if (result <= TEMP_RANGE_LOW || result >= TEMP_RANGE_HIGH)
  {
    return OUTOFRANGE_TEMPERATURE;  // return value ('Out of range')
  }

  return result;
}

/**
 * @brief Request temperature for all sensors
 *
 */
void requestTemperatures()
{
  oneWire.reset();
  oneWire.skip();
  oneWire.write(CONVERT_TEMPERATURE);
}

/**
 * @brief Initialize the Dallas sensors
 *
 */
void initTemperatureSensors()
{
  requestTemperatures();
}
#else
inline constexpr bool TEMP_SENSOR_PRESENT{ false };
#endif  // TEMP_SENSOR_PRESENT

#endif  // _UTILS_TEMP_H
