/**
 * @file constants.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some constants
 * @version 0.1
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2021-2026
 *
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <Arduino.h>

#include "ds18b20.h"  // the DS18B20 commands, and the values of a reading that is not a temperature

inline constexpr int16_t DEVICE_DISCONNECTED_C{ -127 };

#endif  // CONSTANTS_H
