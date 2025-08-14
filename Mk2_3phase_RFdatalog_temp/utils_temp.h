/**
 * @file utils_temp.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Provides utilities for managing temperature sensors.
 *
 * This file defines the `TemperatureSensing` class, which handles temperature sensing
 * using DS18B20 sensors. It includes functions for initializing sensors, reading temperatures,
 * and managing sensor addresses.
 *
 * @details
 * - **Sensor Initialization**: Initializes the OneWire bus and requests temperature readings.
 * - **Temperature Reading**: Reads and validates temperature data from sensors.
 * - **Error Handling**: Handles disconnected or out-of-range sensors.
 *
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
#include "config.h"

#if TEMP_SENSOR_PRESENT
#include <OneWire.h>  // for temperature sensing
#endif

/**
 * @struct DeviceAddress
 * @brief Structure representing the address of a device.
 *
 * This structure is used to store the unique address of a device, such as a DS18B20 temperature sensor.
 * The address is an array of 8 bytes, typically represented in hexadecimal.
 *
 * @details
 * - Each byte in the array represents a part of the unique address of the device.
 * - This structure is used to identify and communicate with specific devices on the OneWire bus.
 *
 * @ingroup TemperatureSensing
 */
struct DeviceAddress
{
  uint8_t addr[8]{}; /**< The address of the device as an array of 8 bytes. */
};

// Mock class for OneWire
/// @cond DOXYGEN_EXCLUDE
class MockOneWire
{
public:
  // Mock constructor
  MockOneWire() = default;

  void begin(uint8_t) {}

  bool reset()
  {
    return true;
  }

  void skip() {}

  void select(const uint8_t*) {}

  void write(uint8_t) {}

  uint8_t read()
  {
    return 0x00;
  }

  uint8_t crc8(const uint8_t*, uint8_t)
  {
    return 0x00;
  }
};
/// @endcond

// Include the real OneWire library if needed
#if TEMP_SENSOR_PRESENT
#include <OneWire.h>          // for temperature sensing
using OneWireType = OneWire;  // Use the real implementation
#else
using OneWireType = MockOneWire;  // Use the mock implementation
#endif

/**
 * @class TemperatureSensing
 * @brief Implements temperature sensing functionality for multiple sensors.
 *
 * The `TemperatureSensing` class manages temperature sensors connected via the OneWire bus.
 * It supports initialization, temperature reading, and error handling for multiple sensors.
 *
 * @tparam N The number of sensors connected to the system.
 *
 * @details
 * - **Initialization**: Initializes the OneWire bus and requests temperature readings.
 * - **Temperature Reading**: Reads and validates temperature data from individual sensors.
 * - **Error Handling**: Handles disconnected or out-of-range sensors.
 * - **Compile-Time Configuration**: Uses `TEMP_ENABLED` to include or exclude temperature sensing features.
 *
 * @ingroup TemperatureSensing
 */
template< uint8_t N >
class TemperatureSensing
{
  /**
   * @typedef ScratchPad
   * @brief Represents a buffer for storing sensor data.
   *
   * The `ScratchPad` type alias is used to define a buffer of 9 bytes, which is
   * typically used to store data read from a DS18B20 temperature sensor's scratchpad memory.
   *
   * @details
   * - The scratchpad memory contains temperature data, configuration settings, and a CRC byte.
   * - This buffer is used during temperature reading and validation processes.
   *
   * @ingroup TemperatureSensing
   */
  using ScratchPad = uint8_t[9];

public:
  constexpr TemperatureSensing() = delete;

  /**
   * @brief Construct a new Temperature Sensing object.
   *
   * This constructor initializes the `TemperatureSensing` object with the specified pin
   * and a list of device addresses for the connected temperature sensors.
   *
   * @tparam N The number of sensors connected to the system.
   * @param pin The pin number where the temperature sensors are connected.
   * @param ref A reference to an array of `DeviceAddress` objects representing the addresses of the sensors.
   */
  constexpr TemperatureSensing(uint8_t pin, const DeviceAddress (&ref)[N])
    : sensorPin{ pin }, sensorAddrs(ref)
  {
  }

  /**
   * @brief Request temperature conversion for all sensors.
   *
   * This method sends a command to all sensors on the OneWire bus to start temperature conversion.
   * It ensures that all connected sensors begin measuring their respective temperatures.
   *
   * @details
   * - The method uses the OneWire protocol to communicate with all sensors on the bus.
   * - It sends a `CONVERT_TEMPERATURE` command to initiate temperature conversion.
   * - This method does not block; the actual temperature reading must be performed later.
   */
  void requestTemperatures() const
  {
    if constexpr (TEMP_SENSOR_PRESENT)
    {
      oneWire.reset();
      oneWire.skip();
      oneWire.write(CONVERT_TEMPERATURE);
    }
  }

  /**
   * @brief Initialize the Dallas temperature sensors.
   *
   * This method initializes the OneWire bus and sends a request to all connected sensors
   * to start temperature conversion. It ensures that the sensors are ready for temperature
   * readings.
   *
   * @details
   * - Initializes the OneWire bus using the specified sensor pin.
   * - Sends a `CONVERT_TEMPERATURE` command to all sensors on the bus.
   * - This method should be called during system initialization to prepare the sensors.
   */
  void initTemperatureSensors() const
  {
    if constexpr (TEMP_SENSOR_PRESENT)
    {
      oneWire.begin(sensorPin);
      requestTemperatures();
    }
  }

  /**
   * @brief Get the number of sensors.
   * 
   * This method returns the number of temperature sensors connected to the system.
   * It provides a compile-time constant value representing the total number of sensors.
   *
   * @return constexpr auto The number of sensors.
   */
  constexpr auto get_size() const
  {
    return N;
  }

  /**
   * @brief Get the pin of the sensor(s).
   *
   * This method returns the pin number where the temperature sensors are connected.
   *
   * @return constexpr auto The pin number.
   */
  constexpr auto get_pin() const
  {
    return sensorPin;
  }

  /**
   * @brief Reads the temperature of a specific sensor.
   *
   * This method reads the temperature data from a specific sensor connected to the OneWire bus.
   * It validates the data using CRC and ensures the temperature is within the acceptable range.
   *
   * @param idx The index of the sensor to read.
   * @return int16_t The temperature in hundredths of a degree Celsius (e.g., 2500 = 25.00°C).
   *         Returns `DEVICE_DISCONNECTED_RAW` if the sensor is disconnected or CRC validation fails.
   *         Returns `OUTOFRANGE_TEMPERATURE` if the temperature is out of the defined range.
   *
   * @details
   * - Communicates with the sensor using the OneWire protocol.
   * - Reads the scratchpad memory of the sensor to retrieve temperature data.
   * - Validates the data using CRC and checks for out-of-range values.
   */
  [[nodiscard]] int16_t readTemperature(const uint8_t idx) const
  {
    static ScratchPad buf;

    if constexpr (TEMP_SENSOR_PRESENT)
    {
      if (!oneWire.reset())
      {
        return DEVICE_DISCONNECTED_RAW;
      }
      oneWire.select(sensorAddrs[idx].addr);
      oneWire.write(READ_SCRATCHPAD);

      for (auto& buf_elem : buf)
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

private:
  const uint8_t sensorPin{ unused_pin }; /**< The pin of the sensor(s) */

  const DeviceAddress sensorAddrs[N]; /**< Array of sensors */

  static inline OneWireType oneWire; /**< For temperature sensing */
};

#endif /* UTILS_TEMP_H */
