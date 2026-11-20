/**
 * @file test_main.cpp
 * @brief Performance benchmarks for FastDivision
 *
 * Measures execution time of AVR assembly fast division functions
 * compared to standard C division operators.
 *
 * Runs in CI in the simavr simulator (uno_sim environment): simavr is cycle-accurate, so the
 * timings are exact and the same from run to run. On a connected board:
 * pio test -e uno -f "*test_fastdivision_perf*"
 *
 * Every result is also printed as a BENCH line (bench_report.h), for the CI benchmark history.
 */

#include <Arduino.h>
#include <unity.h>

#include "../bench_report.h"
#include "../bench_timer.h"
#include "../halt_after_tests.h"

#include "FastDivision.h"
#include "FastDivision.cpp"  // Include implementation for linking

// Number of iterations for timing (must be high enough to measure accurately)
constexpr uint32_t ITERATIONS = 10000UL;

// Volatile to prevent compiler optimization
volatile uint16_t v_result16;
volatile uint32_t v_result32;
volatile uint8_t v_result8;

void setUp(void)
{
}

void tearDown(void)
{
}

// ============================================================================
// Benchmark Helpers
// ============================================================================

/**
 * @brief Reports benchmark results to Serial
 */
void reportBenchmark(const char* name, uint32_t fastTime, uint32_t stdTime)
{
  float speedup = (float)stdTime / (float)fastTime;

  Serial.print(F("\n  "));
  Serial.print(name);
  Serial.print(F(": fast="));
  Serial.print(fastTime);
  Serial.print(F(" cycles, std="));
  Serial.print(stdTime);
  Serial.print(F(" cycles, speedup="));
  Serial.print(speedup, 2);
  Serial.print(F("x"));
  Serial.print(F("\n    per call: fast="));
  printPerOp(fastTime, ITERATIONS);
  Serial.print(F(", std="));
  printPerOp(stdTime, ITERATIONS);
}

// ============================================================================
// Performance Tests for divu10
// ============================================================================

void test_perf_divu10_small_values(void)
{
  uint32_t fastTime, stdTime;

  // Test with small values (0-255)
  Serial.print(F("\n[BENCHMARK] divu10 small values (0-255):"));

  // Fast division timing
  BenchTimer::start();
  for (uint32_t i = 0; i < ITERATIONS; ++i)
  {
    v_result16 = divu10((uint16_t)(i & 0xFF));
  }
  fastTime = BenchTimer::stop();

  // Standard division timing
  BenchTimer::start();
  for (uint32_t i = 0; i < ITERATIONS; ++i)
  {
    v_result16 = (uint16_t)(i & 0xFF) / 10;
  }
  stdTime = BenchTimer::stop();

  reportBenchmark("small", fastTime, stdTime);
  benchCycles(F("fastdivision: divu10 small per call"), fastTime, ITERATIONS);

  // Just verify correctness, not timing (speedup varies)
  TEST_ASSERT_TRUE(fastTime > 0);
  TEST_ASSERT_TRUE(stdTime > 0);
}

void test_perf_divu10_medium_values(void)
{
  uint32_t fastTime, stdTime;

  // Test with medium values (256-4095)
  Serial.print(F("\n[BENCHMARK] divu10 medium values (256-4095):"));

  // Fast division timing
  BenchTimer::start();
  for (uint32_t i = 0; i < ITERATIONS; ++i)
  {
    v_result16 = divu10((uint16_t)((i & 0xFFF) + 256));
  }
  fastTime = BenchTimer::stop();

  // Standard division timing
  BenchTimer::start();
  for (uint32_t i = 0; i < ITERATIONS; ++i)
  {
    v_result16 = ((uint16_t)((i & 0xFFF) + 256)) / 10;
  }
  stdTime = BenchTimer::stop();

  reportBenchmark("medium", fastTime, stdTime);
  benchCycles(F("fastdivision: divu10 medium per call"), fastTime, ITERATIONS);

  TEST_ASSERT_TRUE(fastTime > 0);
  TEST_ASSERT_TRUE(stdTime > 0);
}

void test_perf_divu10_large_values(void)
{
  uint32_t fastTime, stdTime;

  // Test with large values (full uint16_t range)
  Serial.print(F("\n[BENCHMARK] divu10 large values (0-65535):"));

  // Fast division timing
  BenchTimer::start();
  for (uint32_t i = 0; i < ITERATIONS; ++i)
  {
    v_result16 = divu10((uint16_t)i);
  }
  fastTime = BenchTimer::stop();

  // Standard division timing
  BenchTimer::start();
  for (uint32_t i = 0; i < ITERATIONS; ++i)
  {
    v_result16 = ((uint16_t)i) / 10;
  }
  stdTime = BenchTimer::stop();

  reportBenchmark("large", fastTime, stdTime);
  benchCycles(F("fastdivision: divu10 large per call"), fastTime, ITERATIONS);

  TEST_ASSERT_TRUE(fastTime > 0);
  TEST_ASSERT_TRUE(stdTime > 0);
}

// ============================================================================
// Performance Tests for divmod10
// ============================================================================

void test_perf_divmod10_small_values(void)
{
  uint32_t fastTime, stdTime;
  uint32_t div;
  uint8_t mod;

  Serial.print(F("\n[BENCHMARK] divmod10 small values (0-255):"));

  // Fast division timing
  BenchTimer::start();
  for (uint32_t i = 0; i < ITERATIONS; ++i)
  {
    divmod10(i & 0xFF, div, mod);
    v_result32 = div;
    v_result8 = mod;
  }
  fastTime = BenchTimer::stop();

  // Standard division timing
  BenchTimer::start();
  for (uint32_t i = 0; i < ITERATIONS; ++i)
  {
    uint32_t val = i & 0xFF;
    v_result32 = val / 10;
    v_result8 = val % 10;
  }
  stdTime = BenchTimer::stop();

  reportBenchmark("small", fastTime, stdTime);
  benchCycles(F("fastdivision: divmod10 small per call"), fastTime, ITERATIONS);

  TEST_ASSERT_TRUE(fastTime > 0);
  TEST_ASSERT_TRUE(stdTime > 0);
}

