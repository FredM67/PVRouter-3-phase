/**
 * @file utils_temp.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions for temperature sensor(s)
 * @version 0.1
 * @date 2023-02-09
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef UTILS_TEMP_H
#define UTILS_TEMP_H

#include <Arduino.h>

#include "constants.h"

#ifdef TEMP_ENABLED
inline constexpr bool TEMP_SENSOR_PRESENT{ true }; /**< set it to 'true' if temperature sensing is needed */
#include <OneWire.h>                               // for temperature sensing
#else
inline constexpr bool TEMP_SENSOR_PRESENT{ false }; /**< set it to 'true' if temperature sensing is needed */
#endif

/**
 * @struct DeviceAddress
 * @brief Structure representing the address of a device.
 *
 * This structure is used to store the unique address of a device, such as a DS18B20 temperature sensor.
 * The address is an array of 8 bytes, typically represented in hexadecimal.
 */
struct DeviceAddress
{
  uint8_t addr[8]; /**< The address of the device as an array of 8 bytes. */
};

/**
 * @brief This class implements the temperature sensing feature
 * 
 * @tparam N Number of sensors, automatically deduced
 * 
 * @ingroup TemperatureSensing
 */
template< uint8_t N >
class TemperatureSensing
{
  using ScratchPad = uint8_t[9];

public:
  constexpr TemperatureSensing() = delete;

  /**
   * @brief Construct a new Temperature Sensing object
   * 
   * @param pin Pin of the temperature sensor(s)
   * @param ref The list of temperature sensor(s)
   */
  constexpr TemperatureSensing(uint8_t pin, const DeviceAddress (&ref)[N])
    : sensorPin{ pin }, sensorAddrs(ref)
  {
  }

  /**
   * @brief Request temperature for all sensors
   *
   */
  void requestTemperatures() const
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
  void initTemperatureSensors() const
  {
#ifdef TEMP_ENABLED
    oneWire.begin(sensorPin);
    requestTemperatures();
#endif
  }

  /**
   * @brief Get the number of sensors
   * 
   * @return constexpr auto 
   */
  constexpr auto get_size() const
  {
    return N;
  }

  /**
   * @brief Get the pin of the sensor(s)
   * 
   * @return constexpr auto 
   */
  constexpr auto get_pin() const
  {
    return sensorPin;
  }

  /**
   * @brief Read temperature of a specific device
   *
   * @param idx The index of the device
   * @return int16_t Temperature * 100
   */
  int16_t readTemperature(const uint8_t idx) const
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
  const uint8_t sensorPin; /**< The pin of the sensor(s) */

  const DeviceAddress sensorAddrs[N]; /**< Array of sensors */

#ifdef TEMP_ENABLED
  static inline OneWire oneWire; /**< For temperature sensing */
#endif
};

#endif  // UTILS_TEMP_H