/**
 * @file debug.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The text output: informational messages (info) and debug messages (debug)
 * @version 0.1
 * @date 2026-11-13
 *
 * @copyright Copyright (c) 2023-2026
 *
 * @details In the IoT and JSON modes, the serial port carries data for another device (mk2Wifi...).
 *          - info(), infoln(): the startup configuration, in every output mode: without it, no
 *            way to know what the router runs with. In IoT output, it is printed at 9600 baud
 *            8N1, the default of serial monitors, before setup() switches to the 7E1 of the
 *            telemetry: the mk2pvrouter ESPHome component drops everything until its first
 *            frame. In JSON output, its lines come before the first JSON line.
 *          - status(), statusln(): status changes while running, in human-readable output only.
 *          - debug(), debugln(): debug messages at startup, as info(), only with ENABLE_DEBUG.
 *
 *          The arguments are those of Serial.print() / Serial.println(). A datalog output still
 *          in progress is written first, so a message never lands in the middle of a datalog
 *          line or telemetry frame (see serial_output.h). When a message is off, the call
 *          compiles to nothing, provided its arguments have no side effects.
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
 * @brief Whether the status changes are printed while running: in human-readable output only.
 */
inline constexpr bool STATUS_OUTPUT_ENABLED{ SERIAL_OUTPUT_TYPE == SerialOutputType::HumanReadable };

/**
 * @brief Print an informational message, as Serial.print() does.
 */
template< typename... Args >
__attribute__((always_inline)) inline void info(const Args&... args)
{
  SerialOutput::complete();
  Serial.print(args...);
}

/**
 * @brief Print an informational message and a line end, as Serial.println() does.
 */
template< typename... Args >
__attribute__((always_inline)) inline void infoln(const Args&... args)
{
  SerialOutput::complete();
  Serial.println(args...);
}

/**
 * @brief Print a value given as an integer with a fixed number of decimals, and a line end.
 *
 * @details For float constants converted at compile time (SerialOutput::toDecimals()):
 *          printed without the float code of Print.
 */
inline void infolnDecimals(const int32_t value, const uint8_t decimals)
{
  SerialOutput::complete();
  SerialOutput::printDecimals(Serial, value, decimals);
  Serial.println();
}

/**
 * @brief Same as infolnDecimals(), with the value read from PROGMEM.
 */
inline void infolnDecimals_P(const int32_t* value, const uint8_t decimals)
{
  infolnDecimals(static_cast< int32_t >(pgm_read_dword(value)), decimals);
}

/**
 * @brief Print a status message, as Serial.print() does (human-readable output only).
 */
template< typename... Args >
__attribute__((always_inline)) inline void status(const Args&... args)
{
  if constexpr (STATUS_OUTPUT_ENABLED)
  {
    info(args...);
  }
}

/**
 * @brief Print a status message and a line end, as Serial.println() does (human-readable output only).
 */
template< typename... Args >
__attribute__((always_inline)) inline void statusln(const Args&... args)
{
  if constexpr (STATUS_OUTPUT_ENABLED)
  {
    infoln(args...);
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
