#pragma once
/**
 * @file frame.h
 * @brief The 8-byte frame the recorder streams: one voltage and one current of the same sample set.
 *
 * Pure C++ (no Arduino), tested on the PC.
 *
 * Layout:
 *   byte 0     0xA5 (sync)
 *   byte 1     flags: bit0 load ON, bit1 filter ON, bit2 cycle start, bit3 overrun before this frame, bits 4..7 zero
 *   bytes 2-3  V raw, uint16 little-endian, 10-bit right-aligned (ADCW >> 6)
 *   bytes 4-5  I raw, uint16 little-endian, 10-bit right-aligned
 *   bytes 6-7  I out, int16 little-endian: the firmware's current minus DC (its x64 scale), filtered when the filter is on
 */

#include <stdint.h>

inline constexpr uint8_t FRAME_SIZE{ 8 };
inline constexpr uint8_t FRAME_SYNC{ 0xA5 };
inline constexpr uint16_t RAW_LIMIT{ 1024 };  // raw values are 10-bit

// The flags are bit-fields: 7 bytes on the AVR instead of 10, so that 128 frames of the ring fit the 2 KB of RAM
// (no default member initializers for bit-fields before C++20: build it with FrameData f{})
struct FrameData
{
  uint16_t v;    // voltage, 0..1023
  uint16_t i;    // current, 0..1023
  int16_t iOut;  // current minus DC, filtered when the filter is on
  bool loadOn : 1;
  bool filterOn : 1;
  bool cycleStart : 1;
  bool overrun : 1;  // frames were lost before this one
};

inline constexpr uint8_t FLAG_LOAD_ON{ 1U << 0 };
inline constexpr uint8_t FLAG_FILTER_ON{ 1U << 1 };
inline constexpr uint8_t FLAG_CYCLE_START{ 1U << 2 };
inline constexpr uint8_t FLAG_OVERRUN{ 1U << 3 };
inline constexpr uint8_t FLAGS_MASK{ FLAG_LOAD_ON | FLAG_FILTER_ON | FLAG_CYCLE_START | FLAG_OVERRUN };

inline void encodeFrame(uint8_t out[FRAME_SIZE], const FrameData &f)
{
  const uint16_t iOut{ static_cast< uint16_t >(f.iOut) };
  uint8_t flags{ 0 };
  if (f.loadOn)
  {
    flags |= FLAG_LOAD_ON;
  }
  if (f.filterOn)
  {
    flags |= FLAG_FILTER_ON;
  }
  if (f.cycleStart)
  {
    flags |= FLAG_CYCLE_START;
  }
  if (f.overrun)
  {
    flags |= FLAG_OVERRUN;
  }
  out[0] = FRAME_SYNC;
  out[1] = flags;
  out[2] = static_cast< uint8_t >(f.v);
  out[3] = static_cast< uint8_t >(f.v >> 8);
  out[4] = static_cast< uint8_t >(f.i);
  out[5] = static_cast< uint8_t >(f.i >> 8);
  out[6] = static_cast< uint8_t >(iOut);
  out[7] = static_cast< uint8_t >(iOut >> 8);
}

/** False on a bad sync byte, a non-zero high flag bit or a raw value of 1024 or more. */
inline bool decodeFrame(const uint8_t in[FRAME_SIZE], FrameData &f)
{
  if ((in[0] != FRAME_SYNC) || ((in[1] & ~FLAGS_MASK) != 0))
  {
    return false;
  }
  const uint16_t v{ static_cast< uint16_t >(in[2] | (in[3] << 8)) };
  const uint16_t i{ static_cast< uint16_t >(in[4] | (in[5] << 8)) };
  if ((v >= RAW_LIMIT) || (i >= RAW_LIMIT))
  {
    return false;
  }
  f.v = v;
  f.i = i;
  f.iOut = static_cast< int16_t >(static_cast< uint16_t >(in[6] | (in[7] << 8)));
  f.loadOn = (in[1] & FLAG_LOAD_ON) != 0;
  f.filterOn = (in[1] & FLAG_FILTER_ON) != 0;
  f.cycleStart = (in[1] & FLAG_CYCLE_START) != 0;
  f.overrun = (in[1] & FLAG_OVERRUN) != 0;
  return true;
}
