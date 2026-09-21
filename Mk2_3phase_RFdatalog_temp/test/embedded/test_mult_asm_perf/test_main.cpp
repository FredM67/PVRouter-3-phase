/**
 * @file test_main.cpp
 * @brief Performance benchmarks for assembly multiplication functions
 * @version 0.1
 * @date 2026-01-30
 *
 * This file contains performance benchmarks for the assembly-optimized
 * multiplication functions. These tests should only be run on real hardware,
 * not in CI simulators where timing is meaningless.
 *
 * Based on:
 * - florentbr's optimization suggestions for PVRouter
 * - avrfreertos multiplication optimizations by feilipu
 * - OpenMusicLabs AVR assembly techniques
 */

#include <Arduino.h>
#include <unity.h>

#include "mult_asm.h"

void setUp(void)
{
  // Set up code here (if needed)
}

void tearDown(void)
{
  // Clean up code here (if needed)
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
  Serial.println(F(" us"));
  Serial.print(F("Standard time: "));
  Serial.print(std_time);
  Serial.println(F(" us"));

  if (asm_time > 0 && std_time > 0)
  {
    Serial.print(F("Assembly ops/us: "));
    Serial.println((float)total_ops / asm_time, 2);
    Serial.print(F("Standard ops/us: "));
    Serial.println((float)total_ops / std_time, 2);

    if (asm_time < std_time)
    {
      Serial.print(F("Assembly is "));
      Serial.print((float)std_time / asm_time, 2);
      Serial.println(F("x faster"));
    }
    else
    {
      Serial.print(F("Standard is "));
      Serial.print((float)asm_time / std_time, 2);
      Serial.println(F("x faster"));
    }
  }

  // The test passes if assembly is reasonably competitive with standard
  TEST_ASSERT_TRUE(asm_time <= std_time * 10);
}

/**
 * @brief Performance comparison for unsigned multiplication (multU16x16_to32)
 */
void test_performance_multU16x16_to32(void)
{
  const uint16_t iterations = 1000;
  uint32_t result;

  // Test data - unsigned values typical for ADC and V squared calculations
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
  Serial.println(F(" us"));
  Serial.print(F("Standard time: "));
  Serial.print(std_time);
  Serial.println(F(" us"));

  if (asm_time > 0 && std_time > 0)
  {
    Serial.print(F("Assembly ops/us: "));
    Serial.println((float)total_ops / asm_time, 2);
    Serial.print(F("Standard ops/us: "));
    Serial.println((float)total_ops / std_time, 2);

    if (asm_time < std_time)
    {
      Serial.print(F("Assembly is "));
      Serial.print((float)std_time / asm_time, 2);
      Serial.println(F("x faster"));
    }
    else
    {
      Serial.print(F("Standard is "));
      Serial.print((float)asm_time / std_time, 2);
      Serial.println(F("x faster"));
    }
  }

  // The test passes if assembly is reasonably competitive with standard
  TEST_ASSERT_TRUE(asm_time <= std_time * 10);
}

/**
 * @brief Performance comparison for Q8 fractional multiplication
 */
void test_performance_mult16x8_q8(void)
{
  const uint16_t iterations = 10000;
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
  Serial.println(F(" us"));
  Serial.print(F("Standard time: "));
  Serial.print(std_time);
  Serial.println(F(" us"));

  if (asm_time > 0 && std_time > 0)
  {
    Serial.print(F("Assembly ops/us: "));
    Serial.println((float)total_ops / asm_time, 2);
    Serial.print(F("Standard ops/us: "));
    Serial.println((float)total_ops / std_time, 2);

    if (asm_time < std_time)
    {
      Serial.print(F("Assembly is "));
      Serial.print((float)std_time / asm_time, 2);
      Serial.println(F("x faster"));
    }
    else
    {
      Serial.print(F("Standard is "));
      Serial.print((float)asm_time / std_time, 2);
      Serial.println(F("x faster"));
    }
  }

  // The test passes if assembly is reasonably competitive with standard
  TEST_ASSERT_TRUE(asm_time <= std_time * 10);
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
    (void)power;
    (void)vsquared;
    (void)filter_delta;
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
  Serial.println(F(" us"));
  Serial.print(F("Standard ISR time: "));
  Serial.print(std_time);
  Serial.println(F(" us"));

  if (asm_time > 0 && std_time > 0)
  {
    Serial.print(F("Assembly samples/us: "));
    Serial.println((float)samples / asm_time, 2);
    Serial.print(F("Standard samples/us: "));
    Serial.println((float)samples / std_time, 2);

    if (asm_time < std_time)
    {
      Serial.print(F("Assembly ISR is "));
      Serial.print((float)std_time / asm_time, 2);
      Serial.println(F("x faster"));

      // Calculate time savings per ISR call
      float time_saved_per_sample = (float)(std_time - asm_time) / samples;
      Serial.print(F("Time saved per ISR: "));
      Serial.print(time_saved_per_sample, 2);
      Serial.println(F(" us"));
    }
    else
    {
      Serial.print(F("Standard ISR is "));
      Serial.print((float)asm_time / std_time, 2);
      Serial.println(F("x faster"));
    }
  }

  // The ISR test is more realistic - functions are inlined in real usage
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
    Serial.println(F("========================================"));
    Serial.println(F("Performance Tests (Hardware Only)"));
    Serial.println(F("========================================"));
    Serial.println(F(""));

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
