#include <unity.h>

#include "frame.h"

void setUp() {}
void tearDown() {}

static void expectSame(const FrameData &a, const FrameData &b)
{
  TEST_ASSERT_EQUAL_UINT16(a.v, b.v);
  TEST_ASSERT_EQUAL_UINT16(a.i, b.i);
  TEST_ASSERT_EQUAL_INT16(a.iOut, b.iOut);
  TEST_ASSERT_EQUAL(a.loadOn, b.loadOn);
  TEST_ASSERT_EQUAL(a.filterOn, b.filterOn);
  TEST_ASSERT_EQUAL(a.cycleStart, b.cycleStart);
  TEST_ASSERT_EQUAL(a.overrun, b.overrun);
}

void test_frame_roundtrip()
{
  const uint16_t vs[]{ 0, 1023, 512 };
  const int16_t outs[]{ -32768, 32767, 0, -1 };
  for (const uint16_t v : vs)
  {
    for (const int16_t o : outs)
    {
      for (uint8_t flags = 0; flags != 16; ++flags)
      {
        FrameData in{};
        in.v = v;
        in.i = static_cast< uint16_t >(1023 - v);
        in.iOut = o;
        in.loadOn = (flags & 1) != 0;
        in.filterOn = (flags & 2) != 0;
        in.cycleStart = (flags & 4) != 0;
        in.overrun = (flags & 8) != 0;
        uint8_t bytes[FRAME_SIZE];
        encodeFrame(bytes, in);
        FrameData out{};
        TEST_ASSERT_TRUE(decodeFrame(bytes, out));
        expectSame(in, out);
      }
    }
  }
}

void test_frame_layout()
{
  FrameData in{};
  in.loadOn = true;
  in.cycleStart = true;
  in.v = 0x0123;
  in.i = 0x0201;
  in.iOut = -2;
  uint8_t b[FRAME_SIZE];
  encodeFrame(b, in);
  const uint8_t expected[FRAME_SIZE]{ 0xA5, 0x05, 0x23, 0x01, 0x01, 0x02, 0xFE, 0xFF };
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, b, FRAME_SIZE);
}

void test_decode_rejects_bad_sync_flags_range()
{
  FrameData in{};
  in.v = 100;
  in.i = 200;
  uint8_t good[FRAME_SIZE];
  encodeFrame(good, in);
  FrameData out{};
  TEST_ASSERT_TRUE(decodeFrame(good, out));

  uint8_t bad[FRAME_SIZE];
  for (uint8_t k = 0; k != FRAME_SIZE; ++k)
  {
    bad[k] = good[k];
  }
  bad[0] = 0x5A;
  TEST_ASSERT_FALSE(decodeFrame(bad, out));

  bad[0] = good[0];
  bad[1] = 0x10;  // a high flag bit
  TEST_ASSERT_FALSE(decodeFrame(bad, out));
  bad[1] = 0x80;
  TEST_ASSERT_FALSE(decodeFrame(bad, out));

  bad[1] = good[1];
  bad[2] = 0x00;  // V = 1024
  bad[3] = 0x04;
  TEST_ASSERT_FALSE(decodeFrame(bad, out));

  bad[2] = good[2];
  bad[3] = good[3];
  bad[4] = 0xFF;  // I = 0xFFFF
  bad[5] = 0xFF;
  TEST_ASSERT_FALSE(decodeFrame(bad, out));
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_frame_roundtrip);
  RUN_TEST(test_frame_layout);
  RUN_TEST(test_decode_rejects_bad_sync_flags_range);
  return UNITY_END();
}
