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
  TEST_ASSERT_EQUAL(0, list.toLocalBitmask());
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
  TEST_ASSERT_EQUAL(1U << 7, list.toLocalBitmask());
}

void test_PinList_max_pins(void)
{
  constexpr PinList< 4 > list{ 2, 5, 8, 11 };
  TEST_ASSERT_EQUAL(4, list.count);
  TEST_ASSERT_EQUAL((1U << 2) | (1U << 5) | (1U << 8) | (1U << 11), list.toLocalBitmask());
}

void test_PinList_toLocalBitmask(void)
{
  constexpr PinList< 4 > list{ 2, 5, 8 };
  constexpr uint16_t expected = (1U << 2) | (1U << 5) | (1U << 8);
  TEST_ASSERT_EQUAL(expected, list.toLocalBitmask());
}

void test_PinList_from_bitmask(void)
{
  constexpr PinList< 8 > list{ static_cast< uint16_t >(0b10100100) };  // pins 2, 5, 7
  TEST_ASSERT_EQUAL(3, list.count);
  TEST_ASSERT_EQUAL(2, list.pins[0]);
  TEST_ASSERT_EQUAL(5, list.pins[1]);
  TEST_ASSERT_EQUAL(7, list.pins[2]);
  TEST_ASSERT_EQUAL(0b10100100, list.toLocalBitmask());
}

void test_PinList_from_bitmask_empty(void)
{
  constexpr PinList< 4 > list{ static_cast< uint16_t >(0) };
  TEST_ASSERT_EQUAL(0, list.count);
  TEST_ASSERT_EQUAL(0, list.toLocalBitmask());
}

void test_PinList_from_bitmask_full(void)
{
  constexpr PinList< 16 > list{ static_cast< uint16_t >(0xFFFF) };
  TEST_ASSERT_EQUAL(16, list.count);
  TEST_ASSERT_EQUAL(0xFFFF, list.toLocalBitmask());
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

void test_KeyIndexPair_getLocalBitmask(void)
{
  constexpr PinList< 4 > list{ 4, 5, 6 };
  constexpr KeyIndexPair< 4 > pair{ 2, list };

  TEST_ASSERT_EQUAL((1U << 4) | (1U << 5) | (1U << 6), pair.getLocalBitmask());
}

void test_KeyIndexPair_single_pin(void)
{
  constexpr KeyIndexPair< 4 > pair{ 10, PinList< 4 >{ 7 } };
  TEST_ASSERT_EQUAL(10, pair.pin);
  TEST_ASSERT_EQUAL(1U << 7, pair.getLocalBitmask());
}

void test_KeyIndexPair_from_bitmask(void)
{
  constexpr KeyIndexPair< 8 > pair{ 3, PinList< 8 >{ static_cast< uint16_t >(0b11110000) } };
  TEST_ASSERT_EQUAL(3, pair.pin);
  TEST_ASSERT_EQUAL(0b11110000, pair.getLocalBitmask());
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
  TEST_ASSERT_EQUAL((1U << 2) | (1U << 3) | (1U << 4), pins.getLocalBitmask(0));
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
  TEST_ASSERT_EQUAL((1U << 4) | (1U << 5), pins.getLocalBitmask(0));
  TEST_ASSERT_EQUAL((1U << 6) | (1U << 7), pins.getLocalBitmask(1));
}

void test_OverridePins_getPin_out_of_bounds(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 10, { 4, 5 } } }
  };

  TEST_ASSERT_EQUAL(0, pins.getPin(1));
  TEST_ASSERT_EQUAL(0, pins.getPin(99));
}

void test_OverridePins_getLocalBitmask_out_of_bounds(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 10, { 4, 5 } } }
  };

  TEST_ASSERT_EQUAL(0, pins.getLocalBitmask(1));
  TEST_ASSERT_EQUAL(0, pins.getLocalBitmask(99));
}

void test_OverridePins_findLocalBitmask_found(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 10, { 4, 5 } },
      KeyIndexPair< 4 >{ 11, { 6, 7 } },
      KeyIndexPair< 4 >{ 12, { 8, 9 } } }
  };

  TEST_ASSERT_EQUAL((1U << 4) | (1U << 5), pins.findLocalBitmask(10));
  TEST_ASSERT_EQUAL((1U << 6) | (1U << 7), pins.findLocalBitmask(11));
  TEST_ASSERT_EQUAL((1U << 8) | (1U << 9), pins.findLocalBitmask(12));
}

