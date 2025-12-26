/**
 * @file test_main.cpp
 * @brief Unity-based unit tests for assembly multiplication functions
 * @version 0.1
 * @date 2025-10-09
 *
 * This file contains comprehensive unit tests for the assembly-optimized
 * multiplication functions using the Unity testing framework.
 * 
 * Based on:
 * - florentbr's optimization suggestions for PVRouter
 * - avrfreertos multiplication optimizations by feilipu
 * - OpenMusicLabs AVR assembly techniques
 */

#include <Arduino.h>
#include <unity.h>

#include "mult_asm.h"

// Global volatile to prevent optimization without affecting timing
volatile uint8_t g_optimizer_defeat = 0;

void setUp(void)
{
  // Set up code here (if needed)
}

void tearDown(void)
{
  // Clean up code here (if needed)
}

/**
 * @brief Test basic functionality of multS16x16_to32
 */
void test_multS16x16_to32_basic(void)
{
  int32_t result;

  // Test positive × positive
  multS16x16_to32(result, 2, 3);
  TEST_ASSERT_EQUAL(6, result);

  multS16x16_to32(result, 100, 100);
  TEST_ASSERT_EQUAL(10000, result);

  // Test positive × negative
  multS16x16_to32(result, 2, -3);
  TEST_ASSERT_EQUAL(-6, result);

  multS16x16_to32(result, 100, -100);
  TEST_ASSERT_EQUAL(-10000, result);

  // Test negative × negative
  multS16x16_to32(result, -2, -3);
  TEST_ASSERT_EQUAL(6, result);

  // Test edge cases
  multS16x16_to32(result, 0, 1000);
  TEST_ASSERT_EQUAL(0, result);

  multS16x16_to32(result, 1000, 0);
  TEST_ASSERT_EQUAL(0, result);

  // Test maximum values (be careful of overflow)
  multS16x16_to32(result, 32767, 1);
  TEST_ASSERT_EQUAL(32767, result);

  multS16x16_to32(result, -32768, 1);
  TEST_ASSERT_EQUAL(-32768, result);

  // Test larger multiplications
  multS16x16_to32(result, 1000, 1000);
  TEST_ASSERT_EQUAL(1000000, result);

  multS16x16_to32(result, -1000, 1000);
  TEST_ASSERT_EQUAL(-1000000, result);
}

/**
 * @brief Test basic functionality of multU16x16_to32 (unsigned)
 */
void test_multU16x16_to32_basic(void)
{
  uint32_t result;

  // Test basic unsigned multiplication
  multU16x16_to32(result, 2, 3);
  TEST_ASSERT_EQUAL_UINT32(6, result);

  multU16x16_to32(result, 100, 100);
  TEST_ASSERT_EQUAL_UINT32(10000, result);

  multU16x16_to32(result, 1000, 1000);
  TEST_ASSERT_EQUAL_UINT32(1000000, result);

  // Test edge cases
  multU16x16_to32(result, 0, 1000);
  TEST_ASSERT_EQUAL_UINT32(0, result);

  multU16x16_to32(result, 1000, 0);
  TEST_ASSERT_EQUAL_UINT32(0, result);

  multU16x16_to32(result, 1, 65535);
  TEST_ASSERT_EQUAL_UINT32(65535, result);

  multU16x16_to32(result, 65535, 1);
  TEST_ASSERT_EQUAL_UINT32(65535, result);

  // Test maximum values (65535 * 65535)
  multU16x16_to32(result, 65535, 65535);
  TEST_ASSERT_EQUAL_UINT32(4294836225UL, result);  // 65535² = 4,294,836,225

  // Test typical ADC values (like voltage sample squared for V²)
  multU16x16_to32(result, 32768, 32768);
  TEST_ASSERT_EQUAL_UINT32(1073741824UL, result);  // 32768² = 1,073,741,824

  multU16x16_to32(result, 1648, 1648);
  TEST_ASSERT_EQUAL_UINT32(2715904, result);  // Typical V sample squared

  // Test various powers of 2
  multU16x16_to32(result, 256, 256);
  TEST_ASSERT_EQUAL_UINT32(65536, result);

  multU16x16_to32(result, 512, 512);
  TEST_ASSERT_EQUAL_UINT32(262144, result);

  multU16x16_to32(result, 1024, 1024);
  TEST_ASSERT_EQUAL_UINT32(1048576, result);
}

