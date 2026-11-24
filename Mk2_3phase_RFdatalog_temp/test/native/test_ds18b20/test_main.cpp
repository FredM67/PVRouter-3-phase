// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file test_main.cpp
 * @brief The checks of a DS18B20 reading (ds18b20.h): CRC, conversion, range, power-on value
 *
 * @details The scratchpads are those of a DS18B20: temperature LSB and MSB, TH = 0x4B, TL = 0x46,
 *          configuration 0x7F (12 bits), 0xFF, 0x0C, 0x10, then the CRC. The CRC of a few of them
 *          is checked against known values first, so the other tests can build theirs with crc8().
 */

#include <unity.h>

#include <stdint.h>

#include "ds18b20.h"

namespace
{
/** A scratchpad with this temperature register (x16), and its CRC. */
struct Pad
{
  DS18B20::ScratchPad buf;

  Pad(uint8_t lsb, uint8_t msb)
    : buf{ lsb, msb, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x00 }
  {
    buf[8] = DS18B20::crc8(buf, 8);
  }

  /** From the register as a signed value: 1/16 °C each. */
  static Pad x16(int16_t raw)
  {
    const auto u{ static_cast< uint16_t >(raw) };
    return Pad{ static_cast< uint8_t >(u & 0xFF), static_cast< uint8_t >(u >> 8) };
  }
};

int16_t decodeX16(int16_t raw)
{
  return DS18B20::decode(Pad::x16(raw).buf);
}
}  // namespace

void setUp() {}
void tearDown() {}

// ----- CRC ------------------------------------------------------------------------------------

void test_crc_of_a_known_rom_code()
{
  // Maxim application note 27: ROM code 02 1C B8 01 00 00 00, CRC A2
  const uint8_t rom[]{ 0x02, 0x1C, 0xB8, 0x01, 0x00, 0x00, 0x00 };
  TEST_ASSERT_EQUAL_HEX8(0xA2, DS18B20::crc8(rom, 7));
}

void test_crc_of_the_power_on_scratchpad()
{
  // DS18B20 datasheet: 50 05 4B 46 7F FF 0C 10, CRC 1C
  const uint8_t pad[]{ 0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10 };
  TEST_ASSERT_EQUAL_HEX8(0x1C, DS18B20::crc8(pad, 8));
}

void test_crc_of_known_scratchpads()
{
  const uint8_t pad25[]{ 0x91, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10 };  // +25.0625 °C
  TEST_ASSERT_EQUAL_HEX8(0x70, DS18B20::crc8(pad25, 8));
  const uint8_t padNeg[]{ 0x5E, 0xFF, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10 };  // -10.125 °C
  TEST_ASSERT_EQUAL_HEX8(0x6A, DS18B20::crc8(padNeg, 8));
}

void test_crc_of_nothing_is_zero()
{
  const uint8_t none[]{ 0xAA };
  TEST_ASSERT_EQUAL_HEX8(0x00, DS18B20::crc8(none, 0));
}

void test_crc_catches_any_single_bit_error()
{
  const Pad good{ 0x91, 0x01 };
  for (uint8_t byte = 0; byte != 9; ++byte)
  {
    for (uint8_t bit = 0; bit != 8; ++bit)
    {
      Pad bad{ good };
      bad.buf[byte] = static_cast< uint8_t >(bad.buf[byte] ^ (1U << bit));
      TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, DS18B20::decode(bad.buf));
    }
  }
}

void test_good_crc_gives_the_temperature()
{
  TEST_ASSERT_EQUAL_INT16(2506, DS18B20::decode(Pad{ 0x91, 0x01 }.buf));
}

void test_bad_crc_is_a_disconnected_probe()
{
  Pad pad{ 0x91, 0x01 };
  pad.buf[8] = static_cast< uint8_t >(pad.buf[8] + 1);
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, DS18B20::decode(pad.buf));
}

// ----- conversion -----------------------------------------------------------------------------

void test_zero()
{
  TEST_ASSERT_EQUAL_INT16(0, DS18B20::toHundredths(0x00, 0x00));
  TEST_ASSERT_EQUAL_INT16(0, decodeX16(0));
}

void test_positive()
{
  TEST_ASSERT_EQUAL_INT16(2506, DS18B20::toHundredths(0x91, 0x01));  // +25.0625 °C (datasheet)
  TEST_ASSERT_EQUAL_INT16(1000, decodeX16(160));                     // +10 °C
  TEST_ASSERT_EQUAL_INT16(5000, decodeX16(800));                     // +50 °C
}

