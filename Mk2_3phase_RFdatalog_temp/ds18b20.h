/**
 * @file ds18b20.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The checks of a DS18B20 reading: CRC, conversion to hundredths of °C, range, power-on value
 * @version 0.1
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2026-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details The bus I/O stays in utils_temp.h. Here, what is done with the 9 bytes of the
 *          scratchpad once read, and with the reading once converted:
 *          - the CRC of the scratchpad (Dallas/Maxim CRC-8, the one of the OneWire library),
 *          - the temperature, x16 in the sensor, x100 here,
 *          - the range of the sensor (-55 to +125 °C),
 *          - the power-on value (85.00 °C), skipped after a jump,
 *          - which values are not a temperature, and so are not sent.
 *
 *          No Arduino dependency: the native tests run it (test/native/test_ds18b20).
 *
 * @ingroup TemperatureSensing
 */

#ifndef DS18B20_H
#define DS18B20_H

#include <stdint.h>

// Dallas DS18B20 commands
inline constexpr uint8_t CONVERT_TEMPERATURE{ 0x44 };
inline constexpr uint8_t READ_SCRATCHPAD{ 0xBE };
inline constexpr uint8_t WRITE_SCRATCH{ 0x4E };

inline constexpr int16_t OUTOFRANGE_TEMPERATURE{ 30200 }; /**< this value (302C) is sent if the sensor reports below -55C or above +125C */
inline constexpr int16_t TEMP_RANGE_LOW{ -5500 };
inline constexpr int16_t TEMP_RANGE_HIGH{ 12500 };

inline constexpr int16_t DEVICE_DISCONNECTED_RAW{ -7040 }; /**< no answer, or a CRC error */

namespace DS18B20
{
/** @brief The scratchpad: temperature LSB and MSB, TH, TL, configuration, 3 reserved bytes, CRC. */
using ScratchPad = uint8_t[9];

inline constexpr int16_t POWER_ON_VALUE{ 8500 }; /**< the register at power-on, before any conversion */
inline constexpr int16_t POWER_ON_JUMP{ 500 };   /**< a bigger jump to 85.00 °C is the power-on value */

/**
 * @brief The Dallas/Maxim CRC-8 (x^8 + x^5 + x^4 + 1), as OneWire::crc8.
 *
 * @details Bit by bit, as the OneWire library without its table (ONEWIRE_CRC8_TABLE 0): its
 *          32-byte table is not worth it for one scratchpad a second. The firmware's own copy, so
 *          the native tests check the CRC the firmware runs.
 *
 * @param data The bytes
 * @param len Their number
 * @return The CRC: the 9th byte of a scratchpad, the 8th of a ROM code
 */
constexpr uint8_t crc8(const uint8_t* data, uint8_t len)
{
  uint8_t crc{ 0 };
  while (len--)
  {
    uint8_t inbyte{ *data++ };
    for (uint8_t i = 8; i != 0; --i)
    {
      const bool mix{ ((crc ^ inbyte) & 0x01) != 0 };
      crc >>= 1;
      if (mix)
      {
        crc ^= 0x8C;
      }
      inbyte >>= 1;
    }
  }
  return crc;
}

/**
 * @brief The temperature of the register, in hundredths of °C.
 *
 * @details The register is the temperature x16 (1/16 °C at 12 bits), two's complement.
 *          x6.25 = x6 + x/4, rounded down (towards -infinity): 1/16 °C gives 6, -1/16 °C -7.
 *
 * @param lsb Byte 0 of the scratchpad
 * @param msb Byte 1 of the scratchpad
 */
constexpr int16_t toHundredths(uint8_t lsb, uint8_t msb)
{
  const int16_t x16{ static_cast< int16_t >((msb << 8) | lsb) };
  return static_cast< int16_t >((x16 * 6) + (x16 >> 2));
}

/**
 * @brief The reading of a scratchpad, its CRC checked.
 *
 * @param buf The 9 bytes read from the sensor
 * @return The temperature in hundredths of °C, `DEVICE_DISCONNECTED_RAW` on a CRC error,
 *         `OUTOFRANGE_TEMPERATURE` below -55.00 °C or above +125.00 °C.
 */
constexpr int16_t decode(const ScratchPad& buf)
{
  if (crc8(buf, 8) != buf[8])
  {
    return DEVICE_DISCONNECTED_RAW;
  }

  const int16_t result{ toHundredths(buf[0], buf[1]) };
  if (result < TEMP_RANGE_LOW || result > TEMP_RANGE_HIGH)
  {
    return OUTOFRANGE_TEMPERATURE;
  }
  return result;
}

/**
 * @brief The reading to keep, the power-on value skipped.
 *
 * @details A sensor that has just been powered (plugged, or a power glitch) answers 85.00 °C
 *          before its first conversion. 85.00 °C more than 5.00 °C away from the previous reading
 *          is that value, not a temperature.
 *
 * @param reading The reading of the sensor
 * @param previous The last valid reading (PowerOnFilter keeps it)
 * @return `reading`, or `DEVICE_DISCONNECTED_RAW` for the power-on value
 */
constexpr int16_t skipPowerOnValue(int16_t reading, int16_t previous)
{
  if (POWER_ON_VALUE == reading)
  {
    const int delta{ reading - previous };
    if (delta > POWER_ON_JUMP || delta < -POWER_ON_JUMP)
    {
      return DEVICE_DISCONNECTED_RAW;
    }
  }
  return reading;
}
}  // namespace DS18B20

/**
 * @brief Whether a temperature reading is valid, and so worth sending.
 *
 * @param temperature_x100 The reading, in hundredths of °C.
 */
constexpr bool isValidTemperature(int16_t temperature_x100)
{
  return (OUTOFRANGE_TEMPERATURE != temperature_x100) && (DEVICE_DISCONNECTED_RAW != temperature_x100);
}

namespace DS18B20
{
/**
 * @brief The readings of one sensor, the power-on value skipped.
 *
 * @details Compares each 85.00 °C with the last valid reading: an invalid reading in between
 *          (a CRC error) leaves it as it was. Compared with the reading kept last time, a steady
 *          85.00 °C would be dropped for good after a single CRC error. At startup, the reference
 *          is 0, so a first reading of 85.00 °C is skipped.
 */
struct PowerOnFilter
{
  int16_t lastValid{ 0 }; /**< the last valid reading, in hundredths of °C */

  /**
   * @brief The reading to keep.
   *
   * @param reading The reading of the sensor
   * @return `reading`, or `DEVICE_DISCONNECTED_RAW` for the power-on value
   */
  int16_t operator()(const int16_t reading)
  {
    const int16_t kept{ skipPowerOnValue(reading, lastValid) };
    if (isValidTemperature(kept))
    {
      lastValid = kept;
    }
    return kept;
  }
};
}  // namespace DS18B20

#endif /* DS18B20_H */