/**
 * @brief Test basic functionality of mult16x8_q8
 */
void test_mult16x8_q8_basic(void)
{
  int16_t result;

  // Test Q8 fractional values
  uint8_t half = float_to_q8(0.5f);             // 128
  uint8_t quarter = float_to_q8(0.25f);         // 64
  uint8_t full = float_to_q8(1.0f);             // 255 (close to 1.0)
  uint8_t three_quarters = float_to_q8(0.75f);  // 192

  // Test basic fractions
  mult16x8_q8(result, 100, half);
  TEST_ASSERT_EQUAL(50, result);  // 100 * 0.5 = 50

  mult16x8_q8(result, 100, quarter);
  TEST_ASSERT_EQUAL(25, result);  // 100 * 0.25 = 25

  mult16x8_q8(result, 100, three_quarters);
  TEST_ASSERT_EQUAL(75, result);  // 100 * 0.75 = 75

  mult16x8_q8(result, 100, full);
  TEST_ASSERT_EQUAL(100, result);  // 100 * ~1.0 ≈ 100

  // Test negative values
  mult16x8_q8(result, -100, half);
  TEST_ASSERT_EQUAL(-50, result);

  mult16x8_q8(result, -100, quarter);
  TEST_ASSERT_EQUAL(-25, result);

  // Test edge cases
  mult16x8_q8(result, 100, 0);
  TEST_ASSERT_EQUAL(0, result);

  mult16x8_q8(result, 0, half);
  TEST_ASSERT_EQUAL(0, result);

  // Test maximum values
  mult16x8_q8(result, 32767, half);
  TEST_ASSERT_EQUAL(16384, result);  // 32767 * 0.5 ≈ 16383

  mult16x8_q8(result, -32768, half);
  TEST_ASSERT_EQUAL(-16384, result);  // -32768 * 0.5 = -16384
}

/**
 * @brief Test Q8 format conversion helpers
 */
void test_q8_conversion_helpers(void)
{
  // Test float_to_q8 conversion
  TEST_ASSERT_EQUAL(0, float_to_q8(0.0f));
  TEST_ASSERT_EQUAL(64, float_to_q8(0.25f));
  TEST_ASSERT_EQUAL(128, float_to_q8(0.5f));
  TEST_ASSERT_EQUAL(192, float_to_q8(0.75f));
  TEST_ASSERT_EQUAL(255, float_to_q8(1.0f));

  // Test q8_to_float conversion (approximate due to floating-point precision)
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, q8_to_float(0));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.25f, q8_to_float(64));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, q8_to_float(128));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.75f, q8_to_float(192));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.996f, q8_to_float(255));  // 255/256 ≈ 0.996
}

/**
 * @brief Test rounding behavior of mult16x8_q8
 */
void test_mult16x8_q8_rounding(void)
{
  int16_t result;

  // Test rounding with values that should round up
  // 100 * (127/256) = 49.609... should round to 50
  mult16x8_q8(result, 100, 127);
  TEST_ASSERT_EQUAL(50, result);

  // 100 * (129/256) = 50.390... should round to 50
  mult16x8_q8(result, 100, 129);
  TEST_ASSERT_EQUAL(50, result);

  // Test exact values
  mult16x8_q8(result, 256, 128);  // 256 * 0.5 = 128
  TEST_ASSERT_EQUAL(128, result);

  mult16x8_q8(result, 512, 64);  // 512 * 0.25 = 128
  TEST_ASSERT_EQUAL(128, result);
}

