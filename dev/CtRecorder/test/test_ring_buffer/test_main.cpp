#include <unity.h>

#include "ring_buffer.h"

void setUp() {}
void tearDown() {}

static FrameData frameOf(uint16_t v)
{
  FrameData f{};
  f.v = v;
  f.i = static_cast< uint16_t >(v + 1);
  f.iOut = static_cast< int16_t >(v * 3);
  return f;
}

void test_ring_buffer_fifo_order()
{
  RingBuffer< 8 > ring;
  FrameData f;
  TEST_ASSERT_FALSE(ring.pop(f));
  // wrap the indices several times
  uint16_t next{ 0 };
  uint16_t expected{ 0 };
  for (uint8_t round = 0; round != 10; ++round)
  {
    for (uint8_t k = 0; k != 5; ++k)
    {
      TEST_ASSERT_TRUE(ring.push(frameOf(next++)));
    }
    for (uint8_t k = 0; k != 5; ++k)
    {
      TEST_ASSERT_TRUE(ring.pop(f));
      TEST_ASSERT_EQUAL_UINT16(expected, f.v);
      TEST_ASSERT_EQUAL_UINT16(expected + 1, f.i);
      TEST_ASSERT_FALSE(f.overrun);
      ++expected;
    }
  }
  TEST_ASSERT_FALSE(ring.pop(f));
  TEST_ASSERT_EQUAL_UINT32(0, ring.overruns());
}

void test_holds_exactly_n_frames()
{
  RingBuffer< 8 > ring;
  for (uint8_t k = 0; k != 8; ++k)
  {
    TEST_ASSERT_TRUE(ring.push(frameOf(k)));
  }
  TEST_ASSERT_FALSE(ring.push(frameOf(99)));
}

void test_overrun_flag_on_next_frame()
{
  RingBuffer< 8 > ring;
  for (uint8_t k = 0; k != 9; ++k)  // N + 1 without a pop: the last is dropped
  {
    ring.push(frameOf(k));
  }
  TEST_ASSERT_EQUAL_UINT32(1, ring.overruns());
  FrameData f;
  for (uint8_t k = 0; k != 8; ++k)  // the frames before the gap are not flagged
  {
    TEST_ASSERT_TRUE(ring.pop(f));
    TEST_ASSERT_EQUAL_UINT16(k, f.v);
    TEST_ASSERT_FALSE(f.overrun);
  }
  TEST_ASSERT_TRUE(ring.push(frameOf(100)));  // the first stored after the drop
  TEST_ASSERT_TRUE(ring.push(frameOf(101)));
  TEST_ASSERT_TRUE(ring.pop(f));
  TEST_ASSERT_EQUAL_UINT16(100, f.v);
  TEST_ASSERT_TRUE(f.overrun);
  TEST_ASSERT_TRUE(ring.pop(f));
  TEST_ASSERT_EQUAL_UINT16(101, f.v);
  TEST_ASSERT_FALSE(f.overrun);
  TEST_ASSERT_EQUAL_UINT32(1, ring.overruns());
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_ring_buffer_fifo_order);
  RUN_TEST(test_holds_exactly_n_frames);
  RUN_TEST(test_overrun_flag_on_next_frame);
  return UNITY_END();
}
