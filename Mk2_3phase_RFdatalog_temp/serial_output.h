/**
 * @file serial_output.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Non-blocking serial output of the datalog
 * @version 0.1
 * @date 2026-10-02
 *
 * @details At 9600 baud, a datalog line takes about 130 ms to send, and the UART's
 *          transmit buffer holds only 64 characters: a plain Serial.print() of the
 *          whole line keeps the main loop waiting for most of that time, delaying
 *          everything else it does (RF commands to the remote loads, among others).
 *
 *          Here, the output is cut into steps of at most MAX_STEP_LENGTH characters.
 *          poll(), called on every pass of the main loop, only runs a step when the
 *          transmit buffer has room for it: printing never waits.
 *
 *          Text messages (INFO..., DBUG...) first call complete(), so they never land in the
 *          middle of a datalog line or telemetry frame.
 *
 * @copyright Copyright (c) 2026-2026
 *
 */

#ifndef SERIAL_OUTPUT_H
#define SERIAL_OUTPUT_H

#include <Arduino.h>

namespace SerialOutput
{
inline constexpr uint8_t MAX_STEP_LENGTH{ 32 }; /**< a step never writes more characters than this */

/**
 * @brief One step of an output
 *
 * @param out where to write, at most MAX_STEP_LENGTH characters
 * @param step index of the step, from 0
 * @return false if this was the last step
 */
using Step = bool (*)(Print& out, uint8_t step);

inline Step current{ nullptr }; /**< output in progress, nullptr if none */
inline uint8_t nextStep{ 0 };   /**< index of its next step */

/**
 * @brief Whether an output is still in progress
 */
[[nodiscard]] inline bool busy()
{
  return current != nullptr;
}

/**
 * @brief Start an output, written by later calls to poll()
 *
 * @param step the output's step function
 * @return false if the previous output is still in progress (nothing started)
 */
inline bool start(Step step)
{
  if (busy())
  {
    return false;
  }
  current = step;
  nextStep = 0;
  return true;
}

/**
 * @brief Run the next step of the current output
 */
inline void runStep(Print& out)
{
  if (!current(out, nextStep++))
  {
    current = nullptr;
  }
}

/**
 * @brief Write as many steps as the transmit buffer can take, without waiting
 */
inline void poll(Print& out = Serial)
{
  while (current && out.availableForWrite() >= MAX_STEP_LENGTH)
  {
    runStep(out);
  }
}

/**
 * @brief Write whatever is left of the current output to Serial, waiting like a plain print
 *
 * @note Out of line and without parameters: it is called before every debug message,
 *       so each call site costs a plain call.
 */
__attribute__((noinline)) inline void complete()
{
  while (current)
  {
    runStep(Serial);
  }
}

/**
 * @brief Print a value given in hundredths, in its shortest decimal form
 *
 * @details 2137 -> "21.37", 2150 -> "21.5", 2100 -> "21", -5 -> "-0.05".
 *          What ArduinoJson wrote for the float value / 100, without its float
 *          noise (it wrote -9.969999 for -997).
 *
 * @param out where to write
 * @param value_x100 the value, in hundredths
 */
inline void printHundredths(Print& out, int16_t value_x100)
{
  const uint16_t magnitude{ value_x100 < 0 ? static_cast< uint16_t >(0U - static_cast< uint16_t >(value_x100))
                                           : static_cast< uint16_t >(value_x100) };
  if (value_x100 < 0)
  {
    out.print('-');
  }
  out.print(magnitude / 100);

  const uint8_t hundredths{ static_cast< uint8_t >(magnitude % 100) };
  if (!hundredths)
  {
    return;
  }
  out.print('.');
  out.print(hundredths / 10);
  if (hundredths % 10)
  {
    out.print(hundredths % 10);
  }
}
}  // namespace SerialOutput

#endif  // SERIAL_OUTPUT_H