/**
 * @brief Test edge cases and potential overflow scenarios
 */
void test_edge_cases(void)
{
  int32_t result32;
  int16_t result16;

  // Test multS16x16_to32 with values near 16-bit limits
  multS16x16_to32(result32, 32767, 2);
  TEST_ASSERT_EQUAL(65534, result32);

  multS16x16_to32(result32, -32768, 2);
  TEST_ASSERT_EQUAL(-65536, result32);

  multS16x16_to32(result32, -32640, 257);
  TEST_ASSERT_EQUAL(-8388480, result32);

  // Test maximum positive * maximum positive (largest positive result)
  multS16x16_to32(result32, 32767, 32767);
  TEST_ASSERT_EQUAL(1073676289L, result32);  // 32767² = 1,073,676,289

  // Test maximum negative * maximum negative (largest positive result)
  multS16x16_to32(result32, -32768, -32768);
  TEST_ASSERT_EQUAL(1073741824L, result32);  // (-32768)² = 1,073,741,824

  // Test maximum positive * maximum negative (most negative result)
  multS16x16_to32(result32, 32767, -32768);
  TEST_ASSERT_EQUAL(-1073709056L, result32);  // 32767 * (-32768) = -1,073,709,056

  // Test maximum negative * maximum positive (same as above)
  multS16x16_to32(result32, -32768, 32767);
  TEST_ASSERT_EQUAL(-1073709056L, result32);

  // Test one value at limit, other small
  multS16x16_to32(result32, 32767, -1);
  TEST_ASSERT_EQUAL(-32767, result32);

  multS16x16_to32(result32, -32768, -1);
  TEST_ASSERT_EQUAL(32768, result32);

  // Test typical ADC range values (like in PVRouter ISR)
  multS16x16_to32(result32, 1648, 512);  // Typical voltage * current
  TEST_ASSERT_EQUAL(843776, result32);

  multS16x16_to32(result32, -1648, 512);  // Negative voltage
  TEST_ASSERT_EQUAL(-843776, result32);

  multS16x16_to32(result32, 1648, -512);  // Negative current
  TEST_ASSERT_EQUAL(-843776, result32);

  // Test mult16x8_q8 with extreme values
  mult16x8_q8(result16, 32767, 255);   // Maximum positive * maximum fraction
  TEST_ASSERT_EQUAL(32639, result16);  // Should be close to 32767

  mult16x8_q8(result16, -32768, 255);   // Maximum negative * maximum fraction
  TEST_ASSERT_EQUAL(-32640, result16);  // Should be close to -32768

  // Test boundary values with different fractions
  mult16x8_q8(result16, 32767, 1);   // Maximum positive * minimum fraction
  TEST_ASSERT_EQUAL(128, result16);  // 32767 * (1/256) ≈ 128

  mult16x8_q8(result16, -32768, 1);   // Maximum negative * minimum fraction
  TEST_ASSERT_EQUAL(-128, result16);  // -32768 * (1/256) = -128

  mult16x8_q8(result16, 32767, 128);   // Maximum positive * 0.5
  TEST_ASSERT_EQUAL(16384, result16);  // 32767 * 0.5 ≈ 16384

  mult16x8_q8(result16, -32768, 128);   // Maximum negative * 0.5
  TEST_ASSERT_EQUAL(-16384, result16);  // -32768 * 0.5 = -16384

  // Test zero multiplication edge cases
  mult16x8_q8(result16, 32767, 0);  // Maximum positive * zero
  TEST_ASSERT_EQUAL(0, result16);

  mult16x8_q8(result16, -32768, 0);  // Maximum negative * zero
  TEST_ASSERT_EQUAL(0, result16);

  mult16x8_q8(result16, 0, 255);  // Zero * maximum fraction
  TEST_ASSERT_EQUAL(0, result16);

  // Test very small fractions with different values
  mult16x8_q8(result16, 1000, 1);  // 1000 * (1/256) ≈ 4
  TEST_ASSERT_EQUAL(4, result16);

  mult16x8_q8(result16, 1000, 2);  // 1000 * (2/256) ≈ 8
  TEST_ASSERT_EQUAL(8, result16);

  mult16x8_q8(result16, 256, 1);  // 256 * (1/256) = 1
  TEST_ASSERT_EQUAL(1, result16);

  mult16x8_q8(result16, 128, 1);  // 128 * (1/256) = 0.5 → rounds to 1
  TEST_ASSERT_EQUAL(1, result16);

  mult16x8_q8(result16, 127, 1);  // 127 * (1/256) = 0.496... → rounds to 0
  TEST_ASSERT_EQUAL(0, result16);

  // Test typical filter values (like in PVRouter)
  mult16x8_q8(result16, 1000, float_to_q8(0.004f));  // Typical EWMA factor
  TEST_ASSERT_EQUAL(4, result16);                    // 1000 * 0.004 = 4

  mult16x8_q8(result16, -500, float_to_q8(0.004f));  // Negative with small factor
  TEST_ASSERT_EQUAL(-2, result16);                   // -500 * 0.004 = -2

  // Test near-overflow scenarios (values that could cause intermediate overflow)
  mult16x8_q8(result16, 30000, 200);   // Large value * large fraction
  TEST_ASSERT_EQUAL(23438, result16);  // 30000 * (200/256) = 23437.5 → 23438 (rounds up)

  mult16x8_q8(result16, -30000, 200);   // Large negative * large fraction
  TEST_ASSERT_EQUAL(-23437, result16);  // -30000 * (200/256) = -23437.5 → -23438
}

