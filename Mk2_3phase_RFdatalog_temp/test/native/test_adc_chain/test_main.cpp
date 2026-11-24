// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file test_main.cpp
 * @brief Native tests for adc_chain.h: the six conversions, the reference and the ADMUX pipeline
 *
 * @details The ADC converts six channels in a fixed chain V1, I1, V2, I2, V3, I3. It runs free:
 *          when the ISR of a finished conversion runs, the next conversion has already started
 *          with the previous ADMUX, so the ADMUX written now applies to the conversion after
 *          next. The pipeline test plays this out and checks that the channel really converted
 *          is the one the ISR believes it handles, on each board. The last test pins the values
 *          of the table that processing.cpp wrote by hand before it was built from adc_chain.h.
 */

#include <unity.h>

#include "adc_chain.h"  // Real header - pure C++

void setUp(void) {}
void tearDown(void) {}

using AdcChain::Channel;

inline constexpr PcbVersion boards[]{ PcbVersion::OLD, PcbVersion::NEW };
inline constexpr Channel chain[]{ Channel::V1, Channel::I1, Channel::V2, Channel::I2, Channel::V3, Channel::I3 };

inline constexpr uint8_t MUX_MASK{ 0x0F };                      /**< MUX3..0: the analog input */
inline constexpr uint8_t REF_MASK{ (1U << 7) | (1U << 6) };     /**< REFS1, REFS0 */
inline constexpr uint8_t AVCC{ 1U << 6 };                       /**< REFS0 alone: AVcc */
inline constexpr uint8_t INTERNAL_1V1{ (1U << 7) | (1U << 6) }; /**< REFS1 and REFS0: internal 1.1 V */
inline constexpr uint8_t ADLAR_BIT{ 1U << 5 };                  /**< left-adjusted result */

/** @brief The channel an ADMUX value selects, from its analog input. */
static Channel channelOfAdmux(const uint8_t admux)
{
  const uint8_t pin{ static_cast< uint8_t >(admux & MUX_MASK) };
  for (const Channel ch : chain)
  {
    if (AdcChain::pinOf(ch) == pin)
    {
      return ch;
    }
  }
  TEST_FAIL_MESSAGE("ADMUX selects an input that is no channel of the chain");
  return Channel::V1;
}

void test_order_of_the_chain(void)
{
  static_assert(AdcChain::NO_OF_CONVERSIONS == 6, "three phases, a V and an I each");

  TEST_ASSERT_EQUAL_UINT8(0, static_cast< uint8_t >(Channel::V1));
  TEST_ASSERT_EQUAL_UINT8(1, static_cast< uint8_t >(Channel::I1));
  TEST_ASSERT_EQUAL_UINT8(2, static_cast< uint8_t >(Channel::V2));
  TEST_ASSERT_EQUAL_UINT8(3, static_cast< uint8_t >(Channel::I2));
  TEST_ASSERT_EQUAL_UINT8(4, static_cast< uint8_t >(Channel::V3));
  TEST_ASSERT_EQUAL_UINT8(5, static_cast< uint8_t >(Channel::I3));
}

void test_pins(void)
{
  // V1 A0, I1 A1, V2 A2, I2 A3, V3 A4, I3 A5
  TEST_ASSERT_EQUAL_UINT8(0, AdcChain::pinOf(Channel::V1));
  TEST_ASSERT_EQUAL_UINT8(1, AdcChain::pinOf(Channel::I1));
  TEST_ASSERT_EQUAL_UINT8(2, AdcChain::pinOf(Channel::V2));
  TEST_ASSERT_EQUAL_UINT8(3, AdcChain::pinOf(Channel::I2));
  TEST_ASSERT_EQUAL_UINT8(4, AdcChain::pinOf(Channel::V3));
  TEST_ASSERT_EQUAL_UINT8(5, AdcChain::pinOf(Channel::I3));

  static_assert(AdcChain::pinOf(Channel::I3) == 5, "pinOf() must be constexpr");
}

void test_reference_per_board(void)
{
  TEST_ASSERT_EQUAL_HEX8(AVCC, AdcChain::refBits(PcbVersion::OLD));
  TEST_ASSERT_EQUAL_HEX8(INTERNAL_1V1, AdcChain::refBits(PcbVersion::NEW));

  static_assert(AdcChain::refBits(PcbVersion::NEW) == INTERNAL_1V1, "refBits() must be constexpr");
}

void test_admux_of_is_reference_adlar_and_pin(void)
{
  for (const PcbVersion board : boards)
  {
    for (const Channel ch : chain)
    {
      TEST_ASSERT_EQUAL_HEX8(AdcChain::refBits(board) | ADLAR_BIT | AdcChain::pinOf(ch), AdcChain::admuxOf(board, ch));
    }
  }
  TEST_ASSERT_EQUAL_HEX8(0x60, AdcChain::admuxOf(PcbVersion::OLD, Channel::V1));
  TEST_ASSERT_EQUAL_HEX8(0xE5, AdcChain::admuxOf(PcbVersion::NEW, Channel::I3));
}