void test_negative()
{
  TEST_ASSERT_EQUAL_INT16(-1013, DS18B20::toHundredths(0x5E, 0xFF));  // -10.125 °C (datasheet): -1012.5 rounded down
  TEST_ASSERT_EQUAL_INT16(-100, decodeX16(-16));                      // -1 °C
  TEST_ASSERT_EQUAL_INT16(-2500, decodeX16(-400));                    // -25 °C (datasheet: FE 70)
  TEST_ASSERT_EQUAL_HEX16(0xFE70, static_cast< uint16_t >(-400));
}

void test_fractions_of_a_degree()
{
  // 1/16 °C = 6.25 hundredths, rounded down (towards -infinity)
  TEST_ASSERT_EQUAL_INT16(6, decodeX16(1));     // +0.0625
  TEST_ASSERT_EQUAL_INT16(12, decodeX16(2));    // +0.125
  TEST_ASSERT_EQUAL_INT16(18, decodeX16(3));    // +0.1875
  TEST_ASSERT_EQUAL_INT16(25, decodeX16(4));    // +0.25
  TEST_ASSERT_EQUAL_INT16(50, decodeX16(8));    // +0.5 (datasheet: 00 08)
  TEST_ASSERT_EQUAL_INT16(93, decodeX16(15));   // +0.9375
  TEST_ASSERT_EQUAL_INT16(-7, decodeX16(-1));   // -0.0625
  TEST_ASSERT_EQUAL_INT16(-13, decodeX16(-2));  // -0.125
  TEST_ASSERT_EQUAL_INT16(-50, decodeX16(-8));  // -0.5 (datasheet: FF F8)
}

void test_every_register_value_of_the_range()
{
  // the exact value is raw x 6.25: the result is it rounded down
  for (int16_t raw = -879; raw != 2000; ++raw)
  {
    const int32_t x10000{ static_cast< int32_t >(raw) * 625 };  // °C x 10000
    int32_t expected{ x10000 / 100 };
    if (x10000 % 100 < 0)
    {
      --expected;
    }
    TEST_ASSERT_EQUAL_INT16(expected, decodeX16(raw));
  }
}

// ----- range: -55 to +125 °C ------------------------------------------------------------------

void test_just_inside_the_range()
{
  TEST_ASSERT_EQUAL_INT16(12493, decodeX16(1999));  // +124.9375 °C
  TEST_ASSERT_EQUAL_INT16(-5494, decodeX16(-879));  // -54.9375 °C
}

void test_the_bounds_of_the_sensor_are_in_range()
{
  // -55 and +125 °C are the bounds of the DS18B20: readings like any other
  TEST_ASSERT_EQUAL_INT16(12500, DS18B20::toHundredths(0xD0, 0x07));  // +125 °C (datasheet: 07 D0)
  TEST_ASSERT_EQUAL_INT16(-5500, DS18B20::toHundredths(0x90, 0xFC));  // -55 °C (datasheet: FC 90)
  TEST_ASSERT_EQUAL_INT16(12500, decodeX16(2000));
  TEST_ASSERT_EQUAL_INT16(-5500, decodeX16(-880));
  TEST_ASSERT_TRUE(isValidTemperature(12500));
  TEST_ASSERT_TRUE(isValidTemperature(-5500));
}

void test_beyond_the_range()
{
  TEST_ASSERT_EQUAL_INT16(OUTOFRANGE_TEMPERATURE, decodeX16(2001));
  TEST_ASSERT_EQUAL_INT16(OUTOFRANGE_TEMPERATURE, decodeX16(-881));
  TEST_ASSERT_EQUAL_INT16(OUTOFRANGE_TEMPERATURE, decodeX16(0x0FFF));
  TEST_ASSERT_EQUAL_INT16(OUTOFRANGE_TEMPERATURE, decodeX16(-0x0FFF));
}

// ----- disconnected probe ---------------------------------------------------------------------

void test_all_ones_is_a_disconnected_probe()
{
  // no device drives the bus: the pull-up reads 0xFF, whose CRC does not match
  DS18B20::ScratchPad pad{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  TEST_ASSERT_NOT_EQUAL(0xFF, DS18B20::crc8(pad, 8));
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, DS18B20::decode(pad));
}