/**
 * @brief Compare assembly results with standard multiplication
 */
void test_assembly_vs_standard(void)
{
  int32_t asm_result, std_result;
  uint32_t asm_result_u, std_result_u;
  int16_t asm_result16, std_result16;

  // Test values for signed multiplication
  int16_t test_vals[] = { 100, -200, 1000, -1500, 32767, -32768 };
  uint8_t test_fracs[] = { 64, 128, 192, 255 };  // 0.25, 0.5, 0.75, ~1.0

  // Compare multS16x16_to32 results
  for (uint8_t i = 0; i < 6; i++)
  {
    for (uint8_t j = 0; j < 6; j++)
    {
      multS16x16_to32(asm_result, test_vals[i], test_vals[j]);
      std_result = (int32_t)test_vals[i] * test_vals[j];
      TEST_ASSERT_EQUAL(std_result, asm_result);
    }
  }

  // Test values for unsigned multiplication
  uint16_t test_vals_u[] = { 0, 1, 100, 1000, 32767, 32768, 65535 };
  const uint8_t num_vals_u = sizeof(test_vals_u) / sizeof(test_vals_u[0]);

  // Compare multU16x16_to32 results
  for (uint8_t i = 0; i < num_vals_u; i++)
  {
    for (uint8_t j = 0; j < num_vals_u; j++)
    {
      multU16x16_to32(asm_result_u, test_vals_u[i], test_vals_u[j]);
      std_result_u = (uint32_t)test_vals_u[i] * test_vals_u[j];
      TEST_ASSERT_EQUAL_UINT32(std_result_u, asm_result_u);
    }
  }

  // Compare mult16x8_q8 results
  for (uint8_t i = 0; i < 6; i++)
  {
    for (uint8_t j = 0; j < 4; j++)
    {
      mult16x8_q8(asm_result16, test_vals[i], test_fracs[j]);
      std_result16 = ((int32_t)test_vals[i] * test_fracs[j] + 0x80) >> 8;
      TEST_ASSERT_EQUAL(std_result16, asm_result16);
    }
  }
}

/**
 * @brief Performance comparison between assembly and standard multiplication
 */
