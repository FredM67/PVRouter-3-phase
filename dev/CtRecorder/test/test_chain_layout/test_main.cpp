#include <unity.h>

#include "chain_layout.h"

void setUp() {}
void tearDown() {}

void test_three_phase_pins_in_order()
{
  constexpr ChainLayout l{ layoutOf(Router::THREE_PHASE, Board::OLD, Pair::GRID) };
  TEST_ASSERT_EQUAL_UINT8(6, l.conversions);
  for (uint8_t i = 0; i != 6; ++i)
  {
    TEST_ASSERT_EQUAL_UINT8(i, l.pins[i]);
  }
  TEST_ASSERT_EQUAL_UINT8(0, l.vIndex);
  TEST_ASSERT_EQUAL_UINT8(1, l.iIndex);
}

void test_single_phase_old_seg_pins()
{
  constexpr ChainLayout grid{ layoutOf(Router::SINGLE_PHASE, Board::OLD_SEG, Pair::GRID) };
  constexpr ChainLayout diverted{ layoutOf(Router::SINGLE_PHASE, Board::OLD_SEG, Pair::DIVERTED) };
  TEST_ASSERT_EQUAL_UINT8(3, grid.conversions);
  TEST_ASSERT_EQUAL_UINT8(3, grid.pins[0]);
  TEST_ASSERT_EQUAL_UINT8(5, grid.pins[1]);
  TEST_ASSERT_EQUAL_UINT8(4, grid.pins[2]);
  TEST_ASSERT_EQUAL_UINT8(0, grid.vIndex);
  TEST_ASSERT_EQUAL_UINT8(1, grid.iIndex);
  TEST_ASSERT_EQUAL_UINT8(2, diverted.iIndex);
}

void test_single_phase_new_pins()
{
  constexpr ChainLayout n{ layoutOf(Router::SINGLE_PHASE, Board::NEW, Pair::GRID) };
  constexpr ChainLayout oled{ layoutOf(Router::SINGLE_PHASE, Board::OLD_OLED, Pair::GRID) };
  TEST_ASSERT_EQUAL_UINT8(3, n.conversions);
  TEST_ASSERT_EQUAL_UINT8(0, n.pins[0]);
  TEST_ASSERT_EQUAL_UINT8(1, n.pins[1]);
  TEST_ASSERT_EQUAL_UINT8(3, n.pins[2]);
  TEST_ASSERT_EQUAL_HEX8(0xC0, n.refBits);
  TEST_ASSERT_EQUAL_HEX8(0x40, oled.refBits);
}

void test_admux_has_reference_adlar_and_pin()
{
  constexpr ChainLayout l{ layoutOf(Router::SINGLE_PHASE, Board::NEW, Pair::GRID) };
  TEST_ASSERT_EQUAL_HEX8(0xC0 | 0x20 | 3, admuxOf(l, 2));
}

void test_after_next_wraps()
{
  constexpr ChainLayout six{ layoutOf(Router::THREE_PHASE, Board::NEW, Pair::GRID) };
  constexpr ChainLayout three{ layoutOf(Router::SINGLE_PHASE, Board::NEW, Pair::GRID) };
  TEST_ASSERT_EQUAL_UINT8(0, indexAfterNext(six, 4));
  TEST_ASSERT_EQUAL_UINT8(1, indexAfterNext(six, 5));
  TEST_ASSERT_EQUAL_UINT8(1, indexAfterNext(three, 2));
}

void test_sample_spacing()
{
  TEST_ASSERT_EQUAL_UINT16(624, sampleSpacingUs(layoutOf(Router::THREE_PHASE, Board::OLD, Pair::GRID)));
  TEST_ASSERT_EQUAL_UINT16(312, sampleSpacingUs(layoutOf(Router::SINGLE_PHASE, Board::OLD_SEG, Pair::GRID)));
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_three_phase_pins_in_order);
  RUN_TEST(test_single_phase_old_seg_pins);
  RUN_TEST(test_single_phase_new_pins);
  RUN_TEST(test_admux_has_reference_adlar_and_pin);
  RUN_TEST(test_after_next_wraps);
  RUN_TEST(test_sample_spacing);
  return UNITY_END();
}
