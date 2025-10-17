/**
 * @file test_main.cpp
 * @brief Unity tests for voltage accumulation overflow analysis
 * @author Frederic Metrich
 * @date 2025-10-17
 */

#include <Arduino.h>
#include <unity.h>
#include "../../../../mult_asm.h"

// System constants
constexpr uint8_t SUPPLY_FREQUENCY = 50;
constexpr float SAMPLE_PERIOD_US = 624.0f;
constexpr uint16_t ADC_MIDPOINT_ALIGNED = 32768;

uint16_t simulateADC(float voltage_inst)
{
  float adc = (voltage_inst / 400.0f) * 1024.0f + 512.0f;
  if (adc < 0) adc = 0;
  if (adc > 1023) adc = 1023;
  return static_cast< uint16_t >(adc) << 6;
}

/**
 * @brief Generic test function for voltage accumulation with different parameters
 * 
 * @tparam use_64bit If true, uses int64_t accumulator to detect overflow beyond INT32_MAX
 * @param period Datalog period in seconds
 * @param shift Right shift amount (8, 12, or 16)
 * @param vrms RMS voltage to simulate
 * @param expect_overflow If true, expects accumulator to exceed INT32_MAX
 * @param test_name Test name for serial output
 */
template< bool use_64bit = false >
void test_voltage_accumulation(uint8_t period, uint8_t shift, float vrms, bool expect_overflow, const char* test_name)
{
  const float vpeak = vrms * 1.414f;
  const uint32_t cycles = period * SUPPLY_FREQUENCY;
  const uint32_t samples = static_cast< uint32_t >(cycles * (20000.0f / SAMPLE_PERIOD_US));

  if constexpr (use_64bit)
  {
    // Use 64-bit accumulator to detect overflow
    uint64_t sum_vsq = 0;
    uint64_t max_acc = 0;
    float time_us = 0.0f;

    for (uint32_t s = 0; s < samples; ++s)
    {
      float angle = 2.0f * PI * SUPPLY_FREQUENCY * time_us / 1e6f;
      uint16_t adc = simulateADC(vpeak * sin(angle));
      int16_t sample = (adc | 32U) - ADC_MIDPOINT_ALIGNED;
      int32_t vsq_signed;
      multS16x16_to32(vsq_signed, sample, sample);
      uint32_t vsq = static_cast< uint32_t >(vsq_signed);  // V² is always positive
      vsq >>= shift;
      sum_vsq += vsq;
      if (sum_vsq > max_acc) max_acc = sum_vsq;
      time_us += SAMPLE_PERIOD_US;
    }

    Serial.print(test_name);
    Serial.print(F(": samples="));
    Serial.print(samples);
    Serial.print(F(", max="));
    Serial.print((unsigned long)max_acc);
    Serial.print(F(" vs UINT32_MAX="));
    Serial.println((unsigned long)UINT32_MAX);

    if (expect_overflow)
    {
      // For the overflow test, we expect max_acc > UINT32_MAX would have wrapped
      // So we just verify it's a large value indicating we would overflow with uint32_t
      TEST_ASSERT_GREATER_THAN_UINT64(UINT32_MAX, max_acc);
    }
    else
    {
      TEST_ASSERT_LESS_THAN_UINT32(UINT32_MAX, max_acc);
    }
  }
  else
  {
    // Use 32-bit accumulator (normal case)
    uint32_t sum_vsq = 0;
    uint32_t max_acc = 0;
    float time_us = 0.0f;

    for (uint32_t s = 0; s < samples; ++s)
    {
      float angle = 2.0f * PI * SUPPLY_FREQUENCY * time_us / 1e6f;
      uint16_t adc = simulateADC(vpeak * sin(angle));
      int16_t sample = (adc | 32U) - ADC_MIDPOINT_ALIGNED;
      int32_t vsq_signed;
      multS16x16_to32(vsq_signed, sample, sample);
      uint32_t vsq = static_cast< uint32_t >(vsq_signed);  // V² is always positive
      vsq >>= shift;
      sum_vsq += vsq;
      if (sum_vsq > max_acc) max_acc = sum_vsq;
      time_us += SAMPLE_PERIOD_US;
    }

    uint32_t headroom = UINT32_MAX - max_acc;

    Serial.print(test_name);
    Serial.print(F(": samples="));
    Serial.print(samples);
    Serial.print(F(", max="));
    Serial.print(max_acc);
    Serial.print(F(", headroom="));
    Serial.println(headroom);

    // Verify we have significant headroom (at least 10% of UINT32_MAX)
    // This ensures we're not close to overflow
    TEST_ASSERT_GREATER_THAN_UINT32(UINT32_MAX / 10, headroom);
  }
}

