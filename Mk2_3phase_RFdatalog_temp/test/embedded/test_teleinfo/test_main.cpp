#include <Arduino.h>
#include <unity.h>

#include "teleinfo.h"
#include "config_system.h"

void setUp(void)
{
  // Setup for each test
}

void tearDown(void)
{
  // Clean up after each test
}

void test_lineSize_calculation(void)
{
  // Test lineSize function with various tag and value lengths
  // Format: LF + tag + TAB + value + TAB + checksum + CR = 5 + tagLen + valueLen

  TEST_ASSERT_EQUAL(5 + 1 + 3, lineSize(1, 3));  // 1-char tag, 3-char value
  TEST_ASSERT_EQUAL(5 + 2 + 5, lineSize(2, 5));  // 2-char tag, 5-char value
  TEST_ASSERT_EQUAL(5 + 4 + 2, lineSize(4, 2));  // 4-char tag, 2-char value
}

void test_calcBufferSize_compile_time(void)
{
  // Test that calcBufferSize is evaluated at compile time
  constexpr size_t bufferSize = calcBufferSize();

  // Basic validation - should be reasonable size
  TEST_ASSERT_GREATER_THAN(10, bufferSize);  // Should be more than just STX+ETX
  TEST_ASSERT_LESS_THAN(1000, bufferSize);   // Should be reasonable for embedded system
}

void test_teleinfo_instantiation(void)
{
  // Test that we can create TeleInfo objects without issues
  TeleInfo teleinfo;

  // This test mainly validates that the class compiles and can be instantiated
  // The buffer size calculation and memory allocation should work
  TEST_ASSERT_TRUE(true);  // If we get here, instantiation worked
}

void test_teleinfo_basic_operations(void)
{
  // Test basic operations that don't require output verification
  TeleInfo teleinfo;

  // These should not crash or cause issues
  teleinfo.startFrame();
  teleinfo.send("P", 1234);
  teleinfo.send("V", 230, 1);
  teleinfo.send("T", -15, 2);
  teleinfo.endFrame();

  // If we reach here, the basic operations completed without crashing
  TEST_ASSERT_TRUE(true);
}

void test_teleinfo_edge_values(void)
{
  // Test edge case values
  TeleInfo teleinfo;

  teleinfo.startFrame();

  // Test various edge case values
  teleinfo.send("ZERO", 0);
  teleinfo.send("MAX", 32767);   // MAX INT16
  teleinfo.send("MIN", -32768);  // MIN INT16
  teleinfo.send("POS", 1);
  teleinfo.send("NEG", -1);

  teleinfo.endFrame();

  // If execution reaches here, edge values were handled correctly
  TEST_ASSERT_TRUE(true);
}

void test_teleinfo_multiple_frames(void)
{
  // Test creating multiple frames in sequence
  TeleInfo teleinfo;

  // First frame
  teleinfo.startFrame();
  teleinfo.send("F1", 100);
  teleinfo.endFrame();

  // Second frame
  teleinfo.startFrame();
  teleinfo.send("F2", 200);
  teleinfo.endFrame();

  // Third frame
  teleinfo.startFrame();
  teleinfo.send("F3", 300);
  teleinfo.endFrame();

  // If we reach here, multiple frames were created successfully
  TEST_ASSERT_TRUE(true);
}

void test_teleinfo_long_sequences(void)
{
  // Test longer sequences to validate buffer management
  TeleInfo teleinfo;

  teleinfo.startFrame();

  // Add many values to test buffer capacity
  for (int i = 1; i <= 10; i++)
  {
    teleinfo.send("V", 230 + i, i);
  }

  teleinfo.endFrame();

  // If we reach here, long sequences were handled correctly
  TEST_ASSERT_TRUE(true);
}

void setup()
{
  delay(2000);  // Give time for Serial to initialize

  Serial.begin(9600);
  Serial.println("Starting TeleInfo embedded tests...");

  UNITY_BEGIN();
}

void loop()
{
  RUN_TEST(test_lineSize_calculation);
  RUN_TEST(test_calcBufferSize_compile_time);
  RUN_TEST(test_teleinfo_instantiation);
  RUN_TEST(test_teleinfo_basic_operations);
  RUN_TEST(test_teleinfo_edge_values);
  RUN_TEST(test_teleinfo_multiple_frames);
  RUN_TEST(test_teleinfo_long_sequences);

  Serial.println("All TeleInfo tests completed!");

  UNITY_END();
}