void test_OverridePins_findLocalBitmask_not_found(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 10, { 4, 5 } } }
  };

  TEST_ASSERT_EQUAL(0, pins.findLocalBitmask(99));
  TEST_ASSERT_EQUAL(0, pins.findLocalBitmask(0));
}

void test_OverridePins_with_bitmask_constructor(void)
{
  // Use bitmask to specify pins: 0b11100 = pins 2, 3, 4
  constexpr OverridePins pins{
    { KeyIndexPair< 8 >{ 5, static_cast< uint16_t >(0b11100) } }
  };

  TEST_ASSERT_EQUAL(5, pins.getPin(0));
  TEST_ASSERT_EQUAL(0b11100, pins.getLocalBitmask(0));
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
  TEST_ASSERT_EQUAL((1U << 11) | (1U << 12), pins.findLocalBitmask(5));
}

// ============================================================================
// Tests for Remote Load Support (REMOTE_PIN_BASE = 128)
// ============================================================================

// Helper macro for remote load pins
#define REMOTE_LOAD(n) (REMOTE_PIN_BASE + (n))

void test_PinList_toRemoteBitmask_empty(void)
{
  constexpr PinList< 4 > list{};
  TEST_ASSERT_EQUAL(0, list.toRemoteBitmask());
}

void test_PinList_toRemoteBitmask_local_only(void)
{
  // Local pins should not appear in remote bitmask
  constexpr PinList< 4 > list{ 2, 5, 8 };
  TEST_ASSERT_EQUAL(0, list.toRemoteBitmask());
}

void test_PinList_toRemoteBitmask_single_remote(void)
{
  constexpr PinList< 4 > list{ REMOTE_LOAD(0) };  // Remote load 0 = pin 128
  TEST_ASSERT_EQUAL(1U << 0, list.toRemoteBitmask());
}

void test_PinList_toRemoteBitmask_multiple_remote(void)
{
  constexpr PinList< 4 > list{ REMOTE_LOAD(0), REMOTE_LOAD(2), REMOTE_LOAD(5) };
  TEST_ASSERT_EQUAL((1U << 0) | (1U << 2) | (1U << 5), list.toRemoteBitmask());
}

void test_PinList_toRemoteBitmask_mixed(void)
{
  // Mix of local (5, 8) and remote (0, 3) pins
  constexpr PinList< 6 > list{ 5, REMOTE_LOAD(0), 8, REMOTE_LOAD(3) };
  TEST_ASSERT_EQUAL((1U << 5) | (1U << 8), list.toLocalBitmask());
  TEST_ASSERT_EQUAL((1U << 0) | (1U << 3), list.toRemoteBitmask());
}

void test_PinList_from_uint32_bitmask_local_only(void)
{
  // Lower 16 bits only (local pins 2, 5, 7)
  constexpr uint32_t bitmask = 0b10100100;
  constexpr PinList< 8 > list{ bitmask };
  TEST_ASSERT_EQUAL(3, list.count);
  TEST_ASSERT_EQUAL(0b10100100, list.toLocalBitmask());
  TEST_ASSERT_EQUAL(0, list.toRemoteBitmask());
}

void test_PinList_from_uint32_bitmask_remote_only(void)
{
  // Upper 16 bits only (remote loads 0, 2, 4)
  constexpr uint32_t bitmask = (1UL << 16) | (1UL << 18) | (1UL << 20);
  constexpr PinList< 8 > list{ bitmask };
  TEST_ASSERT_EQUAL(3, list.count);
  TEST_ASSERT_EQUAL(0, list.toLocalBitmask());
  TEST_ASSERT_EQUAL((1U << 0) | (1U << 2) | (1U << 4), list.toRemoteBitmask());
}

void test_PinList_from_uint32_bitmask_mixed(void)
{
  // Local pins 4, 6 + Remote loads 1, 3
  constexpr uint32_t bitmask = (1UL << 4) | (1UL << 6) | (1UL << 17) | (1UL << 19);
  constexpr PinList< 8 > list{ bitmask };
  TEST_ASSERT_EQUAL(4, list.count);
  TEST_ASSERT_EQUAL((1U << 4) | (1U << 6), list.toLocalBitmask());
  TEST_ASSERT_EQUAL((1U << 1) | (1U << 3), list.toRemoteBitmask());
}