void test_all_zeros_passes_the_crc()
{
  // NOTE: the CRC of 8 zeros is 0: an all-zero scratchpad reads 0.00 °C. A bus held low fails
  // its reset() before (utils_temp.h), so it never gets this far. Pinned here.
  DS18B20::ScratchPad pad{};
  TEST_ASSERT_EQUAL_INT16(0, DS18B20::decode(pad));
}

// ----- 85 °C, the power-on value --------------------------------------------------------------

void test_power_on_scratchpad_reads_85()
{
  const DS18B20::ScratchPad pad{ 0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x1C };
  TEST_ASSERT_EQUAL_INT16(DS18B20::POWER_ON_VALUE, DS18B20::decode(pad));
  TEST_ASSERT_EQUAL_INT16(8500, DS18B20::POWER_ON_VALUE);
}

void test_85_after_a_big_jump_is_skipped()
{
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, DS18B20::skipPowerOnValue(8500, 2000));
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, DS18B20::skipPowerOnValue(8500, 0));  // first reading
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, DS18B20::skipPowerOnValue(8500, 7999));
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, DS18B20::skipPowerOnValue(8500, 9001));  // from above
}

void test_85_after_a_small_jump_is_kept()
{
  TEST_ASSERT_EQUAL_INT16(8500, DS18B20::skipPowerOnValue(8500, 8500));
  TEST_ASSERT_EQUAL_INT16(8500, DS18B20::skipPowerOnValue(8500, 8000));  // exactly 5.00 °C
  TEST_ASSERT_EQUAL_INT16(8500, DS18B20::skipPowerOnValue(8500, 9000));
  TEST_ASSERT_EQUAL_INT16(8500, DS18B20::skipPowerOnValue(8500, 8494));
}

void test_85_after_an_invalid_reading_is_skipped()
{
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, DS18B20::skipPowerOnValue(8500, DEVICE_DISCONNECTED_RAW));
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, DS18B20::skipPowerOnValue(8500, OUTOFRANGE_TEMPERATURE));
}

void test_other_readings_are_kept_whatever_the_jump()
{
  TEST_ASSERT_EQUAL_INT16(8506, DS18B20::skipPowerOnValue(8506, 0));  // 85.0625 °C
  TEST_ASSERT_EQUAL_INT16(8493, DS18B20::skipPowerOnValue(8493, 0));  // 84.9375 °C
  TEST_ASSERT_EQUAL_INT16(-2000, DS18B20::skipPowerOnValue(-2000, 8500));
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, DS18B20::skipPowerOnValue(DEVICE_DISCONNECTED_RAW, 2000));
  TEST_ASSERT_EQUAL_INT16(OUTOFRANGE_TEMPERATURE, DS18B20::skipPowerOnValue(OUTOFRANGE_TEMPERATURE, 2000));
}

// ----- the power-on value, reading after reading ---------------------------------------------

void test_a_first_reading_of_85_is_skipped()
{
  DS18B20::PowerOnFilter filter;
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, filter(8500));
}

void test_a_steady_85_after_an_invalid_reading_is_kept()
{
  // a CRC error while the water sits at 85 °C: the next 85.00 °C are real
  DS18B20::PowerOnFilter filter;
  TEST_ASSERT_EQUAL_INT16(8450, filter(8450));
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, filter(DEVICE_DISCONNECTED_RAW));
  TEST_ASSERT_EQUAL_INT16(8500, filter(8500));
  TEST_ASSERT_EQUAL_INT16(8500, filter(8500));
}

void test_85_after_an_invalid_reading_far_from_the_last_valid_one_is_skipped()
{
  // the sensor restarted (a glitch on its supply): 85.00 °C is its power-on value
  DS18B20::PowerOnFilter filter;
  TEST_ASSERT_EQUAL_INT16(2000, filter(2000));
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, filter(DEVICE_DISCONNECTED_RAW));
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, filter(8500));
  TEST_ASSERT_EQUAL_INT16(2010, filter(2010));
}

void test_a_skipped_or_out_of_range_value_is_no_reference()
{
  DS18B20::PowerOnFilter filter;
  TEST_ASSERT_EQUAL_INT16(2000, filter(2000));
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, filter(8500));
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, filter(8500));  // still 20 °C away
  TEST_ASSERT_EQUAL_INT16(OUTOFRANGE_TEMPERATURE, filter(OUTOFRANGE_TEMPERATURE));
  TEST_ASSERT_EQUAL_INT16(DEVICE_DISCONNECTED_RAW, filter(8500));
  TEST_ASSERT_EQUAL_INT16(8200, filter(8200));
  TEST_ASSERT_EQUAL_INT16(8500, filter(8500));  // 3 °C from the last valid reading
}

