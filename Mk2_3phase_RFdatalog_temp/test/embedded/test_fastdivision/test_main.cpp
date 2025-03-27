#include <Arduino.h>
#include <unity.h>

#include "FastDivision.h"  // Include the header file for the functions to test

void setUp(void)
{
  // Set up code here (if needed)
}

void tearDown(void)
{
  // Clean up code here (if needed)
}

// Test for divu5
void test_divu5(void)
{
  // Basic cases
  TEST_ASSERT_EQUAL(1, divu5(5));     // 5 / 5 = 1
  TEST_ASSERT_EQUAL(2, divu5(10));    // 10 / 5 = 2
  TEST_ASSERT_EQUAL(0, divu5(0));     // 0 / 5 = 0
  TEST_ASSERT_EQUAL(19, divu5(95));   // 95 / 5 = 19
  TEST_ASSERT_EQUAL(50, divu5(250));  // 250 / 5 = 50
  TEST_ASSERT_EQUAL(1, divu5(5));     // 5 / 5 = 1

  // Edge cases
  TEST_ASSERT_EQUAL(0, divu5(1));   // 1 / 5 = 0
  TEST_ASSERT_EQUAL(0, divu5(4));   // 4 / 5 = 0
  TEST_ASSERT_EQUAL(1, divu5(6));   // 6 / 5 = 1
  TEST_ASSERT_EQUAL(1, divu5(9));   // 9 / 5 = 1
  TEST_ASSERT_EQUAL(2, divu5(11));  // 11 / 5 = 2

  // Large values
  TEST_ASSERT_EQUAL(200, divu5(1000));     // 1000 / 5 = 200
  TEST_ASSERT_EQUAL(13107, divu5(65535));  // 65535 / 5 = 13107 (max 16-bit unsigned value)

  // Random values
  TEST_ASSERT_EQUAL(50, divu5(250));    // 250 / 5 = 50
  TEST_ASSERT_EQUAL(246, divu5(1234));  // 1234 / 5 = 246
  TEST_ASSERT_EQUAL(85, divu5(425));    // 425 / 5 = 85
}

// Test for divu10
void test_divu10(void)
{
  // Basic cases
  TEST_ASSERT_EQUAL(1, divu10(10));  // 10 / 10 = 1
  TEST_ASSERT_EQUAL(2, divu10(20));  // 20 / 10 = 2
  TEST_ASSERT_EQUAL(0, divu10(0));   // 0 / 10 = 0
  TEST_ASSERT_EQUAL(9, divu10(99));  // 99 / 10 = 9

  // Edge cases
  TEST_ASSERT_EQUAL(0, divu10(1));     // 1 / 10 = 0
  TEST_ASSERT_EQUAL(0, divu10(9));     // 9 / 10 = 0
  TEST_ASSERT_EQUAL(10, divu10(100));  // 100 / 10 = 10
  TEST_ASSERT_EQUAL(11, divu10(110));  // 110 / 10 = 11

  // Large values
  TEST_ASSERT_EQUAL(100, divu10(1000));    // 1000 / 10 = 100
  TEST_ASSERT_EQUAL(6553, divu10(65535));  // 65535 / 10 = 6553 (max 16-bit unsigned value)

  // Random values
  TEST_ASSERT_EQUAL(25, divu10(250));    // 250 / 10 = 25
  TEST_ASSERT_EQUAL(123, divu10(1234));  // 1234 / 10 = 123
  TEST_ASSERT_EQUAL(42, divu10(425));    // 425 / 10 = 42
}

// Test for divu15
void test_divu15(void)
{
  // Basic cases
  TEST_ASSERT_EQUAL(1, divu15(15));    // 15 / 15 = 1
  TEST_ASSERT_EQUAL(2, divu15(30));    // 30 / 15 = 2
  TEST_ASSERT_EQUAL(0, divu15(0));     // 0 / 15 = 0
  TEST_ASSERT_EQUAL(6, divu15(90));    // 90 / 15 = 6
  TEST_ASSERT_EQUAL(10, divu15(150));  // 150 / 15 = 10

  // Edge cases
  TEST_ASSERT_EQUAL(0, divu15(1));   // 1 / 15 = 0
  TEST_ASSERT_EQUAL(0, divu15(14));  // 14 / 15 = 0
  TEST_ASSERT_EQUAL(1, divu15(16));  // 16 / 15 = 1
  TEST_ASSERT_EQUAL(1, divu15(29));  // 29 / 15 = 1
  TEST_ASSERT_EQUAL(2, divu15(31));  // 31 / 15 = 2

  // Large values
  TEST_ASSERT_EQUAL(66, divu15(1000));     // 1000 / 15 = 66
  TEST_ASSERT_EQUAL(4369, divu15(65535));  // 65535 / 15 = 4369 (max 16-bit unsigned value)

  // Random values
  TEST_ASSERT_EQUAL(16, divu15(240));   // 240 / 15 = 16
  TEST_ASSERT_EQUAL(82, divu15(1234));  // 1234 / 15 = 82
  TEST_ASSERT_EQUAL(28, divu15(425));   // 425 / 15 = 28
}

// Test for divu60
void test_divu60(void)
{
  // Basic cases
  TEST_ASSERT_EQUAL(1, divu60(60));    // 60 / 60 = 1
  TEST_ASSERT_EQUAL(2, divu60(120));   // 120 / 60 = 2
  TEST_ASSERT_EQUAL(0, divu60(0));     // 0 / 60 = 0
  TEST_ASSERT_EQUAL(5, divu60(300));   // 300 / 60 = 5
  TEST_ASSERT_EQUAL(10, divu60(600));  // 600 / 60 = 10

  // Edge cases
  TEST_ASSERT_EQUAL(0, divu60(1));    // 1 / 60 = 0
  TEST_ASSERT_EQUAL(0, divu60(59));   // 59 / 60 = 0
  TEST_ASSERT_EQUAL(1, divu60(61));   // 61 / 60 = 1
  TEST_ASSERT_EQUAL(1, divu60(119));  // 119 / 60 = 1
  TEST_ASSERT_EQUAL(2, divu60(121));  // 121 / 60 = 2

  // Large values
  TEST_ASSERT_EQUAL(16, divu60(1000));     // 1000 / 60 = 16
  TEST_ASSERT_EQUAL(1092, divu60(65535));  // 65535 / 60 = 1092 (max 16-bit unsigned value)

  // Random values
  TEST_ASSERT_EQUAL(4, divu60(240));    // 240 / 60 = 4
  TEST_ASSERT_EQUAL(20, divu60(1234));  // 1234 / 60 = 20
  TEST_ASSERT_EQUAL(7, divu60(425));    // 425 / 60 = 7
}

void setup()
{
  delay(1000);    // Wait for Serial to initialize
  UNITY_BEGIN();  // Start Unity test framework
}

uint8_t i = 0;
uint8_t max_blinks = 1;

void loop()
{
  if (i < max_blinks)
  {
    RUN_TEST(test_divu5);
    delay(100);
    RUN_TEST(test_divu10);
    delay(100);
    RUN_TEST(test_divu15);
    delay(100);
    RUN_TEST(test_divu60);
    delay(100);
    ++i;
  }
  else if (i == max_blinks)
  {
    UNITY_END();  // End Unity test framework
  }
}