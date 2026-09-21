/**
 * @file test_main.cpp
 * @brief Native unit tests for utils_override.h
 *
 * Tests pure C++ bitmask utilities: PinList, KeyIndexPair, OverridePins,
 * indicesToBitmask, are_pins_valid. No config.h, no stubs - just the real code.
 */

#include <unity.h>

#include "utils_override.h"  // Real header - pure C++

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Tests for indicesToBitmask<>()
// ============================================================================

void test_indicesToBitmask_single_index(void)
{
  TEST_ASSERT_EQUAL(1U << 5, indicesToBitmask< 5 >());
  TEST_ASSERT_EQUAL(1U << 0, indicesToBitmask< 0 >());
  TEST_ASSERT_EQUAL(1U << 15, indicesToBitmask< 15 >());
}

void test_indicesToBitmask_multiple_indices(void)
{
  constexpr uint16_t mask = indicesToBitmask< 2, 4, 7 >();
  TEST_ASSERT_EQUAL((1U << 2) | (1U << 4) | (1U << 7), mask);
}

void test_indicesToBitmask_consecutive(void)
{
  constexpr uint16_t mask = indicesToBitmask< 3, 4, 5, 6 >();
  TEST_ASSERT_EQUAL(0b01111000, mask);
}

void test_indicesToBitmask_all_bits(void)
{
  constexpr uint16_t mask = indicesToBitmask< 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 >();
  TEST_ASSERT_EQUAL(0xFFFF, mask);
}

// ============================================================================
// Tests for are_pins_valid<>()
// ============================================================================

void test_are_pins_valid_all_valid(void)
{
  TEST_ASSERT_TRUE((are_pins_valid< 2, 7, 10, 13 >()));
  TEST_ASSERT_TRUE((are_pins_valid< 2 >()));
  TEST_ASSERT_TRUE((are_pins_valid< 13 >()));
  TEST_ASSERT_TRUE((are_pins_valid< 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 >()));
}

void test_are_pins_valid_rejects_pin_0(void)
{
  TEST_ASSERT_FALSE((are_pins_valid< 0 >()));
  TEST_ASSERT_FALSE((are_pins_valid< 0, 5 >()));
}

void test_are_pins_valid_rejects_pin_1(void)
{
  TEST_ASSERT_FALSE((are_pins_valid< 1 >()));
  TEST_ASSERT_FALSE((are_pins_valid< 1, 5 >()));
}

void test_are_pins_valid_rejects_pin_14_and_above(void)
{
  TEST_ASSERT_FALSE((are_pins_valid< 14 >()));
  TEST_ASSERT_FALSE((are_pins_valid< 15 >()));
  TEST_ASSERT_FALSE((are_pins_valid< 5, 14 >()));
}

// ============================================================================
// Tests for PinList
// ============================================================================

void test_PinList_default_constructor(void)
{
  constexpr PinList< 4 > list{};
  TEST_ASSERT_EQUAL(0, list.count);
  TEST_ASSERT_EQUAL(0, list.toBitmask());
}

void test_PinList_variadic_constructor(void)
{
  constexpr PinList< 4 > list{ 3, 6, 9 };
  TEST_ASSERT_EQUAL(3, list.count);
  TEST_ASSERT_EQUAL(3, list.pins[0]);
  TEST_ASSERT_EQUAL(6, list.pins[1]);
  TEST_ASSERT_EQUAL(9, list.pins[2]);
}

void test_PinList_single_pin(void)
{
  constexpr PinList< 4 > list{ 7 };
  TEST_ASSERT_EQUAL(1, list.count);
  TEST_ASSERT_EQUAL(7, list.pins[0]);
  TEST_ASSERT_EQUAL(1U << 7, list.toBitmask());
}

void test_PinList_max_pins(void)
{
  constexpr PinList< 4 > list{ 2, 5, 8, 11 };
  TEST_ASSERT_EQUAL(4, list.count);
  TEST_ASSERT_EQUAL((1U << 2) | (1U << 5) | (1U << 8) | (1U << 11), list.toBitmask());
}

