/**
 * @file teleinfo.h
 * @author your name (you@domain.com)
 * @brief Manages telemetry data and frame formatting.
 *
 * This file defines the `TeleInfo` class, which is responsible for creating and sending
 * telemetry frames. It includes functions for calculating buffer sizes, formatting data,
 * and ensuring data integrity with checksums.
 *
 * @details
 * - **Frame Structure**: Frames start with a Start-of-Text (STX) character and end with an
 *   End-of-Text (ETX) character.
 * - **Checksum**: Ensures data integrity for each line in the frame.
 * - **Conditional Features**: Supports optional features like relay diversion and temperature sensing.
 * - **Serial Configuration**: Telemetry data is sent via Serial with 9600 baud, 7 data bits,
 *   1 stop bit, and even parity.
 *
 * @version 0.1
 * @date 2025-04-04
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef TELEINFO_H
#define TELEINFO_H

#include "config_system.h"
#include "config.h"

/**
 * @brief Calculates the size of a single telemetry line in the frame.
 *
 * This function computes the size of a single line in the telemetry frame, including
 * the tag, value, and formatting characters. Each line consists of:
 * - 1 byte for the Line Feed (LF) character.
 * - `tagLen` bytes for the tag.
 * - 1 byte for the Tab (TAB) character separating the tag and value.
 * - `valueLen` bytes for the value.
 * - 1 byte for the Tab (TAB) character separating the value and checksum.
 * - 1 byte for the checksum.
 * - 1 byte for the Carriage Return (CR) character.
 *
 * @param tagLen The length of the tag in bytes.
 * @param valueLen The length of the value in bytes.
 * @return The total size of the line in bytes.
 * 
 * @ingroup Telemetry
 */
inline static constexpr size_t lineSize(size_t tagLen, size_t valueLen)
{
  return 1 + tagLen + 1 + valueLen + 1 + 1 + 1;  // LF+tag+TAB+value+TAB+checksum+CR
}

/**
 * @brief Calculates the total buffer size required for the telemetry frame.
 *
 * This function computes the size of the buffer needed to store the entire telemetry frame,
 * including all tags, values, and formatting characters. The calculation takes into account
 * the presence of optional features and system configuration.
 *
 * @return The total buffer size as a compile-time constant.
 *
 * The buffer size is calculated as follows:
 * - 1 byte for the start-of-text (STX) character.
 * - 1 line for the "P" tag (signed 6 digits) - power measurement.
 * 
 * For multi-phase systems (`NO_OF_PHASES > 1`):
 * - `NO_OF_PHASES` lines for the "V1" to "Vn" tags (unsigned 5 digits each) - voltage measurements.
 * - `NO_OF_DUMPLOADS` lines for the "D1" to "Dn" tags (unsigned 3 digits each) - diversion rates.
 * 
 * For single-phase systems:
 * - 1 line for the "V" tag (unsigned 5 digits) - voltage measurement.
 * - 1 line for the "D" tag (unsigned 4 digits) - diverted power.
 * - 1 line for the "E" tag (unsigned 5 digits) - diverted energy.
 * 
 * If relay diversion is enabled (`RELAY_DIVERSION`):
 * - 1 line for the "R" tag (signed 6 digits) - mean power for relay diversion.
 * - `relays.get_size()` lines for the "R1" to "Rn" tags (1 digit each) - relay states.
 * 
 * If temperature sensors are present (`TEMP_SENSOR_PRESENT`):
 * - `temperatureSensing.get_size()` lines for the "T1" to "Tn" tags (4 digits each) - temperature readings.
 * 
 * Common for all configurations:
 * - 1 line for the "N" tag (unsigned 5 digits) - absence of diverted energy count.
 * - 1 line for the "S_MC" tag (unsigned 2 digits) - sample sets per mains cycle.
 * - 1 line for the "S" tag (unsigned 5 digits) - sample count.
 * - 1 byte for the end-of-text (ETX) character.
 * 
 * @ingroup Telemetry
 */
inline static constexpr size_t calcBufferSize()
{
  size_t size{ 1 };  // STX

  size += lineSize(1, 6);  // P (signed 6 digits)

  if constexpr (NO_OF_PHASES > 1)
  {
    size += NO_OF_PHASES * lineSize(2, 5);  // V1-Vn (unsigned 5 digits) - voltage

    size += NO_OF_DUMPLOADS * lineSize(2, 3);  // D1-Dn (unsigned 3 digits) - diversion rate
  }
  else
  {
    size += lineSize(1, 5);  // V (unsigned 5 digits) - voltage

    size += lineSize(1, 4);  // D (unsigned 4 digits) - diverted power
    size += lineSize(1, 5);  // E (unsigned 5 digits) - diverted energy
  }

  if constexpr (RELAY_DIVERSION)
  {
    size += lineSize(1, 6);                      // R (signed 6 digits) - mean power for relay diversion
    size += relays.get_size() * lineSize(2, 1);  // R1-Rn (1 (ON), 0 (OFF)) - relay state
  }

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    size += temperatureSensing.get_size() * lineSize(2, 4);  // T1-Tn (4 digits) - temperature
  }

  size += lineSize(1, 5);  // N (unsigned 5 digits) - absence of diverted energy count

  size += lineSize(4, 2);  // S_MC (unsigned 2 digits) - sample sets per mains cycle
  size += lineSize(1, 5);  // S (unsigned 5 digits) - sample count

  size += 1;  // ETX

  return size;
}