void test_performance_multS16x16_to32(void)
{
  const uint16_t iterations = 1000;
  int32_t result;

  // Test data - use various values to prevent compiler optimizations
  int16_t test_vals[] = { 100, -200, 1000, -1500, 32767, -32768 };
  const uint8_t num_vals = sizeof(test_vals) / sizeof(test_vals[0]);

  // Use volatile pointer to force storage without arithmetic overhead
  volatile int32_t* volatile_result = &result;

  // Warm up
  for (uint8_t k = 0; k < 10; k++)
  {
    multS16x16_to32(result, test_vals[k % num_vals], test_vals[(k + 1) % num_vals]);
  }

  // Performance test for assembly multiplication
  unsigned long start_time = micros();

  for (uint16_t i = 0; i < iterations; i++)
  {
    for (uint8_t j = 0; j < num_vals; j++)
    {
      for (uint8_t k = 0; k < num_vals; k++)
      {
        multS16x16_to32(result, test_vals[j], test_vals[k]);
        *volatile_result = result;  // Force storage
      }
    }
  }

  unsigned long asm_time = micros() - start_time;

  // Warm up standard multiplication
  for (uint8_t k = 0; k < 10; k++)
  {
    result = (int32_t)test_vals[k % num_vals] * test_vals[(k + 1) % num_vals];
  }

  // Performance test for standard multiplication
  start_time = micros();

  for (uint16_t i = 0; i < iterations; i++)
  {
    for (uint8_t j = 0; j < num_vals; j++)
    {
      for (uint8_t k = 0; k < num_vals; k++)
      {
        result = (int32_t)test_vals[j] * test_vals[k];
        *volatile_result = result;  // Force storage
      }
    }
  }

  unsigned long std_time = micros() - start_time;

  // Calculate total operations
  uint32_t total_ops = (uint32_t)iterations * num_vals * num_vals;

  // Report results
  Serial.println(F("--- multS16x16_to32 Performance Results ---"));
  Serial.print(F("Operations: "));
  Serial.println(total_ops);
  Serial.print(F("Assembly time: "));
  Serial.print(asm_time);
  Serial.println(F(" µs"));
  Serial.print(F("Standard time: "));
  Serial.print(std_time);
  Serial.println(F(" µs"));

  if (asm_time > 0 && std_time > 0)
  {
    Serial.print(F("Assembly ops/µs: "));
    Serial.println((float)total_ops / asm_time, 2);
    Serial.print(F("Standard ops/µs: "));
    Serial.println((float)total_ops / std_time, 2);

    if (asm_time < std_time)
    {
      Serial.print(F("✓ Assembly is "));
      Serial.print((float)std_time / asm_time, 2);
      Serial.println(F("x faster"));
    }
    else
    {
      Serial.print(F("⚠ Standard is "));
      Serial.print((float)asm_time / std_time, 2);
      Serial.println(F("x faster"));
    }
  }

  // The test passes if assembly is reasonably competitive with standard
  // Function call overhead may make assembly slower in micro-benchmarks
  // but should be faster in real-world usage (ISR context)
  TEST_ASSERT_TRUE(asm_time <= std_time * 10);  // Allow 10x slower for function overhead
}

/**
 * @brief Performance comparison for unsigned multiplication (multU16x16_to32)
 */