// ============================================================================
// Tests for KeyIndexPair with Remote Loads
// ============================================================================

void test_KeyIndexPair_getRemoteBitmask_empty(void)
{
  constexpr PinList< 4 > list{ 4, 5, 6 };  // Local only
  constexpr KeyIndexPair< 4 > pair{ 2, list };
  TEST_ASSERT_EQUAL(0, pair.getRemoteBitmask());
}

void test_KeyIndexPair_getRemoteBitmask_remote_only(void)
{
  constexpr PinList< 4 > list{ REMOTE_LOAD(0), REMOTE_LOAD(2) };
  constexpr KeyIndexPair< 4 > pair{ 5, list };
  TEST_ASSERT_EQUAL(0, pair.getLocalBitmask());
  TEST_ASSERT_EQUAL((1U << 0) | (1U << 2), pair.getRemoteBitmask());
}

void test_KeyIndexPair_getRemoteBitmask_mixed(void)
{
  constexpr PinList< 6 > list{ 4, 5, REMOTE_LOAD(1), REMOTE_LOAD(3) };
  constexpr KeyIndexPair< 6 > pair{ 10, list };
  TEST_ASSERT_EQUAL((1U << 4) | (1U << 5), pair.getLocalBitmask());
  TEST_ASSERT_EQUAL((1U << 1) | (1U << 3), pair.getRemoteBitmask());
}

// ============================================================================
// Tests for OverridePins with Remote Loads
// ============================================================================

void test_OverridePins_getRemoteBitmask_local_only(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 9, { 2, 3, 4 } } }
  };
  TEST_ASSERT_EQUAL(0, pins.getRemoteBitmask(0));
}

void test_OverridePins_getRemoteBitmask_remote_only(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 9, { REMOTE_LOAD(0), REMOTE_LOAD(2) } } }
  };
  TEST_ASSERT_EQUAL(0, pins.getLocalBitmask(0));
  TEST_ASSERT_EQUAL((1U << 0) | (1U << 2), pins.getRemoteBitmask(0));
}

void test_OverridePins_getRemoteBitmask_mixed(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 6 >{ 9, { 4, 5, REMOTE_LOAD(1), REMOTE_LOAD(3) } } }
  };
  TEST_ASSERT_EQUAL((1U << 4) | (1U << 5), pins.getLocalBitmask(0));
  TEST_ASSERT_EQUAL((1U << 1) | (1U << 3), pins.getRemoteBitmask(0));
}

void test_OverridePins_getRemoteBitmask_out_of_bounds(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 10, { REMOTE_LOAD(2) } } }
  };
  TEST_ASSERT_EQUAL(0, pins.getRemoteBitmask(1));
  TEST_ASSERT_EQUAL(0, pins.getRemoteBitmask(99));
}

void test_OverridePins_findRemoteBitmask_found(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 10, { REMOTE_LOAD(0), REMOTE_LOAD(1) } },
      KeyIndexPair< 4 >{ 11, { REMOTE_LOAD(2), REMOTE_LOAD(3) } },
      KeyIndexPair< 4 >{ 12, { REMOTE_LOAD(4), REMOTE_LOAD(5) } } }
  };
  TEST_ASSERT_EQUAL((1U << 0) | (1U << 1), pins.findRemoteBitmask(10));
  TEST_ASSERT_EQUAL((1U << 2) | (1U << 3), pins.findRemoteBitmask(11));
  TEST_ASSERT_EQUAL((1U << 4) | (1U << 5), pins.findRemoteBitmask(12));
}

void test_OverridePins_findRemoteBitmask_not_found(void)
{
  constexpr OverridePins pins{
    { KeyIndexPair< 4 >{ 10, { REMOTE_LOAD(0) } } }
  };
  TEST_ASSERT_EQUAL(0, pins.findRemoteBitmask(99));
  TEST_ASSERT_EQUAL(0, pins.findRemoteBitmask(0));
}

