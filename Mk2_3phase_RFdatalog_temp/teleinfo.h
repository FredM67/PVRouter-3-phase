/**
 * @file teleinfo.h
 * @author your name (you@domain.com)
 * @brief The telemetry frame of the IoT output, written a line at a time.
 *
 * @details
 * - **Frame Structure**: a frame starts with a Start-of-Text (STX) character and ends with an
 *   End-of-Text (ETX) character. In between, one line per value: LF, tag (and index), TAB,
 *   value, TAB, checksum, CR.
 * - **Checksum**: each line carries its own, (the sum of the bytes from the tag to the TAB
 *   before the checksum) & 0x3F, + 0x20.
 * - **Streamed**: the frame is written one line per step of the serial output (see
 *   serial_output.h), as the text and JSON outputs: a line is built in a buffer of a few bytes
 *   on the stack, there is no frame buffer.
 * - **Arduino-free**: the values come from a data source given as a template parameter (see
 *   writeFrameStep()), so the frame is tested natively (test/native/test_teleinfo).
 * - **Serial Configuration**: Serial at 9600 baud, 7 data bits, even parity, 1 stop bit.
 *
 * @version 0.2
 * @date 2026-11-24
 *
 * @copyright Copyright (c) 2025-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#ifndef TELEINFO_H
#define TELEINFO_H

#include <stdint.h>

/**
 * @brief The telemetry frame of the IoT output.
 *
 * @ingroup Telemetry
 */
