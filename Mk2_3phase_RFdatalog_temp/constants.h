/**
 * @file constants.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some constants
 * @version 0.1
 * @date 2021-10-04
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

// Dallas DS18B20 commands
inline constexpr uint8_t CONVERT_TEMPERATURE{ 0x44 };
inline constexpr uint8_t READ_SCRATCHPAD{ 0xBE };
inline constexpr uint8_t WRITESCRATCH{ 0x4E };

inline constexpr int16_t OUTOFRANGE_TEMPERATURE{ 30200 }; /**< this value (302C) is sent if the sensor reports < -55C or > +125C */
inline constexpr int16_t TEMP_RANGE_LOW{ -5500 };
inline constexpr int16_t TEMP_RANGE_HIGH{ 12500 };

inline constexpr int16_t DEVICE_DISCONNECTED_C{ -127 };
inline constexpr int16_t DEVICE_DISCONNECTED_RAW{ -7040 };

#endif  // __CONSTANTS_H__