void test_performance_multU16x16_to32(void)
{
  const uint16_t iterations = 1000;
  uint32_t result;

  // Test data - unsigned values typical for ADC and V² calculations
  uint16_t test_vals[] = { 0, 100, 1000, 16384, 32768, 49152, 65535 };
  const uint8_t num_vals = sizeof(test_vals) / sizeof(test_vals[0]);

  // Use volatile pointer to force storage without arithmetic overhead
  volatile uint32_t* volatile_result = &result;

  // Warm up
  for (uint8_t k = 0; k < 10; k++)
  {
    multU16x16_to32(result, test_vals[k % num_vals], test_vals[(k + 1) % num_vals]);
  }

  // Performance test for assembly multiplication
  unsigned long start_time = micros();

  for (uint16_t i = 0; i < iterations; i++)
  {
    for (uint8_t j = 0; j < num_vals; j++)
    {
      for (uint8_t k = 0; k < num_vals; k++)
      {
        multU16x16_to32(result, test_vals[j], test_vals[k]);
        *volatile_result = result;  // Force storage
      }
    }
  }

  unsigned long asm_time = micros() - start_time;

  // Warm up standard multiplication
  for (uint8_t k = 0; k < 10; k++)
  {
    result = (uint32_t)test_vals[k % num_vals] * test_vals[(k + 1) % num_vals];
  }

  // Performance test for standard multiplication
  start_time = micros();

  for (uint16_t i = 0; i < iterations; i++)
  {
    for (uint8_t j = 0; j < num_vals; j++)
    {
      for (uint8_t k = 0; k < num_vals; k++)
      {
        result = (uint32_t)test_vals[j] * test_vals[k];
        *volatile_result = result;  // Force storage
      }
    }
  }

  unsigned long std_time = micros() - start_time;

  // Calculate total operations
  uint32_t total_ops = (uint32_t)iterations * num_vals * num_vals;

  // Report results
  Serial.println(F("--- multU16x16_to32 Performance Results ---"));
  Serial.print(F("Operations: "));
  Serial.println(total_ops);
  Serial.print(F("Assembly time: "));
  Serial.print(asm_time);
  Serial.println(F(" µs"));
  Serial.print(F("Standard time: "));
  Serial.print(std_time);
  Serial.println(F(" µs"));

  if (asm_time > 0 && std_time > 0)
  {
    Serial.print(F("Assembly ops/µs: "));
    Serial.println((float)total_ops / asm_time, 2);
    Serial.print(F("Standard ops/µs: "));
    Serial.println((float)total_ops / std_time, 2);

    if (asm_time < std_time)
    {
      Serial.print(F("✓ Assembly is "));
      Serial.print((float)std_time / asm_time, 2);
      Serial.println(F("x faster"));
    }
    else
    {
      Serial.print(F("⚠ Standard is "));
      Serial.print((float)asm_time / std_time, 2);
      Serial.println(F("x faster"));
    }
  }

  // The test passes if assembly is reasonably competitive with standard
  TEST_ASSERT_TRUE(asm_time <= std_time * 10);  // Allow 10x slower for function overhead
}

/**
 * @brief Performance comparison for Q8 fractional multiplication
 */
