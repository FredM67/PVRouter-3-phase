/**
 * @file utils_temp.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Provides utilities for managing temperature sensors.
 *
 * This file defines the `TempSensors` class, which handles temperature sensing
 * using DS18B20 sensors, from the `TemperatureSensing` of config.h. It includes functions for
 * initializing sensors, reading temperatures,
 * and managing sensor addresses.
 *
 * @details
 * - **Sensor Initialization**: Initializes the OneWire bus and requests temperature readings.
 * - **Temperature Reading**: Reads and validates temperature data from sensors.
 * - **Error Handling**: Handles disconnected or out-of-range sensors.
 *
 * @version 0.1
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2024-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef UTILS_TEMP_H
#define UTILS_TEMP_H

#include <Arduino.h>

#include "constants.h"
#include "config.h"
#include "debug.h"
#include "ds18b20.h"
#include "onewire_search.h"

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

  void write_bit(uint8_t) {}

  uint8_t read_bit()
  {
    return 0x01;
  }
};
/// @endcond

// The real OneWire bus only with temperature sensing. TEMP_SENSOR_PRESENT is a constexpr, not a
// macro: the choice must be made in C++, an #if on it would always pick the mock.
#include <OneWire.h>  // for temperature sensing
using OneWireType = conditional_t< TEMP_SENSOR_PRESENT, OneWire, MockOneWire >;

/**
 * @class TempSensors
 * @brief Implements temperature sensing functionality for multiple sensors.
 *
 * The `TempSensors` class reads the sensors of config.h (its base, `TemperatureSensing`). It
 * manages temperature sensors connected via the OneWire bus.
 * It supports initialization, temperature reading, and error handling for multiple sensors.
 *
 * @tparam N The number of sensors connected to the system.
 *
 * @details
 * - **Initialization**: Initializes the OneWire bus and requests temperature readings.
 * - **Temperature Reading**: Reads and validates temperature data from individual sensors.
 * - **Error Handling**: Handles disconnected or out-of-range sensors.
 * - **Compile-Time Configuration**: `TEMP_SENSOR_PRESENT` selects the real OneWire bus or a mock.
 *
 * @ingroup TemperatureSensing
 */
template< uint8_t N >
class TempSensors : public TemperatureSensing< N >
{
  using TemperatureSensing< N >::sensorPin;
  using TemperatureSensing< N >::sensorAddrs;

public:
  /**
   * @brief The sensors of config.h.
   *
   * @param sensors The pin of the bus and the address of each sensor, as in config.h.
   */
  explicit constexpr TempSensors(const TemperatureSensing< N >& sensors)
    : TemperatureSensing< N >(sensors)
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
   * @brief Lists the sensors on the bus, with their ROM codes as config.h takes them.
   *
   * @details Calibration mode only (CALIBRATION_MODE), with temperature sensing: to find the codes
   *          to put in config.h, or to check them. Each code says whether config.h has it. The
   *          OneWire library is built without its search (-DONEWIRE_SEARCH=0, platformio.ini):
   *          RomSearch (onewire_search.h) does it here, with its state on the stack.
   */
  void printSensorsOnBus() const
  {
    if constexpr (TEMP_SENSOR_PRESENT && CALIBRATION_MODE)
    {
      oneWire.begin(sensorPin);

      info(F("Temperature sensors found on pin "));
      info(sensorPin);
      infoln(F(":"));

      RomSearch< OneWireType > search{ oneWire };
      DeviceAddress found;
      uint8_t count{ 0 };
      // bounded: noise on a long or badly terminated bus could otherwise keep the search going
      constexpr uint8_t MAX_LISTED{ 16 };
      static_assert(MAX_LISTED == 16, "update the message below");
      while (search.next(found.addr))
      {
        if (MAX_LISTED == count)
        {
          // a 17th device: exactly 16 sensors give no warning
          infoln(F("\t... more than 16: noise on the bus? Check the wiring"));
          break;
        }
        ++count;
        info(F("\t{ "));
        for (uint8_t i = 0; i != 8; ++i)
        {
          info(F("0x"));
          if (found.addr[i] < 0x10)
          {
            info('0');
          }
          info(found.addr[i], HEX);
          if (i != 7)
          {
            info(F(", "));
          }
        }
        info(F(" }"));

        if (DS18B20::crc8(found.addr, 7) != found.addr[7])
        {
          infoln(F("  CRC error: check the wiring"));
        }
        else
        {
          infoln(isConfigured(found) ? F("  in config.h") : F("  not in config.h"));
        }
      }
      if (0 == count)
      {
        infoln(F("\tnone: check the wiring and the pull-up resistor"));
      }
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
   * @brief Read temperature from a specific sensor (compile-time bounds checking).
   *
   * This template version provides compile-time bounds checking for constant sensor indices.
   * Use this version when the sensor index is known at compile time for better safety.
   *
   * @tparam IDX The sensor index (must be < N)
   * @return int16_t The temperature in hundredths of a degree Celsius (e.g., 2500 = 25.00°C).
   *         Returns `DEVICE_DISCONNECTED_RAW` if the sensor is disconnected or CRC validation fails.
   *         Returns `OUTOFRANGE_TEMPERATURE` if the temperature is out of the defined range.
   */
  template< uint8_t IDX >
  [[nodiscard]] int16_t readTemperature() const
  {
    static_assert(IDX < N, "Sensor index out of bounds! Check your sensor configuration.");
    return readTemperature(IDX);
  }

  /**
   * @brief Reads the temperature of a specific sensor (runtime bounds checking).
   *
   * This method reads the temperature data from a specific sensor connected to the OneWire bus.
   * It validates the data using CRC and ensures the temperature is within the acceptable range.
   * Includes runtime bounds checking for safety when the index is not known at compile time.
   *
   * @param idx The index of the sensor to read.
   * @return int16_t The temperature in hundredths of a degree Celsius (e.g., 2500 = 25.00°C).
   *         Returns `DEVICE_DISCONNECTED_RAW` if the sensor is disconnected or CRC validation fails.
   *         Returns `OUTOFRANGE_TEMPERATURE` if the temperature is out of the defined range.
   *
   * @details
   * - Communicates with the sensor using the OneWire protocol.
   * - Reads the scratchpad memory of the sensor to retrieve temperature data.
   * - DS18B20::decode() (ds18b20.h) checks its CRC, converts it and checks the range.
   */
  [[nodiscard]] int16_t readTemperature(const uint8_t idx) const
  {
    if constexpr (!TEMP_SENSOR_PRESENT)
    {
      // Suppress unused parameter warning when temperature sensing is disabled
      (void)idx;
      return 0;
    }
    else
    {
      static DS18B20::ScratchPad buf;

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
      return DS18B20::decode(buf);
    }
  }

private:
  /** @brief Whether config.h lists this sensor. */
  bool isConfigured(const DeviceAddress& device) const
  {
    for (uint8_t idx = 0; idx != N; ++idx)
    {
      if (0 == memcmp(sensorAddrs[idx].addr, device.addr, sizeof(device.addr)))
      {
        return true;
      }
    }
    return false;
  }

  static inline OneWireType oneWire; /**< For temperature sensing */
};

/** @brief Deduction guide: N from the sensors of config.h. */
template< uint8_t N >
TempSensors(const TemperatureSensing< N >&) -> TempSensors< N >;

#endif /* UTILS_TEMP_H */