void test_PinList_toBitmask(void)
{
  constexpr PinList< 4 > list{ 2, 5, 8 };
  constexpr uint16_t expected = (1U << 2) | (1U << 5) | (1U << 8);
  TEST_ASSERT_EQUAL(expected, list.toBitmask());
}

void test_PinList_from_bitmask(void)
{
  constexpr PinList< 8 > list{ static_cast< uint16_t >(0b10100100) };  // pins 2, 5, 7
  TEST_ASSERT_EQUAL(3, list.count);
  TEST_ASSERT_EQUAL(2, list.pins[0]);
  TEST_ASSERT_EQUAL(5, list.pins[1]);
  TEST_ASSERT_EQUAL(7, list.pins[2]);
  TEST_ASSERT_EQUAL(0b10100100, list.toBitmask());
}

void test_PinList_from_bitmask_empty(void)
{
  constexpr PinList< 4 > list{ static_cast< uint16_t >(0) };
  TEST_ASSERT_EQUAL(0, list.count);
  TEST_ASSERT_EQUAL(0, list.toBitmask());
}

void test_PinList_from_bitmask_full(void)
{
  constexpr PinList< 16 > list{ static_cast< uint16_t >(0xFFFF) };
  TEST_ASSERT_EQUAL(16, list.count);
  TEST_ASSERT_EQUAL(0xFFFF, list.toBitmask());
}

// ============================================================================
// Tests for KeyIndexPair
// ============================================================================

void test_KeyIndexPair_construction(void)
{
  constexpr PinList< 4 > list{ 4, 5, 6 };
  constexpr KeyIndexPair< 4 > pair{ 2, list };

  TEST_ASSERT_EQUAL(2, pair.pin);
}

void test_KeyIndexPair_getBitmask(void)
{
  constexpr PinList< 4 > list{ 4, 5, 6 };
  constexpr KeyIndexPair< 4 > pair{ 2, list };

  TEST_ASSERT_EQUAL((1U << 4) | (1U << 5) | (1U << 6), pair.getBitmask());
}

void test_KeyIndexPair_single_pin(void)
{
  constexpr KeyIndexPair< 4 > pair{ 10, PinList< 4 >{ 7 } };
  TEST_ASSERT_EQUAL(10, pair.pin);
  TEST_ASSERT_EQUAL(1U << 7, pair.getBitmask());
}

void test_KeyIndexPair_from_bitmask(void)
{
  constexpr KeyIndexPair< 8 > pair{ 3, PinList< 8 >{ static_cast< uint16_t >(0b11110000) } };
  TEST_ASSERT_EQUAL(3, pair.pin);
  TEST_ASSERT_EQUAL(0b11110000, pair.getBitmask());
}

// ============================================================================
// Tests for OverridePins
// ============================================================================

void test_OverridePins_single_entry(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 9, { 2, 3, 4 } } }
  };

  TEST_ASSERT_EQUAL(1, pins.size());
  TEST_ASSERT_EQUAL(9, pins.getPin(0));
  TEST_ASSERT_EQUAL((1U << 2) | (1U << 3) | (1U << 4), pins.getBitmask(0));
}

void test_OverridePins_multiple_entries(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 2, { 4, 5 } },
      KeyIndexPair< 4 >{ 3, { 6, 7 } } }
  };

  TEST_ASSERT_EQUAL(2, pins.size());
  TEST_ASSERT_EQUAL(2, pins.getPin(0));
  TEST_ASSERT_EQUAL(3, pins.getPin(1));
  TEST_ASSERT_EQUAL((1U << 4) | (1U << 5), pins.getBitmask(0));
  TEST_ASSERT_EQUAL((1U << 6) | (1U << 7), pins.getBitmask(1));
}

void test_OverridePins_getPin_out_of_bounds(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 10, { 4, 5 } } }
  };

  TEST_ASSERT_EQUAL(0, pins.getPin(1));
  TEST_ASSERT_EQUAL(0, pins.getPin(99));
}