void test_performance_mult16x8_q8(void)
{
  const uint16_t iterations = 1000;
  int16_t result;

  // Test data
  int16_t test_vals[] = { 100, -200, 1000, -1500, 32767, -32768 };
  uint8_t test_fracs[] = { 32, 64, 96, 128, 160, 192, 224, 255 };  // Various Q8 fractions
  const uint8_t num_vals = sizeof(test_vals) / sizeof(test_vals[0]);
  const uint8_t num_fracs = sizeof(test_fracs) / sizeof(test_fracs[0]);

  // Use volatile pointer to force storage without arithmetic overhead
  volatile int16_t* volatile_result = &result;

  // Warm up
  for (uint8_t k = 0; k < 10; k++)
  {
    mult16x8_q8(result, test_vals[k % num_vals], test_fracs[k % num_fracs]);
  }

  // Performance test for assembly Q8 multiplication
  unsigned long start_time = micros();

  for (uint16_t i = 0; i < iterations; i++)
  {
    for (uint8_t j = 0; j < num_vals; j++)
    {
      for (uint8_t k = 0; k < num_fracs; k++)
      {
        mult16x8_q8(result, test_vals[j], test_fracs[k]);
        *volatile_result = result;  // Force storage
      }
    }
  }

  unsigned long asm_time = micros() - start_time;

  // Warm up standard Q8 multiplication
  for (uint8_t k = 0; k < 10; k++)
  {
    result = ((int32_t)test_vals[k % num_vals] * test_fracs[k % num_fracs] + 0x80) >> 8;
  }

  // Performance test for standard Q8 multiplication
  start_time = micros();

  for (uint16_t i = 0; i < iterations; i++)
  {
    for (uint8_t j = 0; j < num_vals; j++)
    {
      for (uint8_t k = 0; k < num_fracs; k++)
      {
        result = ((int32_t)test_vals[j] * test_fracs[k] + 0x80) >> 8;
        *volatile_result = result;  // Force storage
      }
    }
  }

  unsigned long std_time = micros() - start_time;

  // Calculate total operations
  uint32_t total_ops = (uint32_t)iterations * num_vals * num_fracs;

  // Report results
  Serial.println(F("--- mult16x8_q8 Performance Results ---"));
  Serial.print(F("Operations: "));
  Serial.println(total_ops);
  Serial.print(F("Assembly time: "));
  Serial.print(asm_time);
  Serial.println(F(" µs"));
  Serial.print(F("Standard time: "));
  Serial.print(std_time);
  Serial.println(F(" µs"));

  if (asm_time > 0 && std_time > 0)
  {
    Serial.print(F("Assembly ops/µs: "));
    Serial.println((float)total_ops / asm_time, 2);
    Serial.print(F("Standard ops/µs: "));
    Serial.println((float)total_ops / std_time, 2);

    if (asm_time < std_time)
    {
      Serial.print(F("✓ Assembly is "));
      Serial.print((float)std_time / asm_time, 2);
      Serial.println(F("x faster"));
    }
    else
    {
      Serial.print(F("⚠ Standard is "));
      Serial.print((float)asm_time / std_time, 2);
      Serial.println(F("x faster"));
    }
  }

  // The test passes if assembly is reasonably competitive with standard
  // Function call overhead may make assembly slower in micro-benchmarks
  // but should be faster in real-world usage (ISR context)
  TEST_ASSERT_TRUE(asm_time <= std_time * 10);  // Allow 10x slower for function overhead
}

/**
 * @brief ISR-like performance test simulating real-world usage
 */