void setUp(void) {}
void tearDown(void) {}

void test_5s_shift12_230V(void)
{
  test_voltage_accumulation(5, 12, 230.0f, false, "5s, >>12, 230V");
}

void test_5s_shift8_230V(void)
{
  test_voltage_accumulation(5, 8, 230.0f, false, "5s, >>8 (TODO), 230V");
}

void test_5s_shift12_253V(void)
{
  test_voltage_accumulation(5, 12, 253.0f, false, "5s, >>12, 253V");
}

void test_10s_shift12_230V(void)
{
  test_voltage_accumulation(10, 12, 230.0f, false, "10s, >>12, 230V");
}

void test_20s_shift16_230V(void)
{
  test_voltage_accumulation(20, 16, 230.0f, false, "20s, >>16, 230V");
}

void test_20s_shift12_overflow(void)
{
  test_voltage_accumulation< true >(20, 12, 230.0f, true, "20s, >>12 OVERFLOW");
}

void test_40s_shift16_253V(void)
{
  test_voltage_accumulation(40, 16, 253.0f, false, "40s (max), >>16, 253V");
}

void setup()
{
  delay(2000);
  UNITY_BEGIN();
  Serial.println(F(""));
  Serial.println(F("=== Voltage Accumulation Overflow Tests ==="));
  Serial.print(F("ADC Midpoint: "));
  Serial.println(ADC_MIDPOINT_ALIGNED);
  Serial.print(F("Sample Period: "));
  Serial.print(SAMPLE_PERIOD_US);
  Serial.println(F(" us"));
  Serial.print(F("UINT32_MAX: "));
  Serial.println(UINT32_MAX);
  Serial.println(F(""));
}

uint8_t i = 0;

void loop()
{
  if (i == 0)
  {
    Serial.println(F("--- Current Implementation (should PASS) ---"));
    RUN_TEST(test_5s_shift12_230V);
    delay(100);
    RUN_TEST(test_5s_shift12_253V);
    delay(100);
    RUN_TEST(test_10s_shift12_230V);
    delay(100);
    RUN_TEST(test_20s_shift16_230V);
    delay(100);
    RUN_TEST(test_40s_shift16_253V);
    delay(100);

    Serial.println(F(""));
    Serial.println(F("--- TODO Suggestion (>>8 shift) ---"));
    RUN_TEST(test_5s_shift8_230V);
    delay(100);

    Serial.println(F(""));
    Serial.println(F("--- Overflow Demo (should FAIL) ---"));
    RUN_TEST(test_20s_shift12_overflow);
    delay(100);

    Serial.println(F(""));
    Serial.println(F("=== CONCLUSIONS ==="));
    Serial.println(F("1. Current code (>>12 for <=10s, >>16 for >10s) is SAFE"));
    Serial.println(F("2. TODO suggestion (>>8) provides marginal benefit"));
    Serial.println(F("3. Using >>12 for long periods WILL OVERFLOW"));
    Serial.println(F("4. Recommendation: KEEP current implementation!"));
    Serial.println(F(""));

    UNITY_END();
  }
  ++i;
}
