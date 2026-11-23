#pragma once
/**
 * @file chain_layout.h
 * @brief The ADC chain of each router flavour: which pins, in which order, with which reference.
 *
 * Pure C++ (no Arduino), so it is tested on the PC. The sketch checks the ADMUX bit numbers
 * against avr/io.h with a static_assert, as the router firmware does.
 */

#include <stdint.h>

enum class Router : uint8_t
{
  SINGLE_PHASE,
  THREE_PHASE
};

/** OLD is the 3-phase old board. */
enum class Board : uint8_t
{
  OLD_SEG,
  OLD_OLED,
  OLD,
  NEW
};

/** Which current is recorded with the voltage (single-phase: CT1 is the grid, CT2 the diverted one). */
enum class Pair : uint8_t
{
  GRID,
  DIVERTED
};

// ADMUX bit numbers (ATmega328P)
inline constexpr uint8_t ADMUX_REFS0{ 6 };
inline constexpr uint8_t ADMUX_REFS1{ 7 };
inline constexpr uint8_t ADMUX_ADLAR{ 5 };

/** ADC clocks per conversion at the free-running rate: 13 clocks, prescaler 128, 16 MHz. */
inline constexpr uint16_t CONVERSION_US{ 104 };

struct ChainLayout
{
  uint8_t conversions;  // ADC conversions per sample set
  uint8_t pins[6];      // ADC pin of each conversion, in the order the firmware samples them
  uint8_t vIndex;       // position of the recorded voltage in the set
  uint8_t iIndex;       // position of the recorded current in the set
  uint8_t refBits;      // ADMUX reference bits: AVcc (OLD*) or the internal 1.1 V (NEW)
};

constexpr ChainLayout layoutOf(Router router, Board board, Pair pair)
{
  const uint8_t refBits{ (board == Board::NEW) ? static_cast< uint8_t >((1U << ADMUX_REFS1) | (1U << ADMUX_REFS0))
                                               : static_cast< uint8_t >(1U << ADMUX_REFS0) };

  if (router == Router::THREE_PHASE)
  {
    // V1, I1, V2, I2, V3, I3 on pins 0..5; the recorded pair is V1/I1
    return ChainLayout{ 6, { 0, 1, 2, 3, 4, 5 }, 0, 1, refBits };
  }

  // single-phase: V, CT1, CT2
  const uint8_t iIndex{ (pair == Pair::DIVERTED) ? uint8_t{ 2 } : uint8_t{ 1 } };
  if (board == Board::OLD_SEG)
  {
    return ChainLayout{ 3, { 3, 5, 4, 0, 0, 0 }, 0, iIndex, refBits };
  }
  return ChainLayout{ 3, { 0, 1, 3, 0, 0, 0 }, 0, iIndex, refBits };
}

/** The ADMUX value that selects conversion `index` of the set (reference | ADLAR | pin). */
constexpr uint8_t admuxOf(const ChainLayout &layout, uint8_t index)
{
  return static_cast< uint8_t >(layout.refBits | (1U << ADMUX_ADLAR) | layout.pins[index]);
}

/** The ADC runs freely: the ISR of a conversion writes ADMUX for the one after the next. */
constexpr uint8_t indexAfterNext(const ChainLayout &layout, uint8_t index)
{
  return static_cast< uint8_t >((index + 2U) % layout.conversions);
}

constexpr uint16_t sampleSpacingUs(const ChainLayout &layout)
{
  return static_cast< uint16_t >(layout.conversions * CONVERSION_US);
}
