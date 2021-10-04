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
inline constexpr uint8_t SKIP_ROM{0xcc};
inline constexpr uint8_t CONVERT_TEMPERATURE{0x44};
inline constexpr uint8_t READ_SCRATCHPAD{0xbe};
inline constexpr int16_t UNUSED_TEMPERATURE{30000};     /**< this value (300C) is sent if no sensor has ever been detected */
inline constexpr int16_t OUTOFRANGE_TEMPERATURE{30200}; /**< this value (302C) is sent if the sensor reports < -55C or > +125C */
inline constexpr int16_t BAD_TEMPERATURE{30400};        /**< this value (304C) is sent if no sensor is present or the checksum is bad (corrupted data) */
inline constexpr int16_t TEMP_RANGE_LOW{-5500};
inline constexpr int16_t TEMP_RANGE_HIGH{12500};

#endif // __CONSTANTS_H__