/**
 * @class TeleInfo
 * @brief A class for managing and sending telemetry information in a structured frame format.
 *
 * The `TeleInfo` class is responsible for creating and sending telemetry frames that include
 * various data points such as power, voltage, temperature, and relay states. The frames are
 * formatted with tags, values, and checksums to ensure data integrity.
 *
 * @details
 * - **Frame Structure**: Each frame starts with a Start-of-Text (STX) character and ends with
 *   an End-of-Text (ETX) character. Data points are added as lines, each containing a tag,
 *   value, and checksum.
 * - **Checksum Calculation**: A checksum is calculated for each line to ensure data integrity.
 * - **Conditional Features**: The class supports optional features such as relay diversion
 *   and temperature sensing, which are included or excluded at compile time based on
 *   configuration constants.
 * - **Buffer Management**: A buffer is used to store the frame data before sending it over
 *   the Serial interface.
 * - **Serial Configuration**: Uses Serial with 9600 baud, 7 data bits, 1 stop bit, and even parity.
 *
 * @ingroup Telemetry
 */
class TeleInfo
{
private:
  static const char STX{ 0x02 }; /**< Start of Frame character. */
  static const char ETX{ 0x03 }; /**< End of Frame character. */
  static const char LF{ 0x0A };  /**< Line Feed character. */
  static const char CR{ 0x0D };  /**< Carriage Return character. */
  static const char TAB{ 0x09 }; /**< Tab character. */

  char buffer[calcBufferSize()]{}; /**< Buffer to store the frame data. Adjust size as needed. */
  size_t bufferPos{ 0 };           /**< Current position in the buffer. */

  /**
   * @brief Calculates the checksum for a portion of the buffer.
   * @param startPos The starting position in the buffer.
   * @param endPos The ending position in the buffer.
   * @return The calculated checksum as a single byte.
   */
  [[nodiscard]] uint8_t calculateChecksum(size_t startPos, size_t endPos) const
  {
    uint8_t sum{ 0 };
    auto* ptr = buffer + startPos;
    const auto* end = buffer + endPos;

    // Process 4 bytes at once for longer segments
    while (ptr + 4 <= end)
    {
      sum += *ptr++;
      sum += *ptr++;
      sum += *ptr++;
      sum += *ptr++;
    }

    // Handle remaining bytes
    while (ptr < end)
    {
      sum += *ptr++;
    }

    return (sum & 0x3F) + 0x20;
  }

  /**
   * @brief Writes a tag to the buffer.
   * @param tag The tag to write.
   */
  __attribute__((always_inline)) void writeTag(const char* tag, uint8_t index)
  {
    auto* ptr{ tag };
    while (*ptr) buffer[bufferPos++] = *ptr++;

    // If an index is provided, append it to the tag
    if (index != 0)
    {
      buffer[bufferPos++] = static_cast< char >('0' + index);  // Convert index to a character
    }

    buffer[bufferPos++] = TAB;
  }

public:
  /**
   * @brief Initializes a new frame by resetting the buffer and adding the start character.
   */
  __attribute__((always_inline)) void startFrame()
  {
    bufferPos = 0;
    buffer[bufferPos++] = STX;
  }

  /**
   * @brief Sends a telemetry value as an integer.
   * @param tag The tag associated with the value.
   * @param value The integer value to send.
   */
  void send(const char* tag, int16_t value, uint8_t index = 0)
  {
    buffer[bufferPos++] = LF;

    const auto startPos{ bufferPos };

    writeTag(tag, index);
    auto str = itoa(value, buffer + bufferPos, 10);
    bufferPos += strlen(str);  // Advance bufferPos by the length of the written string
    buffer[bufferPos++] = TAB;

    const auto crc{ calculateChecksum(startPos, bufferPos) };
    buffer[bufferPos++] = crc;

    buffer[bufferPos++] = CR;
  }

  /**
   * @brief Finalizes the frame by adding the end character and sending the buffer over Serial.
   */
  __attribute__((always_inline)) void endFrame()
  {
    buffer[bufferPos++] = ETX;
    Serial.write(buffer, bufferPos);
  }
};

#endif /* TELEINFO_H */
