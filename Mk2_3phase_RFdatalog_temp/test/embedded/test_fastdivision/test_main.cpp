/**
 * @file test_main.cpp
 * @brief Embedded unit tests for FastDivision
 *
 * Tests the AVR assembly fast division functions.
 * Must run on Arduino hardware (uses AVR-specific assembly).
 */

#include <Arduino.h>
#include <unity.h>

#include "FastDivision.h"
#include "FastDivision.cpp"  // Include implementation for linking

void setUp(void)
{
}

void tearDown(void)
{
}

// ============================================================================
// Tests for divu10 (AVR assembly implementation)
// ============================================================================

void test_divu10_basic(void)
{
  TEST_ASSERT_EQUAL(0, divu10(0));     // 0 / 10 = 0
  TEST_ASSERT_EQUAL(1, divu10(10));    // 10 / 10 = 1
  TEST_ASSERT_EQUAL(2, divu10(20));    // 20 / 10 = 2
  TEST_ASSERT_EQUAL(10, divu10(100));  // 100 / 10 = 10
}

void test_divu10_edge_cases(void)
{
  TEST_ASSERT_EQUAL(0, divu10(1));   // 1 / 10 = 0
  TEST_ASSERT_EQUAL(0, divu10(9));   // 9 / 10 = 0
  TEST_ASSERT_EQUAL(1, divu10(11));  // 11 / 10 = 1
  TEST_ASSERT_EQUAL(1, divu10(19));  // 19 / 10 = 1
}

void test_divu10_large_values(void)
{
  TEST_ASSERT_EQUAL(100, divu10(1000));    // 1000 / 10 = 100
  TEST_ASSERT_EQUAL(1000, divu10(10000));  // 10000 / 10 = 1000
  TEST_ASSERT_EQUAL(6553, divu10(65535));  // max uint16_t / 10 = 6553
}

void test_divu10_random_values(void)
{
  TEST_ASSERT_EQUAL(25, divu10(250));    // 250 / 10 = 25
  TEST_ASSERT_EQUAL(123, divu10(1234));  // 1234 / 10 = 123
  TEST_ASSERT_EQUAL(42, divu10(425));    // 425 / 10 = 42
  TEST_ASSERT_EQUAL(99, divu10(999));    // 999 / 10 = 99
}

// ============================================================================
// Tests for divmod10 (AVR assembly implementation)
// ============================================================================

void test_divmod10_basic(void)
{
  uint32_t div;
  uint8_t mod;

  divmod10(0, div, mod);
  TEST_ASSERT_EQUAL(0, div);
  TEST_ASSERT_EQUAL(0, mod);

  divmod10(10, div, mod);
  TEST_ASSERT_EQUAL(1, div);
  TEST_ASSERT_EQUAL(0, mod);

  divmod10(100, div, mod);
  TEST_ASSERT_EQUAL(10, div);
  TEST_ASSERT_EQUAL(0, mod);
}

void test_divmod10_with_remainder(void)
{
  uint32_t div;
  uint8_t mod;

  divmod10(1, div, mod);
  TEST_ASSERT_EQUAL(0, div);
  TEST_ASSERT_EQUAL(1, mod);

  divmod10(9, div, mod);
  TEST_ASSERT_EQUAL(0, div);
  TEST_ASSERT_EQUAL(9, mod);

  divmod10(15, div, mod);
  TEST_ASSERT_EQUAL(1, div);
  TEST_ASSERT_EQUAL(5, mod);

  divmod10(99, div, mod);
  TEST_ASSERT_EQUAL(9, div);
  TEST_ASSERT_EQUAL(9, mod);
}

void test_divmod10_large_values(void)
{
  uint32_t div;
  uint8_t mod;

  divmod10(12345, div, mod);
  TEST_ASSERT_EQUAL(1234, div);
  TEST_ASSERT_EQUAL(5, mod);

  divmod10(1000000, div, mod);
  TEST_ASSERT_EQUAL(100000, div);
  TEST_ASSERT_EQUAL(0, mod);
}

// ============================================================================
// Tests for constexpr shift-based divisions
// ============================================================================

void test_divu8(void)
{
  TEST_ASSERT_EQUAL(0, divu8(0));         // 0 / 8 = 0
  TEST_ASSERT_EQUAL(0, divu8(7));         // 7 / 8 = 0
  TEST_ASSERT_EQUAL(1, divu8(8));         // 8 / 8 = 1
  TEST_ASSERT_EQUAL(1, divu8(15));        // 15 / 8 = 1
  TEST_ASSERT_EQUAL(2, divu8(16));        // 16 / 8 = 2
  TEST_ASSERT_EQUAL(125, divu8(1000));    // 1000 / 8 = 125
  TEST_ASSERT_EQUAL(8191, divu8(65535));  // 65535 / 8 = 8191
}

void test_divu4(void)
{
  TEST_ASSERT_EQUAL(0, divu4(0));          // 0 / 4 = 0
  TEST_ASSERT_EQUAL(0, divu4(3));          // 3 / 4 = 0
  TEST_ASSERT_EQUAL(1, divu4(4));          // 4 / 4 = 1
  TEST_ASSERT_EQUAL(1, divu4(7));          // 7 / 4 = 1
  TEST_ASSERT_EQUAL(2, divu4(8));          // 8 / 4 = 2
  TEST_ASSERT_EQUAL(250, divu4(1000));     // 1000 / 4 = 250
  TEST_ASSERT_EQUAL(16383, divu4(65535));  // 65535 / 4 = 16383
}

void test_divu2(void)
{
  TEST_ASSERT_EQUAL(0, divu2(0));          // 0 / 2 = 0
  TEST_ASSERT_EQUAL(0, divu2(1));          // 1 / 2 = 0
  TEST_ASSERT_EQUAL(1, divu2(2));          // 2 / 2 = 1
  TEST_ASSERT_EQUAL(1, divu2(3));          // 3 / 2 = 1
  TEST_ASSERT_EQUAL(500, divu2(1000));     // 1000 / 2 = 500
  TEST_ASSERT_EQUAL(32767, divu2(65535));  // 65535 / 2 = 32767
}

void test_divu1(void)
{
  TEST_ASSERT_EQUAL(0, divu1(0));
  TEST_ASSERT_EQUAL(1, divu1(1));
  TEST_ASSERT_EQUAL(1000, divu1(1000));
  TEST_ASSERT_EQUAL(65535, divu1(65535));
}

// ============================================================================
// Main
// ============================================================================

void setup()
{
  delay(1000);
  UNITY_BEGIN();
}

void loop()
{
  // divu10 tests
  RUN_TEST(test_divu10_basic);
  RUN_TEST(test_divu10_edge_cases);
  RUN_TEST(test_divu10_large_values);
  RUN_TEST(test_divu10_random_values);

  // divmod10 tests
  RUN_TEST(test_divmod10_basic);
  RUN_TEST(test_divmod10_with_remainder);
  RUN_TEST(test_divmod10_large_values);

  // Shift-based division tests
  RUN_TEST(test_divu8);
  RUN_TEST(test_divu4);
  RUN_TEST(test_divu2);
  RUN_TEST(test_divu1);

  UNITY_END();
}