// ----- what is sent ---------------------------------------------------------------------------

void test_invalid_readings_are_not_sent()
{
  TEST_ASSERT_FALSE(isValidTemperature(DEVICE_DISCONNECTED_RAW));
  TEST_ASSERT_FALSE(isValidTemperature(OUTOFRANGE_TEMPERATURE));
  TEST_ASSERT_EQUAL_INT16(-7040, DEVICE_DISCONNECTED_RAW);
  TEST_ASSERT_EQUAL_INT16(30200, OUTOFRANGE_TEMPERATURE);
}

void test_valid_readings_are_sent()
{
  TEST_ASSERT_TRUE(isValidTemperature(0));
  TEST_ASSERT_TRUE(isValidTemperature(2506));
  TEST_ASSERT_TRUE(isValidTemperature(-5494));
  TEST_ASSERT_TRUE(isValidTemperature(12493));
  TEST_ASSERT_TRUE(isValidTemperature(8500));
}

void test_the_invalid_values_cannot_be_temperatures()
{
  // the two markers lie outside the range: no reading of the sensor can be taken for them
  TEST_ASSERT_TRUE(DEVICE_DISCONNECTED_RAW < TEMP_RANGE_LOW);
  TEST_ASSERT_TRUE(OUTOFRANGE_TEMPERATURE > TEMP_RANGE_HIGH);
}

void test_a_skipped_power_on_value_is_not_sent()
{
  const DS18B20::ScratchPad pad{ 0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x1C };
  TEST_ASSERT_FALSE(isValidTemperature(DS18B20::skipPowerOnValue(DS18B20::decode(pad), 2000)));
}

// constexpr: the checks can be used at compile time
static_assert(DS18B20::toHundredths(0x91, 0x01) == 2506);
static_assert(DS18B20::skipPowerOnValue(8500, 0) == DEVICE_DISCONNECTED_RAW);
static_assert(!isValidTemperature(OUTOFRANGE_TEMPERATURE));

int main(int, char **)
{
  UNITY_BEGIN();

  RUN_TEST(test_crc_of_a_known_rom_code);
  RUN_TEST(test_crc_of_the_power_on_scratchpad);
  RUN_TEST(test_crc_of_known_scratchpads);
  RUN_TEST(test_crc_of_nothing_is_zero);
  RUN_TEST(test_crc_catches_any_single_bit_error);
  RUN_TEST(test_good_crc_gives_the_temperature);
  RUN_TEST(test_bad_crc_is_a_disconnected_probe);

  RUN_TEST(test_zero);
  RUN_TEST(test_positive);
  RUN_TEST(test_negative);
  RUN_TEST(test_fractions_of_a_degree);
  RUN_TEST(test_every_register_value_of_the_range);

  RUN_TEST(test_just_inside_the_range);
  RUN_TEST(test_the_bounds_of_the_sensor_are_in_range);
  RUN_TEST(test_beyond_the_range);

  RUN_TEST(test_all_ones_is_a_disconnected_probe);
  RUN_TEST(test_all_zeros_passes_the_crc);

  RUN_TEST(test_power_on_scratchpad_reads_85);
  RUN_TEST(test_85_after_a_big_jump_is_skipped);
  RUN_TEST(test_85_after_a_small_jump_is_kept);
  RUN_TEST(test_85_after_an_invalid_reading_is_skipped);
  RUN_TEST(test_other_readings_are_kept_whatever_the_jump);

  RUN_TEST(test_invalid_readings_are_not_sent);
  RUN_TEST(test_valid_readings_are_sent);
  RUN_TEST(test_the_invalid_values_cannot_be_temperatures);
  RUN_TEST(test_a_skipped_power_on_value_is_not_sent);
  RUN_TEST(test_a_first_reading_of_85_is_skipped);
  RUN_TEST(test_a_steady_85_after_an_invalid_reading_is_kept);
  RUN_TEST(test_85_after_an_invalid_reading_far_from_the_last_valid_one_is_skipped);
  RUN_TEST(test_a_skipped_or_out_of_range_value_is_no_reference);

  return UNITY_END();
}