namespace TeleInfo
{
inline constexpr char STX{ 0x02 }; /**< Start of Frame character. */
inline constexpr char ETX{ 0x03 }; /**< End of Frame character. */
inline constexpr char LF{ 0x0A };  /**< Line Feed character. */
inline constexpr char CR{ 0x0D };  /**< Carriage Return character. */
inline constexpr char TAB{ 0x09 }; /**< Tab character. */

/**
 * @brief The size of a line: LF, tag, TAB, value, TAB, checksum and CR.
 *
 * @param tagLength The length of the tag, its index included.
 * @param valueLength The length of the value, its sign included.
 * @return The size of the line in bytes.
 */
constexpr uint8_t lineSize(uint8_t tagLength, uint8_t valueLength)
{
  return 1 + tagLength + 1 + valueLength + 1 + 1 + 1;
}

/**
 * @brief The most a step writes: STX or ETX, and the longest line, a 4-character tag ("S_MC")
 *        with a signed 16-bit value (-32768).
 */
inline constexpr uint8_t MAX_STEP_LENGTH{ 1 + lineSize(4, 6) };

/**
 * @brief Writes the decimal digits of a 16-bit value.
 *
 * @param dst Where to write.
 * @param value The value.
 * @return The end of what was written.
 */
inline char* writeDigits(char* dst, uint16_t value)
{
  char digits[5];
  uint8_t count{ 0 };
  do
  {
    digits[count++] = static_cast< char >('0' + value % 10);
    value /= 10;
  } while (value != 0);

  do
  {
    *dst++ = digits[--count];
  } while (count != 0);

  return dst;
}

/**
 * @brief Writes a whole line: LF, tag and index, TAB, value, TAB, checksum and CR.
 *
 * @details The checksum is summed over the line once written: about 15 bytes, once per datalog
 *          period, in the main loop.
 *
 * @param dst Where to write, room for lineSize() bytes.
 * @param tag The tag, at most 4 characters with the index.
 * @param index Appended to the tag as a digit (1-9), none when 0.
 * @param magnitude The magnitude of the value.
 * @param negative Whether the value is negative.
 * @return The end of the line.
 */
inline char* writeLine(char* dst, const char* tag, uint8_t index, uint16_t magnitude, bool negative)
{
  *dst++ = LF;

  char* const start{ dst };
  while (*tag)
  {
    *dst++ = *tag++;
  }
  if (index != 0)
  {
    *dst++ = static_cast< char >('0' + index);
  }
  *dst++ = TAB;

  if (negative)
  {
    *dst++ = '-';
  }
  dst = writeDigits(dst, magnitude);
  *dst++ = TAB;

  uint8_t sum{ 0 };
  for (const char* ptr = start; ptr != dst; ++ptr)
  {
    sum += static_cast< uint8_t >(*ptr);
  }
  *dst++ = static_cast< char >((sum & 0x3F) + 0x20);

  *dst++ = CR;
  return dst;
}

/**
 * @brief Writes the line of a signed value.
 *
 * @param dst Where to write, room for lineSize() bytes.
 * @param tag The tag.
 * @param value The value.
 * @param index Appended to the tag as a digit (1-9), none when 0.
 * @return The end of the line.
 */
inline char* line(char* dst, const char* tag, int16_t value, uint8_t index = 0)
{
  const uint16_t magnitude{ value < 0 ? static_cast< uint16_t >(0U - static_cast< uint16_t >(value)) : static_cast< uint16_t >(value) };
  return writeLine(dst, tag, index, magnitude, value < 0);
}

/**
 * @brief Writes the line of an unsigned value (above 32767, line() would wrap).
 *
 * @param dst Where to write, room for lineSize() bytes.
 * @param tag The tag.
 * @param value The value.
 * @param index Appended to the tag as a digit (1-9), none when 0.
 * @return The end of the line.
 */
inline char* lineUnsigned(char* dst, const char* tag, uint16_t value, uint8_t index = 0)
{
  return writeLine(dst, tag, index, value, false);
}

/**
 * @brief The number of steps of a frame: one per line, invalid temperatures included (they
 *        write nothing).
 */
template< typename Source >
constexpr uint8_t frameSteps()
{
  uint8_t steps{ 1 };  // P

  if constexpr (Source::PHASE_COUNT > 1)
  {
    steps += 2 * Source::PHASE_COUNT;  // Pn, Vn ... P1, V1
  }

  if constexpr (Source::RELAY_DIVERSION)
  {
    steps += 1 + Source::RELAY_COUNT;  // R, R1-Rn
  }
  steps += Source::LOAD_COUNT + Source::TEMP_COUNT;  // D1-Dn, T1-Tn
  steps += 1;                                        // N
  if constexpr (Source::DUAL_TARIFF)
  {
    steps += 1;  // TA
  }
  steps += 1 + 1;  // S, S_MC

  return steps;
}

/**
 * @brief Writes the line of a step.
 *
 * @param step The index of the step.
 * @param dst Where to write.
 * @return The end of the line, dst for an invalid temperature (no line).
 */
template< typename Source >
char* frameLine(uint8_t step, char* dst)
{
  if (step == 0)
  {
    return line(dst, "P", Source::power());
  }
  --step;

  if constexpr (Source::PHASE_COUNT > 1)
  {
    if (step < 2 * Source::PHASE_COUNT)  // Pn, Vn first
    {
      const uint8_t phase{ static_cast< uint8_t >(Source::PHASE_COUNT - 1 - step / 2) };
      return (step & 1) == 0 ? line(dst, "P", Source::phasePower(phase), phase + 1) : lineUnsigned(dst, "V", Source::phaseVoltage(phase), phase + 1);
    }
    step -= 2 * Source::PHASE_COUNT;
  }

  if constexpr (Source::RELAY_DIVERSION)
  {
    if (step == 0)
    {
      return line(dst, "R", Source::relayAverage());
    }
    --step;

    if (step < Source::RELAY_COUNT)  // Rn first
    {
      const uint8_t index{ static_cast< uint8_t >(Source::RELAY_COUNT - step) };
      return line(dst, "R", Source::relayOn(index - 1), index);
    }
    step -= Source::RELAY_COUNT;
  }

  if (step < Source::LOAD_COUNT)  // Dn first
  {
    const uint8_t index{ static_cast< uint8_t >(Source::LOAD_COUNT - step) };
    return line(dst, "D", Source::loadOnPercent(index - 1), index);
  }
  step -= Source::LOAD_COUNT;

  if constexpr (Source::TEMP_COUNT != 0)
  {
    if (step < Source::TEMP_COUNT)
    {
      return Source::temperatureValid(step) ? line(dst, "T", Source::temperature(step), step + 1) : dst;
    }
    step -= Source::TEMP_COUNT;
  }

  if (step == 0)
  {
    return lineUnsigned(dst, "N", Source::absenceCount());
  }
  --step;

  if constexpr (Source::DUAL_TARIFF)
  {
    if (step == 0)
    {
      return line(dst, "TA", Source::offPeak() ? 1 : 0);
    }
    --step;
  }

  if (step == 0)
  {
    return lineUnsigned(dst, "S", Source::sampleSets());
  }
  return line(dst, "S_MC", Source::lowestSampleSetsPerCycle());
}

/**
 * @brief Writes a step of the frame: one line, STX before the first one, ETX after the last.
 *
 * @details The lines, in this order:
 * - "P": the power at the grid connection, in W (import > 0);
 * - with several phases, "Pn" then "Vn" for each phase, the last phase first: the phase's
 *   power in W, and its RMS voltage in hundredths of V;
 * - with relay diversion, "R": the relays' average power, in W, then "R1" to "Rn": each relay's
 *   state (1 ON, 0 OFF), Rn first;
 * - "D1" to "Dn": each load's ON time over the datalog period, in %, Dn first;
 * - with temperature sensing, "T1" to "Tn": each valid temperature, in hundredths of °C;
 * - "N": the number of seconds without diverted energy;
 * - with dual tariff, "TA": the tariff (0 high/on-peak, 1 low/off-peak);
 * - "S": the number of sample sets over the datalog period;
 * - "S_MC": the lowest number of sample sets in a mains cycle.
 *
 * The data source is a type with these members:
 * - `static constexpr bool RELAY_DIVERSION`, `DUAL_TARIFF`;
 * - `static constexpr uint8_t PHASE_COUNT`, `RELAY_COUNT`, `LOAD_COUNT`, `TEMP_COUNT`;
 * - `static int16_t power()`, `phasePower(uint8_t idx)`, `relayAverage()`,
 *   `loadOnPercent(uint8_t idx)`, `temperature(uint8_t idx)`;
 * - `static uint16_t phaseVoltage(uint8_t idx)`, `absenceCount()`, `sampleSets()`;
 * - `static bool relayOn(uint8_t idx)`, `temperatureValid(uint8_t idx)`, `offPeak()`;
 * - `static uint8_t lowestSampleSetsPerCycle()`.
 *
 * Every value of a frame must stay that of the same datalog period until its last step.
 *
 * @param step The index of the step, from 0.
 * @param text Where to write, room for MAX_STEP_LENGTH characters.
 * @param[out] length The number of characters written (0 for an invalid temperature).
 * @return false at the last step.
 */
template< typename Source >
bool writeFrameStep(const uint8_t step, char* const text, uint8_t& length)
{
  char* end{ text };
  if (step == 0)
  {
    *end++ = STX;
  }

  end = frameLine< Source >(step, end);

  const bool last{ step == frameSteps< Source >() - 1 };
  if (last)
  {
    *end++ = ETX;
  }

  length = static_cast< uint8_t >(end - text);
  return !last;
}
}  // namespace TeleInfo

#endif /* TELEINFO_H */
