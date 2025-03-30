#ifndef TELEINFO_H
#define TELEINFO_H

#include <Arduino.h>

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
 * the presence of optional features such as relay diversion and temperature sensing.
 *
 * @return The total buffer size as a compile-time constant.
 *
 * The buffer size is calculated as follows:
 * - 1 byte for the start-of-text (STX) character.
 * - 1 line for the "P" tag (signed 6 digits).
 * - For multi-phase systems (`NO_OF_PHASES > 1`):
 *   - `NO_OF_PHASES` lines for the "Vn" tag (unsigned 5 digits each).
 * - For single-phase systems:
 *   - 1 line for the "D" tag (unsigned 4 digits).
 *   - 1 line for the "E" tag (unsigned 5 digits).
 * - If relay diversion is enabled (`RELAY_DIVERSION`):
 *   - 1 line for the "R" tag (signed 6 digits).
 *   - `relays.get_size()` lines for relay states ("R1" to "Rn", 1 digit each).
 * - If temperature sensors are present (`TEMP_SENSOR_PRESENT`):
 *   - `temperatureSensing.get_size()` lines for temperature tags ("T1" to "Tn", 4 digits each).
 * - 1 line for the "N" tag (unsigned 5 digits).
 * - 1 byte for the end-of-text (ETX) character.
 */
inline static constexpr size_t calcBufferSize()
{
  size_t size{ 1 };  // STX

  size += lineSize(1, 6);  // P (signed 6 digits)

  if constexpr (NO_OF_PHASES > 1)
  {
    size += NO_OF_PHASES * lineSize(2, 5);  // V1 (unsigned 5 digits)
  }
  else
  {
    size += lineSize(1, 4);  // D (unsigned 4 digits)
    size += lineSize(1, 5);  // E (unsigned 5 digits)
  }

  size += NO_OF_DUMPLOADS * lineSize(2, 3);  // L1-Ln (unsigned 3 digits)

  if constexpr (RELAY_DIVERSION)
  {
    size += lineSize(1, 6);                      // R (signed 6 digits)
    size += relays.get_size() * lineSize(2, 1);  // R1-Rn (1 (ON), 0 (OFF))
  }

  if constexpr (TEMP_SENSOR_PRESENT)
  {
    size += temperatureSensing.get_size() * lineSize(2, 4);  // T1-Tn (4 digits)
  }

  size += lineSize(1, 5);  // N (unsigned 5 digits)

  size += 1;  // ETX

  return size;
}

/**
 * @class TeleInfo
 * @brief A class for managing and sending telemetry information in a specific frame format.
 */
class TeleInfo
{
private:
  static const char STX{ 0x02 }; /**< Start of Frame character. */
  static const char ETX{ 0x03 }; /**< End of Frame character. */
  static const char LF{ 0x0A };  /**< Line Feed character. */
  static const char CR{ 0x0D };  /**< Carriage Return character. */
  static const char TAB{ 0x09 }; /**< Tab character. */

  char buffer[calcBufferSize()]; /**< Buffer to store the frame data. Adjust size as needed. */
  uint8_t bufferPos;             /**< Current position in the buffer. */

  /**
   * @brief Calculates the checksum for a portion of the buffer.
   * @param startPos The starting position in the buffer.
   * @param endPos The ending position in the buffer.
   * @return The calculated checksum as a single byte.
   */
  uint8_t calculateChecksum(uint8_t startPos, uint8_t endPos) const
  {
    uint8_t sum{ 0 };
    uint8_t i{ startPos };

    // Process 4 bytes at a time (loop unrolling)
    for (; i + 3 < endPos; i += 4)
    {
      sum += buffer[i];
      sum += buffer[i + 1];
      sum += buffer[i + 2];
      sum += buffer[i + 3];
    }

    // Process remaining bytes
    for (; i < endPos; i++)
    {
      sum += buffer[i];
    }

    return (sum & 0x3F) + 0x20;
  }

  /**
   * @brief Writes a tag to the buffer.
   * @param tag The tag to write.
   */
  void writeTag(const char* tag, uint8_t index)
  {
    const char* ptr{ tag };
    while (*ptr) buffer[bufferPos++] = *ptr++;

    // If an index is provided, append it to the tag
    if (index > 0)
    {
      buffer[bufferPos++] = '0' + index;  // Convert index to a character
    }

    buffer[bufferPos++] = TAB;
  }

public:
  /**
   * @brief Initializes a new frame by resetting the buffer and adding the start character.
   */
  void startFrame()
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

    const uint8_t startPos{ bufferPos };

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
  void endFrame()
  {
    buffer[bufferPos++] = ETX;
    Serial.write(buffer, bufferPos);
  }
};

#endif /* TELEINFO_H */