void test_OverridePins_getBitmask_out_of_bounds(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 10, { 4, 5 } } }
  };

  TEST_ASSERT_EQUAL(0, pins.getBitmask(1));
  TEST_ASSERT_EQUAL(0, pins.getBitmask(99));
}

void test_OverridePins_findBitmask_found(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 10, { 4, 5 } },
      KeyIndexPair< 4 >{ 11, { 6, 7 } },
      KeyIndexPair< 4 >{ 12, { 8, 9 } } }
  };

  TEST_ASSERT_EQUAL((1U << 4) | (1U << 5), pins.findBitmask(10));
  TEST_ASSERT_EQUAL((1U << 6) | (1U << 7), pins.findBitmask(11));
  TEST_ASSERT_EQUAL((1U << 8) | (1U << 9), pins.findBitmask(12));
}

void test_OverridePins_findBitmask_not_found(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 10, { 4, 5 } } }
  };

  TEST_ASSERT_EQUAL(0, pins.findBitmask(99));
  TEST_ASSERT_EQUAL(0, pins.findBitmask(0));
}

void test_OverridePins_with_bitmask_constructor(void)
{
  // Use bitmask to specify pins: 0b11100 = pins 2, 3, 4
  constexpr OverridePins pins{
    { KeyIndexPair< 8 >{ 5, static_cast< uint16_t >(0b11100) } }
  };

  TEST_ASSERT_EQUAL(5, pins.getPin(0));
  TEST_ASSERT_EQUAL(0b11100, pins.getBitmask(0));
}

void test_OverridePins_many_entries(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 2 >{ 2, { 5, 6 } },
      KeyIndexPair< 2 >{ 3, { 7, 8 } },
      KeyIndexPair< 2 >{ 4, { 9, 10 } },
      KeyIndexPair< 2 >{ 5, { 11, 12 } } }
  };

  TEST_ASSERT_EQUAL(4, pins.size());
  TEST_ASSERT_EQUAL((1U << 11) | (1U << 12), pins.findBitmask(5));
}

// ============================================================================
// Main
// ============================================================================

int main(void)
{
  UNITY_BEGIN();

  // indicesToBitmask tests
  RUN_TEST(test_indicesToBitmask_single_index);
  RUN_TEST(test_indicesToBitmask_multiple_indices);
  RUN_TEST(test_indicesToBitmask_consecutive);
  RUN_TEST(test_indicesToBitmask_all_bits);

  // are_pins_valid tests
  RUN_TEST(test_are_pins_valid_all_valid);
  RUN_TEST(test_are_pins_valid_rejects_pin_0);
  RUN_TEST(test_are_pins_valid_rejects_pin_1);
  RUN_TEST(test_are_pins_valid_rejects_pin_14_and_above);

  // PinList tests
  RUN_TEST(test_PinList_default_constructor);
  RUN_TEST(test_PinList_variadic_constructor);
  RUN_TEST(test_PinList_single_pin);
  RUN_TEST(test_PinList_max_pins);
  RUN_TEST(test_PinList_toBitmask);
  RUN_TEST(test_PinList_from_bitmask);
  RUN_TEST(test_PinList_from_bitmask_empty);
  RUN_TEST(test_PinList_from_bitmask_full);

  // KeyIndexPair tests
  RUN_TEST(test_KeyIndexPair_construction);
  RUN_TEST(test_KeyIndexPair_getBitmask);
  RUN_TEST(test_KeyIndexPair_single_pin);
  RUN_TEST(test_KeyIndexPair_from_bitmask);

  // OverridePins tests
  RUN_TEST(test_OverridePins_single_entry);
  RUN_TEST(test_OverridePins_multiple_entries);
  RUN_TEST(test_OverridePins_getPin_out_of_bounds);
  RUN_TEST(test_OverridePins_getBitmask_out_of_bounds);
  RUN_TEST(test_OverridePins_findBitmask_found);
  RUN_TEST(test_OverridePins_findBitmask_not_found);
  RUN_TEST(test_OverridePins_with_bitmask_constructor);
  RUN_TEST(test_OverridePins_many_entries);

  return UNITY_END();
}