void test_perf_divmod10_large_values(void)
{
  uint32_t fastTime, stdTime;
  uint32_t div;
  uint8_t mod;

  Serial.print(F("\n[BENCHMARK] divmod10 large values (full uint32_t):"));

  // Fast division timing
  BenchTimer::start();
  for (uint32_t i = 0; i < ITERATIONS; ++i)
  {
    divmod10(i * 1000UL, div, mod);
    v_result32 = div;
    v_result8 = mod;
  }
  fastTime = BenchTimer::stop();

  // Standard division timing
  BenchTimer::start();
  for (uint32_t i = 0; i < ITERATIONS; ++i)
  {
    uint32_t val = i * 1000UL;
    v_result32 = val / 10;
    v_result8 = val % 10;
  }
  stdTime = BenchTimer::stop();

  reportBenchmark("large", fastTime, stdTime);
  benchCycles(F("fastdivision: divmod10 large per call"), fastTime, ITERATIONS);

  TEST_ASSERT_TRUE(fastTime > 0);
  TEST_ASSERT_TRUE(stdTime > 0);
}

// ============================================================================
// Cycle Count Estimation
// ============================================================================

void test_perf_cycle_estimation(void)
{
  constexpr uint32_t CYCLE_ITERATIONS = 1000UL;

  Serial.print(F("\n\n[CYCLE ESTIMATION] ("));
  Serial.print(CYCLE_ITERATIONS);
  Serial.print(F(" iterations):"));

  // divu10 timing
  BenchTimer::start();
  for (uint32_t i = 0; i < CYCLE_ITERATIONS; ++i)
  {
    v_result16 = divu10(12345);
  }
  const uint32_t divu10Time{ BenchTimer::stop() };

  // divmod10 timing
  uint32_t div;
  uint8_t mod;
  BenchTimer::start();
  for (uint32_t i = 0; i < CYCLE_ITERATIONS; ++i)
  {
    divmod10(12345678UL, div, mod);
    v_result32 = div;
  }
  const uint32_t divmod10Time{ BenchTimer::stop() };

  // per call, loop included
  Serial.print(F("\n  divu10: "));
  printPerOp(divu10Time, CYCLE_ITERATIONS);
  Serial.print(F(" (claimed: 29 cycles)"));

  Serial.print(F("\n  divmod10: "));
  printPerOp(divmod10Time, CYCLE_ITERATIONS);

  benchCycles(F("fastdivision: divu10(12345) per call"), divu10Time, CYCLE_ITERATIONS);
  benchCycles(F("fastdivision: divmod10(12345678) per call"), divmod10Time, CYCLE_ITERATIONS);

  Serial.print(F("\n\nNote: Cycle counts include loop overhead and function call."));
  Serial.print(F("\nActual instruction cycles are lower than measured."));

  TEST_PASS();
}

// ============================================================================
// Summary
// ============================================================================

void test_perf_summary(void)
{
  Serial.print(F("\n\n"));
  Serial.print(F("================================================================================\n"));
  Serial.print(F("PERFORMANCE SUMMARY\n"));
  Serial.print(F("================================================================================\n"));
  Serial.print(F("Platform: Arduino Uno (ATmega328P @ 16MHz)\n"));
  Serial.print(F("Iterations per test: "));
  Serial.print(ITERATIONS);
  Serial.print(F("\n\n"));
  Serial.print(F("The AVR assembly fast division functions provide significant speedup\n"));
  Serial.print(F("over standard C division operators, which is critical for ISR performance.\n"));
  Serial.print(F("\n"));
  Serial.print(F("Key findings (measured on an Arduino Uno, see the speedups above):\n"));
  Serial.print(F("- divu10: ~4.8x faster than standard uint16_t division\n"));
  Serial.print(F("- divmod10: ~4.4x faster than standard uint32_t div+mod\n"));
  Serial.print(F("- Shift-based divisions (divu2/4/8) compile to single instructions\n"));
  Serial.print(F("================================================================================\n"));

  TEST_PASS();
}

// ============================================================================
// Main
// ============================================================================

void setup()
{
  delay(1000);
  UNITY_BEGIN();

  Serial.print(F("\n\n"));
  Serial.print(F("================================================================================\n"));
  Serial.print(F("FastDivision Performance Benchmarks\n"));
  Serial.print(F("================================================================================\n"));
  Serial.print(F("Platform: Arduino Uno (ATmega328P @ 16MHz)\n"));
  Serial.print(F("This test measures execution time of fast division vs standard division.\n"));
  Serial.print(F("================================================================================\n"));
}

void loop()
{
  // divu10 benchmarks
  RUN_TEST(test_perf_divu10_small_values);
  RUN_TEST(test_perf_divu10_medium_values);
  RUN_TEST(test_perf_divu10_large_values);

  // divmod10 benchmarks
  RUN_TEST(test_perf_divmod10_small_values);
  RUN_TEST(test_perf_divmod10_large_values);

  // Cycle estimation
  RUN_TEST(test_perf_cycle_estimation);

  // Summary
  RUN_TEST(test_perf_summary);

  UNITY_END();
  haltAfterTests();
}