void test_OverridePins_multiple_entries_mixed(void)
{
  // Real-world scenario: some override pins control local loads, some control remote
  constexpr OverridePins pins{
    { KeyIndexPair< 6 >{ 2, { 4, 5 } },                            // Local only
      KeyIndexPair< 6 >{ 3, { REMOTE_LOAD(0), REMOTE_LOAD(1) } },  // Remote only
      KeyIndexPair< 6 >{ 4, { 6, 7, REMOTE_LOAD(2) } } }           // Mixed
  };

  TEST_ASSERT_EQUAL(3, pins.size());

  // Entry 0: local only
  TEST_ASSERT_EQUAL((1U << 4) | (1U << 5), pins.getLocalBitmask(0));
  TEST_ASSERT_EQUAL(0, pins.getRemoteBitmask(0));

  // Entry 1: remote only
  TEST_ASSERT_EQUAL(0, pins.getLocalBitmask(1));
  TEST_ASSERT_EQUAL((1U << 0) | (1U << 1), pins.getRemoteBitmask(1));

  // Entry 2: mixed
  TEST_ASSERT_EQUAL((1U << 6) | (1U << 7), pins.getLocalBitmask(2));
  TEST_ASSERT_EQUAL(1U << 2, pins.getRemoteBitmask(2));

  // Find by pin
  TEST_ASSERT_EQUAL((1U << 4) | (1U << 5), pins.findLocalBitmask(2));
  TEST_ASSERT_EQUAL(0, pins.findRemoteBitmask(2));
  TEST_ASSERT_EQUAL(0, pins.findLocalBitmask(3));
  TEST_ASSERT_EQUAL((1U << 0) | (1U << 1), pins.findRemoteBitmask(3));
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
  RUN_TEST(test_PinList_toLocalBitmask);
  RUN_TEST(test_PinList_from_bitmask);
  RUN_TEST(test_PinList_from_bitmask_empty);
  RUN_TEST(test_PinList_from_bitmask_full);

  // KeyIndexPair tests
  RUN_TEST(test_KeyIndexPair_construction);
  RUN_TEST(test_KeyIndexPair_getLocalBitmask);
  RUN_TEST(test_KeyIndexPair_single_pin);
  RUN_TEST(test_KeyIndexPair_from_bitmask);

  // OverridePins tests
  RUN_TEST(test_OverridePins_single_entry);
  RUN_TEST(test_OverridePins_multiple_entries);
  RUN_TEST(test_OverridePins_getPin_out_of_bounds);
  RUN_TEST(test_OverridePins_getLocalBitmask_out_of_bounds);
  RUN_TEST(test_OverridePins_findLocalBitmask_found);
  RUN_TEST(test_OverridePins_findLocalBitmask_not_found);
  RUN_TEST(test_OverridePins_with_bitmask_constructor);
  RUN_TEST(test_OverridePins_many_entries);

  // PinList remote bitmask tests
  RUN_TEST(test_PinList_toRemoteBitmask_empty);
  RUN_TEST(test_PinList_toRemoteBitmask_local_only);
  RUN_TEST(test_PinList_toRemoteBitmask_single_remote);
  RUN_TEST(test_PinList_toRemoteBitmask_multiple_remote);
  RUN_TEST(test_PinList_toRemoteBitmask_mixed);
  RUN_TEST(test_PinList_from_uint32_bitmask_local_only);
  RUN_TEST(test_PinList_from_uint32_bitmask_remote_only);
  RUN_TEST(test_PinList_from_uint32_bitmask_mixed);

  // KeyIndexPair remote bitmask tests
  RUN_TEST(test_KeyIndexPair_getRemoteBitmask_empty);
  RUN_TEST(test_KeyIndexPair_getRemoteBitmask_remote_only);
  RUN_TEST(test_KeyIndexPair_getRemoteBitmask_mixed);

  // OverridePins remote bitmask tests
  RUN_TEST(test_OverridePins_getRemoteBitmask_local_only);
  RUN_TEST(test_OverridePins_getRemoteBitmask_remote_only);
  RUN_TEST(test_OverridePins_getRemoteBitmask_mixed);
  RUN_TEST(test_OverridePins_getRemoteBitmask_out_of_bounds);
  RUN_TEST(test_OverridePins_findRemoteBitmask_found);
  RUN_TEST(test_OverridePins_findRemoteBitmask_not_found);
  RUN_TEST(test_OverridePins_multiple_entries_mixed);

  return UNITY_END();
}
