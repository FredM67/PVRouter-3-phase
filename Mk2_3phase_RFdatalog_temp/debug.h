/**
 * @file debug.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Macros for the text output: informational messages (INFO) and debug messages (DBUG)
 * @version 0.1
 * @date 2026-10-02
 *
 * @copyright Copyright (c) 2023-2026
 *
 */

#ifndef DEBUG_H
#define DEBUG_H

// #define ENABLE_DEBUG
// #define DEBUG_PORT Serial

#define TEXTIFY(A) #A
#define ESCAPEQUOTE(A) TEXTIFY(A)

#ifdef ARDUINO

#include "serial_output.h"

#ifndef DEBUG_USE_PRINT_P
#if defined(ESP8266)
#define DEBUG_USE_PRINT_P 1
#else
#define DEBUG_USE_PRINT_P 0
#endif  // ESP8266
#endif  // DEBUG_USE_PRINT_P

// Text output only in human-readable mode: in the IoT and JSON modes, the serial port carries
// data for another device (mk2Wifi...), which must not receive anything else.
#define TEXT_OUTPUT_ENABLED (SERIAL_OUTPUT_TYPE == SerialOutputType::HumanReadable)

// A datalog output still in progress is written first: a message never lands
// in the middle of a datalog line or telemetry frame (see serial_output.h).
#define TEXT_PRINT_STATEMENT(...) \
  do \
  { \
    if constexpr (TEXT_OUTPUT_ENABLED) \
    { \
      SerialOutput::complete(); \
      __VA_ARGS__; \
    } \
  } while (false)

// Informational messages (startup banner, configuration, status changes):
// printed whenever the output is human-readable, whatever ENABLE_DEBUG.
#define INFO(...) TEXT_PRINT_STATEMENT(DEBUG_PORT.print(__VA_ARGS__))
#define INFOLN(...) TEXT_PRINT_STATEMENT(DEBUG_PORT.println(__VA_ARGS__))

#define DEBUG_BEGIN(speed) DEBUG_PORT.begin(speed)

// Debug messages: only with ENABLE_DEBUG, and also only when the output is human-readable.
#ifdef ENABLE_DEBUG

// Use os_printf, works but also outputs additional dubug if not using Serial
//#define DEBUG_BEGIN(speed)  DEBUG_PORT.begin(speed);
// DEBUG_PORT.setDebugOutput(true) #define DBUGF(format, ...)
// os_printf(PSTR(format "\n"), ##__VA_ARGS__)

#if DEBUG_USE_PRINT_P
// Serial.printf_P needs Git version of Arduino Core
#define DBUGF(format, ...) TEXT_PRINT_STATEMENT(DEBUG_PORT.printf_P(PSTR(format "\n"), ##__VA_ARGS__))
#else
#define DBUGF(format, ...) TEXT_PRINT_STATEMENT(DEBUG_PORT.printf(format "\n", ##__VA_ARGS__))
#endif

#define DBUG(...) TEXT_PRINT_STATEMENT(DEBUG_PORT.print(__VA_ARGS__))
#define DBUGLN(...) TEXT_PRINT_STATEMENT(DEBUG_PORT.println(__VA_ARGS__))
#define DBUGVAR(x, ...) TEXT_PRINT_STATEMENT(DEBUG_PORT.print(F(ESCAPEQUOTE(x) " = ")); DEBUG_PORT.println(x, ##__VA_ARGS__))

#else  // ENABLE_DEBUG

#define DBUGF(...)
#define DBUG(...)
#define DBUGLN(...)
#define DBUGVAR(...)

#endif  // ENABLE_DEBUG

#ifdef DEBUG_SERIAL1
#error DEBUG_SERIAL1 defiend, please use -DDEBUG_PORT=Serial1 instead
#endif

#ifndef DEBUG_PORT
#define DEBUG_PORT Serial
#endif
#define DEBUG DEBUG_PORT

#else  // ARDUINO

#define DEBUG_BEGIN(speed)
#define INFO(...)
#define INFOLN(...)

#ifdef ENABLE_DEBUG

#define DBUGF(format, ...) printf(format "\n", ##__VA_ARGS__)
#define DBUG(...)
#define DBUGLN(...)
#define DBUGVAR(...)

#else

#define DBUGF(...)
#define DBUG(...)
#define DBUGLN(...)
#define DBUGVAR(...)

#endif  // DEBUG

#endif  // ARDUINO

#endif  // DEBUG_H
