#include <unity.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "sample_path.h"

void setUp() {}
void tearDown() {}

namespace
{
constexpr double kPi{ 3.14159265358979323846 };
constexpr double kSpacing{ 624e-6 };  // 3-phase sample spacing, seconds

// left-aligned ADC value of a 50 Hz sine at sample n
uint16_t sineRaw(uint32_t n, double centre, double amplitude)
{
  const double counts{ centre + amplitude * std::sin(2.0 * kPi * 50.0 * static_cast< double >(n) * kSpacing) };
  return static_cast< uint16_t >(static_cast< uint16_t >(std::lround(counts)) << 6);
}
}  // namespace

void test_initial_offset_is_nominal()
{
  SamplePath p;
  TEST_ASSERT_EQUAL_UINT16(32704, p.dcOffset());
}

void test_voltage_minus_dc_rounds_half()
{
  SamplePath p;
  TEST_ASSERT_EQUAL_INT16(96, p.onVoltage(512U << 6));  // (32768 | 32) - 32704
}

void test_offset_tracks_a_shifted_sine()
{
  // the integrator's gain is 1/32768 per sample (time constant ~20 s): 100 s settles it
  SamplePath p;
  const uint32_t total{ 160256 };
  uint16_t last{ p.dcOffset() };
  std::vector< uint32_t > changes;
  for (uint32_t n = 0; n != total; ++n)
  {
    p.onVoltage(sineRaw(n, 530.0, 300.0));
    if (p.dcOffset() != last)
    {
      last = p.dcOffset();
      if (n < 3200)  // the first 2 s: the offset moves at every crossing
      {
        changes.push_back(n);
      }
    }
  }
  const int32_t error{ static_cast< int32_t >(p.dcOffset()) - (530 << 6) };
  TEST_ASSERT_LESS_OR_EQUAL_INT32(64, std::abs(error));

  // one change per cycle (20 ms = 32.05 samples): never in between
  TEST_ASSERT_GREATER_THAN_UINT32(3, static_cast< uint32_t >(changes.size()));
  for (size_t i = 1; i < changes.size(); ++i)
  {
    const uint32_t gap{ changes[i] - changes[i - 1] };
    TEST_ASSERT_TRUE(gap >= 31 && gap <= 33);
  }
}

void test_persistence_needs_two_samples()
{
  SamplePath p;
  p.onVoltage(600U << 6);  // one positive sample
  TEST_ASSERT_FALSE(p.cycleStarted());
  p.onVoltage(400U << 6);  // negative again: the count restarts
  p.onVoltage(600U << 6);
  TEST_ASSERT_FALSE(p.cycleStarted());
  p.onVoltage(600U << 6);  // two in a row
  TEST_ASSERT_TRUE(p.cycleStarted());
  p.onVoltage(600U << 6);
  TEST_ASSERT_FALSE(p.cycleStarted());
}

void test_cycle_started_once_per_cycle()
{
  SamplePath p;
  uint32_t starts{ 0 };
  const uint32_t samples{ 1602 };  // 1 s
  for (uint32_t n = 0; n != samples; ++n)
  {
    p.onVoltage(sineRaw(n, 511.0, 300.0));
    starts += p.cycleStarted() ? 1U : 0U;
  }
  TEST_ASSERT_UINT32_WITHIN(1, 50, starts);
}

void test_current_uses_voltage_offset()
{
  SamplePath p;
  for (uint32_t n = 0; n != 3200; ++n)
  {
    p.onVoltage(sineRaw(n, 530.0, 300.0));
  }
  TEST_ASSERT_NOT_EQUAL(32704, p.dcOffset());
  const uint16_t raw{ 700U << 6 };
  TEST_ASSERT_EQUAL_INT16(static_cast< int16_t >((raw | 32U) - p.dcOffset()), p.currentMinusDc(raw));
}

void test_copy_is_a_snapshot_of_the_whole_state()
{
  SamplePath p;
  for (uint32_t n = 0; n != 3200; ++n)
  {
    p.onVoltage(sineRaw(n, 530.0, 300.0));
  }
  SamplePath copy{ p };  // what the recorder's ISR snapshots
  TEST_ASSERT_EQUAL_UINT32(p.integrator(), copy.integrator());
  for (uint32_t n = 3200; n != 3600; ++n)
  {
    const uint16_t raw{ sineRaw(n, 530.0, 300.0) };
    TEST_ASSERT_EQUAL_INT16(p.onVoltage(raw), copy.onVoltage(raw));
    TEST_ASSERT_EQUAL_UINT16(p.dcOffset(), copy.dcOffset());
    TEST_ASSERT_EQUAL_UINT32(p.integrator(), copy.integrator());
    TEST_ASSERT_EQUAL(p.confirmedPositive(), copy.confirmedPositive());
    TEST_ASSERT_EQUAL(p.lastConfirmedPositive(), copy.lastConfirmedPositive());
    TEST_ASSERT_EQUAL_UINT8(p.persistenceCount(), copy.persistenceCount());
    TEST_ASSERT_EQUAL(p.cycleStarted(), copy.cycleStarted());
  }
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_copy_is_a_snapshot_of_the_whole_state);
  RUN_TEST(test_initial_offset_is_nominal);
  RUN_TEST(test_voltage_minus_dc_rounds_half);
  RUN_TEST(test_offset_tracks_a_shifted_sine);
  RUN_TEST(test_persistence_needs_two_samples);
  RUN_TEST(test_cycle_started_once_per_cycle);
  RUN_TEST(test_current_uses_voltage_offset);
  return UNITY_END();
}
