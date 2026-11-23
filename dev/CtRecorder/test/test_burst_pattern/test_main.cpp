#include <unity.h>

#include "burst_pattern.h"

void setUp() {}
void tearDown() {}

void test_burst_default_pattern()
{
  const BurstPattern p;
  for (uint32_t c = 0; c != 5; ++c)
  {
    TEST_ASSERT_TRUE(p.onForCycle(c));
  }
  for (uint32_t c = 5; c != 155; ++c)
  {
    TEST_ASSERT_FALSE(p.onForCycle(c));
  }
  for (uint32_t c = 155; c != 165; ++c)
  {
    TEST_ASSERT_TRUE(p.onForCycle(c));
  }
  // the last pair is 50 on, 150 off; the period is 5+150+10+150+25+150+50+150 = 690
  TEST_ASSERT_EQUAL_UINT32(690, p.period());
  TEST_ASSERT_TRUE(p.onForCycle(690 - 200));
  TEST_ASSERT_TRUE(p.onForCycle(690 - 151));
  TEST_ASSERT_FALSE(p.onForCycle(690 - 150));
  TEST_ASSERT_FALSE(p.onForCycle(689));
  TEST_ASSERT_TRUE(p.onForCycle(690));  // wraps
  TEST_ASSERT_TRUE(p.onForCycle(690 + 4));
  TEST_ASSERT_FALSE(p.onForCycle(690 + 5));
}

void test_custom_pattern_and_limits()
{
  BurstPattern p;
  const BurstPattern::Step steps[]{ { 2, 3 }, { 1, 1 } };
  TEST_ASSERT_TRUE(p.set(steps, 2));
  const bool expected[]{ true, true, false, false, false, true, false };
  for (uint32_t c = 0; c != 14; ++c)
  {
    TEST_ASSERT_EQUAL(expected[c % 7], p.onForCycle(c));
  }
  // refused: empty, a pair of zero cycles, too many pairs; the pattern is left as it was
  TEST_ASSERT_FALSE(p.set(steps, 0));
  const BurstPattern::Step zero[]{ { 0, 0 } };
  TEST_ASSERT_FALSE(p.set(zero, 1));
  BurstPattern::Step many[BurstPattern::MAX_STEPS + 1]{};
  for (auto &s : many)
  {
    s = { 1, 1 };
  }
  TEST_ASSERT_FALSE(p.set(many, BurstPattern::MAX_STEPS + 1));
  TEST_ASSERT_TRUE(p.onForCycle(0));
  TEST_ASSERT_EQUAL_UINT32(7, p.period());
  // a pair with no off time (always on) is fine
  const BurstPattern::Step alwaysOn[]{ { 5, 0 } };
  TEST_ASSERT_TRUE(p.set(alwaysOn, 1));
  TEST_ASSERT_TRUE(p.onForCycle(1000));
}

// the ISR steps a cursor once per cycle (no 32-bit modulo there): it must agree with onForCycle()
void test_cursor_matches_onForCycle()
{
  BurstPattern p;
  const BurstPattern::Step steps[]{ { 0, 3 }, { 2, 0 }, { 1, 2 }, { 3, 1 } };
  for (uint8_t round = 0; round != 2; ++round)
  {
    if (round != 0)
    {
      TEST_ASSERT_TRUE(p.set(steps, 4));
    }
    BurstPattern::Cursor cursor;
    cursor.reset(p);
    for (uint32_t c = 0; c != 3 * p.period() + 7; ++c)
    {
      TEST_ASSERT_EQUAL(p.onForCycle(c), cursor.next(p));
    }
  }
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_burst_default_pattern);
  RUN_TEST(test_custom_pattern_and_limits);
  RUN_TEST(test_cursor_matches_onForCycle);
  return UNITY_END();
}
