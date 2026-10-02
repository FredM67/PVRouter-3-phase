/**
 * @file debug.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The text output: informational messages (info) and debug messages (debug)
 * @version 0.1
 * @date 2026-10-02
 *
 * @copyright Copyright (c) 2023-2026
 *
 * @details Text is printed only when the serial output is human-readable: in the IoT and JSON
 *          modes, the serial port carries data for another device (mk2Wifi...), which must not
 *          receive anything else.
 *          - info(), infoln(): startup configuration, status changes...
 *          - debug(), debugln(): debug messages, only with ENABLE_DEBUG as well.
 *
 *          The arguments are those of Serial.print() / Serial.println(). A datalog output still
 *          in progress is written first, so a message never lands in the middle of a datalog
 *          line or telemetry frame (see serial_output.h). When the output is off, the calls
 *          compile to nothing, provided their arguments have no side effects.
 *
 *          The templates are always inlined: out of line, GCC keeps a copy per argument type,
 *          about 20 bytes more in all.
 *
 * @note Needs SERIAL_OUTPUT_TYPE and ENABLE_DEBUG: include it after them (see config.h).
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

#include "serial_output.h"

/**
 * @brief Whether text can be printed: only in human-readable output mode.
 */
inline constexpr bool TEXT_OUTPUT_ENABLED{ SERIAL_OUTPUT_TYPE == SerialOutputType::HumanReadable };

/**
 * @brief Print an informational message, as Serial.print() does.
 */
template< typename... Args >
__attribute__((always_inline)) inline void info(const Args&... args)
{
  if constexpr (TEXT_OUTPUT_ENABLED)
  {
    SerialOutput::complete();
    Serial.print(args...);
  }
}

/**
 * @brief Print an informational message and a line end, as Serial.println() does.
 */
template< typename... Args >
__attribute__((always_inline)) inline void infoln(const Args&... args)
{
  if constexpr (TEXT_OUTPUT_ENABLED)
  {
    SerialOutput::complete();
    Serial.println(args...);
  }
}

/**
 * @brief Print a value given as an integer with a fixed number of decimals, and a line end.
 *
 * @details For float constants converted at compile time (SerialOutput::toDecimals()):
 *          printed without the float code of Print.
 */
inline void infolnDecimals(const int32_t value, const uint8_t decimals)
{
  if constexpr (TEXT_OUTPUT_ENABLED)
  {
    SerialOutput::complete();
    SerialOutput::printDecimals(Serial, value, decimals);
    Serial.println();
  }
}

/**
 * @brief Same as infolnDecimals(), with the value read from PROGMEM.
 *
 * @details The value is read here, not by the caller: a PROGMEM read (pgm_read_dword) is
 *          volatile inline assembly, which would stay in the firmware even with the text
 *          output off.
 */
inline void infolnDecimals_P(const int32_t* value, const uint8_t decimals)
{
  if constexpr (TEXT_OUTPUT_ENABLED)
  {
    infolnDecimals(static_cast< int32_t >(pgm_read_dword(value)), decimals);
  }
}

/**
 * @brief Print a debug message, as Serial.print() does (only with ENABLE_DEBUG).
 */
template< typename... Args >
__attribute__((always_inline)) inline void debug(const Args&... args)
{
  if constexpr (ENABLE_DEBUG)
  {
    info(args...);
  }
}

/**
 * @brief Print a debug message and a line end, as Serial.println() does (only with ENABLE_DEBUG).
 */
template< typename... Args >
__attribute__((always_inline)) inline void debugln(const Args&... args)
{
  if constexpr (ENABLE_DEBUG)
  {
    infoln(args...);
  }
}

#endif  // DEBUG_H