void test_after_next_of_every_position(void)
{
  // the ADMUX written after a conversion applies to the conversion after next, wrap included
  TEST_ASSERT_EQUAL_UINT8(static_cast< uint8_t >(Channel::V2), static_cast< uint8_t >(AdcChain::channelAfterNext(Channel::V1)));
  TEST_ASSERT_EQUAL_UINT8(static_cast< uint8_t >(Channel::I2), static_cast< uint8_t >(AdcChain::channelAfterNext(Channel::I1)));
  TEST_ASSERT_EQUAL_UINT8(static_cast< uint8_t >(Channel::V3), static_cast< uint8_t >(AdcChain::channelAfterNext(Channel::V2)));
  TEST_ASSERT_EQUAL_UINT8(static_cast< uint8_t >(Channel::I3), static_cast< uint8_t >(AdcChain::channelAfterNext(Channel::I2)));
  TEST_ASSERT_EQUAL_UINT8(static_cast< uint8_t >(Channel::V1), static_cast< uint8_t >(AdcChain::channelAfterNext(Channel::V3)));  // wrap
  TEST_ASSERT_EQUAL_UINT8(static_cast< uint8_t >(Channel::I1), static_cast< uint8_t >(AdcChain::channelAfterNext(Channel::I3)));  // wrap

  static_assert(AdcChain::channelAfterNext(Channel::I3) == Channel::I1, "channelAfterNext() must be constexpr");
}

void test_admux_written_after_selects_the_channel_after_next(void)
{
  for (const PcbVersion board : boards)
  {
    for (const Channel ch : chain)
    {
      TEST_ASSERT_EQUAL_HEX8(AdcChain::admuxOf(board, AdcChain::channelAfterNext(ch)), AdcChain::admuxWrittenAfter(board, ch));
    }
  }
}

void test_channel_converted_by_round(void)
{
  for (uint8_t round = 0; round != 30; ++round)
  {
    TEST_ASSERT_EQUAL_UINT8(round % 6, static_cast< uint8_t >(AdcChain::channelConverted(round)));
  }
  static_assert(AdcChain::channelConverted(6) == Channel::V1, "channelConverted() must be constexpr");
}

void test_pipeline_order(void)
{
  constexpr uint8_t ROUNDS{ 60 };

  for (const PcbVersion board : boards)
  {
    // admuxOf[k]: the ADMUX the conversion of round k runs with
    uint8_t admuxOf[ROUNDS + 2]{};

    // the chain primed with V1, then I1
    admuxOf[0] = AdcChain::admuxOf(board, Channel::V1);
    admuxOf[1] = AdcChain::admuxOf(board, Channel::I1);

    for (uint8_t round = 0; round != ROUNDS; ++round)
    {
      // the ISR of round k believes it handles channelConverted(k)...
      const Channel believed{ AdcChain::channelConverted(round) };

      // ... and the ADC really converted the channel its ADMUX selected
      TEST_ASSERT_EQUAL_UINT8(static_cast< uint8_t >(believed), static_cast< uint8_t >(channelOfAdmux(admuxOf[round])));
      TEST_ASSERT_EQUAL_HEX8(AdcChain::refBits(board), admuxOf[round] & REF_MASK);
      TEST_ASSERT_EQUAL_HEX8(ADLAR_BIT, admuxOf[round] & ADLAR_BIT);

      // the conversion of round k+1 has already started: the write of round k applies to round k+2
      admuxOf[round + 2] = AdcChain::admuxWrittenAfter(board, believed);
    }
  }
}

void test_channel_table_matches_the_former_literals(void)
{
  // the table processing.cpp held by hand: { next, index, admux }, ADMUX = reference | ADLAR | pin
  struct Former
  {
    uint8_t index;
    uint8_t setupPin; /**< the pin the ADMUX of this entry selects */
  };
  constexpr Former former[6]{
    { 0, 2 },  // V1 -> setup for V2
    { 1, 3 },  // I1 -> setup for I2
    { 2, 4 },  // V2 -> setup for V3
    { 3, 5 },  // I2 -> setup for I3
    { 4, 0 },  // V3 -> setup for V1
    { 5, 1 }   // I3 -> setup for I1
  };

  for (const PcbVersion board : boards)
  {
    const uint8_t ref{ board == PcbVersion::NEW ? static_cast< uint8_t >(INTERNAL_1V1) : AVCC };
    for (const Channel ch : chain)
    {
      const Former &f{ former[static_cast< uint8_t >(ch)] };
      TEST_ASSERT_EQUAL_UINT8(f.index, static_cast< uint8_t >(ch));
      TEST_ASSERT_EQUAL_HEX8(ref | ADLAR_BIT | f.setupPin, AdcChain::admuxWrittenAfter(board, ch));
    }
    // the start-up ADMUX: reference, ADLAR, V1 (_ADMUX | sensorV[0])
    TEST_ASSERT_EQUAL_HEX8(ref | ADLAR_BIT | 0, AdcChain::admuxOf(board, Channel::V1));
  }
}

int main(int /* argc */, char ** /* argv */)
{
  UNITY_BEGIN();

  RUN_TEST(test_order_of_the_chain);
  RUN_TEST(test_pins);
  RUN_TEST(test_reference_per_board);
  RUN_TEST(test_admux_of_is_reference_adlar_and_pin);
  RUN_TEST(test_after_next_of_every_position);
  RUN_TEST(test_admux_written_after_selects_the_channel_after_next);
  RUN_TEST(test_channel_converted_by_round);
  RUN_TEST(test_pipeline_order);
  RUN_TEST(test_channel_table_matches_the_former_literals);

  return UNITY_END();
}