void test_performance_isr_simulation(void)
{
  const uint16_t samples = 1000;
  volatile int32_t power_sum = 0;
  volatile int32_t voltage_squared_sum = 0;
  volatile int16_t filtered_current = 0;

  // Simulate typical ISR values
  int16_t voltage_samples[] = { 1650, 1648, 1645, 1640, 1630, 1615, 1595, 1570 };
  int16_t current_samples[] = { 512, 510, 505, 498, 485, 468, 445, 415 };
  int16_t prev_current[] = { 515, 512, 508, 500, 488, 470, 448, 420 };
  uint8_t filter_factor = float_to_q8(0.004f);  // Typical EWMA factor

  const uint8_t num_samples = sizeof(voltage_samples) / sizeof(voltage_samples[0]);

  // Warm up
  for (uint8_t k = 0; k < 10; k++)
  {
    int32_t power, vsquared;
    int16_t filter_delta;
    multS16x16_to32(power, voltage_samples[k % num_samples], current_samples[k % num_samples]);
    multS16x16_to32(vsquared, voltage_samples[k % num_samples], voltage_samples[k % num_samples]);
    mult16x8_q8(filter_delta, prev_current[k % num_samples] - current_samples[k % num_samples], filter_factor);
  }

  // Performance test for assembly-optimized ISR simulation
  unsigned long start_time = micros();

  for (uint16_t i = 0; i < samples; i++)
  {
    uint8_t idx = i % num_samples;

    // Typical ISR operations using assembly functions
    int32_t instant_power, voltage_squared;
    int16_t filter_delta;

    multS16x16_to32(instant_power, voltage_samples[idx], current_samples[idx]);
    multS16x16_to32(voltage_squared, voltage_samples[idx], voltage_samples[idx]);
    mult16x8_q8(filter_delta, prev_current[idx] - current_samples[idx], filter_factor);

    power_sum += instant_power;
    voltage_squared_sum += voltage_squared;
    filtered_current += filter_delta;
  }

  unsigned long asm_time = micros() - start_time;

  // Reset for standard test
  power_sum = 0;
  voltage_squared_sum = 0;
  filtered_current = 0;

  // Warm up standard operations
  for (uint8_t k = 0; k < 10; k++)
  {
    int32_t power = (int32_t)voltage_samples[k % num_samples] * current_samples[k % num_samples];
    int32_t vsquared = (int32_t)voltage_samples[k % num_samples] * voltage_samples[k % num_samples];
    int16_t filter_delta = ((int32_t)(prev_current[k % num_samples] - current_samples[k % num_samples]) * filter_factor + 0x80) >> 8;
  }

  // Performance test for standard C ISR simulation
  start_time = micros();

  for (uint16_t i = 0; i < samples; i++)
  {
    uint8_t idx = i % num_samples;

    // Typical ISR operations using standard C
    int32_t instant_power = (int32_t)voltage_samples[idx] * current_samples[idx];
    int32_t voltage_squared = (int32_t)voltage_samples[idx] * voltage_samples[idx];
    int16_t filter_delta = ((int32_t)(prev_current[idx] - current_samples[idx]) * filter_factor + 0x80) >> 8;

    power_sum += instant_power;
    voltage_squared_sum += voltage_squared;
    filtered_current += filter_delta;
  }

  unsigned long std_time = micros() - start_time;

  // Report results
  Serial.println(F("--- ISR Simulation Performance Results ---"));
  Serial.print(F("Samples processed: "));
  Serial.println(samples);
  Serial.print(F("Assembly ISR time: "));
  Serial.print(asm_time);
  Serial.println(F(" µs"));
  Serial.print(F("Standard ISR time: "));
  Serial.print(std_time);
  Serial.println(F(" µs"));

  if (asm_time > 0 && std_time > 0)
  {
    Serial.print(F("Assembly samples/µs: "));
    Serial.println((float)samples / asm_time, 2);
    Serial.print(F("Standard samples/µs: "));
    Serial.println((float)samples / std_time, 2);

    if (asm_time < std_time)
    {
      Serial.print(F("✓ Assembly ISR is "));
      Serial.print((float)std_time / asm_time, 2);
      Serial.println(F("x faster"));

      // Calculate time savings per ISR call
      float time_saved_per_sample = (float)(std_time - asm_time) / samples;
      Serial.print(F("Time saved per ISR: "));
      Serial.print(time_saved_per_sample, 2);
      Serial.println(F(" µs"));
    }
    else
    {
      Serial.print(F("⚠ Standard ISR is "));
      Serial.print((float)asm_time / std_time, 2);
      Serial.println(F("x faster"));
    }
  }

  // The ISR test is more realistic - functions are inlined in real usage
  // Allow assembly to be up to 2x slower due to function call overhead in test
  TEST_ASSERT_TRUE(asm_time <= std_time * 2);
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
    // Functional tests
    RUN_TEST(test_multS16x16_to32_basic);
    delay(100);
    RUN_TEST(test_multU16x16_to32_basic);
    delay(100);
    RUN_TEST(test_mult16x8_q8_basic);
    delay(100);
    RUN_TEST(test_q8_conversion_helpers);
    delay(100);
    RUN_TEST(test_mult16x8_q8_rounding);
    delay(100);
    RUN_TEST(test_edge_cases);
    delay(100);
    RUN_TEST(test_assembly_vs_standard);
    delay(100);

    // Performance tests
    Serial.println(F(""));
    Serial.println(F("========================================"));
    Serial.println(F("Performance Tests"));
    Serial.println(F("========================================"));
    RUN_TEST(test_performance_multS16x16_to32);
    delay(200);
    RUN_TEST(test_performance_multU16x16_to32);
    delay(200);
    RUN_TEST(test_performance_mult16x8_q8);
    delay(200);
    RUN_TEST(test_performance_isr_simulation);
    delay(200);

    ++i;
  }
  else if (i == max_blinks)
  {
    UNITY_END();  // End Unity test framework
    ++i;
  }
}