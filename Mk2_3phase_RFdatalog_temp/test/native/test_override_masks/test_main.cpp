// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file test_main.cpp
 * @brief Native unit tests for override_masks.h: what is forced ON, by the override pins and the
 *        scheduled boosts of the dual tariff
 *
 * @details The pins and boosts are built with the real OverrideTable and BoostSchedule, and the
 *          boosts resolved with DualTariff::resolve(), as dualtariff.h does at compile time. A
 *          pin is active when LOW: here, when its bit is set in a mask of active pins.
 */

#include <unity.h>

#include "override_masks.h"  // Real header - pure C++
#include "utils_override.h"  // OverrideTable

namespace
{
constexpr uint32_t MINUTE{ 60UL * 1000UL };
constexpr uint32_t HOUR{ 60UL * MINUTE };

constexpr uint8_t OFF_PEAK_HOURS{ 8 };
constexpr int16_t THRESHOLD_x100{ 6000 };  // 60 °C

// The same values as constants.h, which needs Arduino.h
constexpr int16_t DEVICE_DISCONNECTED_RAW{ -7040 };
constexpr int16_t OUTOFRANGE_TEMPERATURE{ 30200 };

constexpr uint8_t remote(const uint8_t n)
{
  return REMOTE_PIN_BASE + n;
}

// Override pins: D3 forces D5 and remote load 1, D4 forces D5 and D6, D12 forces remote load 0
constexpr OverrideTable pins{ { { 3, { 5, remote(1) } },
                                { 4, { 5, 6 } },
                                { 12, { remote(0) } } } };

// Boosts, over 8 hours of off-peak:
//  #0 the first 2 h: D5
//  #1 from 1 h, for 2 h (1 h to 3 h): D6 and remote load 2
//  #2 the last 3 h (5 h to the end): D7
constexpr BoostSchedule boosts{ { { FIRST(HOURS(2)), { 5 } },
                                  { FROM_START(HOURS(1)), FOR(HOURS(2)), { 6, remote(2) } },
                                  { LAST(HOURS(3)), { 7 } } } };

constexpr uint16_t bit(const uint8_t n)
{
  return static_cast< uint16_t >(1U << n);
}

/** @brief A callable telling whether a pin is active (LOW): its bit is set in @p activePins. */
auto activeIn(const uint16_t activePins)
{
  return [activePins](const uint8_t pin)
  {
    return (activePins & bit(pin)) != 0;
  };
}

/** @brief Reads the boosts resolved for OFF_PEAK_HOURS, as readBoost() does from flash. */
DualTariff::Boost readBoost(const uint8_t i)
{
  return DualTariff::resolve(boosts, i, OFF_PEAK_HOURS);
}

OverrideMasks pinsOnly(const uint16_t activePins)
{
  return Forcing::pinForcing(pins, activeIn(activePins));
}

OverrideMasks boostsAt(const uint32_t elapsed, const int16_t temperature_x100 = 2000, const bool bOffPeak = true)
{
  return Forcing::boostForcing(bOffPeak, elapsed, temperature_x100, THRESHOLD_x100, boosts.size(), readBoost);
}

void assertMasks(const uint16_t local, const uint8_t remoteMask, const OverrideMasks& masks)
{
  TEST_ASSERT_EQUAL_HEX16(local, masks.local);
  TEST_ASSERT_EQUAL_HEX8(remoteMask, masks.remote);
}
}  // namespace

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Override pins
// ============================================================================

void test_no_active_pin_forces_nothing(void)
{
  assertMasks(0, 0, pinsOnly(0));
}

void test_an_active_pin_forces_its_targets(void)
{
  assertMasks(bit(5) | bit(6), 0, pinsOnly(bit(4)));
}

void test_a_pin_forces_its_remote_targets(void)
{
  assertMasks(0, 0x01, pinsOnly(bit(12)));
  assertMasks(bit(5), 0x02, pinsOnly(bit(3)));
}

void test_only_the_override_pins_count(void)
{
  // D2, D5, D13 active, but none is an override pin
  assertMasks(0, 0, pinsOnly(bit(2) | bit(5) | bit(13)));
}

void test_several_active_pins_add_up(void)
{
  // D3 and D4 share D5
  assertMasks(bit(5) | bit(6), 0x02, pinsOnly(bit(3) | bit(4)));
  assertMasks(bit(5) | bit(6), 0x03, pinsOnly(bit(3) | bit(4) | bit(12)));
}

void test_each_pin_is_read_once(void)
{
  uint16_t readPins{ 0 };
  uint8_t reads{ 0 };
  Forcing::pinForcing(pins, [&](const uint8_t pin)
                      {
                        readPins |= bit(pin);
                        ++reads;
                        return false;
                      });
  TEST_ASSERT_EQUAL_UINT8(3, reads);
  TEST_ASSERT_EQUAL_HEX16(bit(3) | bit(4) | bit(12), readPins);
}

// ============================================================================
// Scheduled boosts: the windows
// ============================================================================

void test_boost_window_opens_at_its_start(void)
{
  // #1 opens at 1 h: just before, only #0
  assertMasks(bit(5), 0, boostsAt(HOUR - 1));
  assertMasks(bit(5) | bit(6), 0x04, boostsAt(HOUR));
}

void test_boost_window_closes_at_its_end(void)
{
  // #0 closes at 2 h, #1 at 3 h
  assertMasks(bit(5) | bit(6), 0x04, boostsAt(2 * HOUR - 1));
  assertMasks(bit(6), 0x04, boostsAt(2 * HOUR));
  assertMasks(bit(6), 0x04, boostsAt(3 * HOUR - 1));
  assertMasks(0, 0, boostsAt(3 * HOUR));
}

void test_first_boost_is_open_from_the_start(void)
{
  assertMasks(bit(5), 0, boostsAt(0));
}

void test_no_boost_between_the_windows(void)
{
  assertMasks(0, 0, boostsAt(4 * HOUR));
  assertMasks(0, 0, boostsAt(5 * HOUR - 1));
}

void test_last_boost_lasts_until_the_meter_ends_off_peak(void)
{
  assertMasks(bit(7), 0, boostsAt(5 * HOUR));
  assertMasks(bit(7), 0, boostsAt(OFF_PEAK_HOURS * HOUR));
  // the meter is late: the boost goes on until it switches to peak
  assertMasks(bit(7), 0, boostsAt(OFF_PEAK_HOURS * HOUR + 30 * MINUTE));
}

void test_boost_across_the_millis_wraparound(void)
{
  // off-peak started 90 min before millis() wrapped around: elapsed = now - start, unsigned
  const uint32_t start{ UINT32_MAX - 90 * MINUTE + 1 };
  const uint32_t now{ 30 * MINUTE };
  assertMasks(bit(6), 0x04, boostsAt(now - start));
}

// ============================================================================
// Scheduled boosts: off-peak and temperature
// ============================================================================

void test_no_boost_outside_off_peak(void)
{
  assertMasks(0, 0, boostsAt(0, 2000, false));
  assertMasks(0, 0, boostsAt(90 * MINUTE, 2000, false));
  assertMasks(0, 0, boostsAt(6 * HOUR, 2000, false));
}

void test_boost_at_the_threshold(void)
{
  assertMasks(bit(5), 0, boostsAt(0, THRESHOLD_x100));
}

void test_no_boost_above_the_threshold(void)
{
  assertMasks(0, 0, boostsAt(0, THRESHOLD_x100 + 1));
  assertMasks(0, 0, boostsAt(90 * MINUTE, THRESHOLD_x100 + 1));
}

void test_boost_below_the_threshold(void)
{
  assertMasks(bit(5), 0, boostsAt(0, THRESHOLD_x100 - 1));
  assertMasks(bit(5), 0, boostsAt(0, -500));  // frost: still a valid reading
}

void test_a_dead_probe_lets_the_boost_run(void)
{
  // DEVICE_DISCONNECTED_RAW (no answer, or a CRC error) counts as cold: the heater's thermostat
  // stays the safety limit
  assertMasks(bit(5) | bit(6), 0x04, boostsAt(90 * MINUTE, DEVICE_DISCONNECTED_RAW));
}

void test_an_out_of_range_reading_stops_the_boost(void)
{
  // OUTOFRANGE_TEMPERATURE (302 °C) is above any threshold
  assertMasks(0, 0, boostsAt(90 * MINUTE, OUTOFRANGE_TEMPERATURE));
}

// ============================================================================
// Scheduled boosts: the table
// ============================================================================

void test_overlapping_boosts_add_up(void)
{
  // 1 h 30: #0 and #1, #1 forces a local and a remote target
  assertMasks(bit(5) | bit(6), 0x04, boostsAt(90 * MINUTE));
}

void test_only_the_boosts_of_the_count_are_read(void)
{
  uint8_t reads{ 0 };
  uint8_t highest{ 0 };
  const auto masks{ Forcing::boostForcing(true, 0, 0, THRESHOLD_x100, 2, [&](const uint8_t i)
                                          {
                                            ++reads;
                                            highest = i;
                                            return readBoost(i);
                                          }) };
  TEST_ASSERT_EQUAL_UINT8(2, reads);
  TEST_ASSERT_EQUAL_UINT8(1, highest);
  assertMasks(bit(5), 0, masks);
}

void test_no_boost_at_all(void)
{
  const auto masks{ Forcing::boostForcing(true, 0, 0, THRESHOLD_x100, 0, [](const uint8_t)
                                          {
                                            TEST_FAIL_MESSAGE("no boost to read");
                                            return DualTariff::Boost{ 0, UINT32_MAX, 0xFFFF, 0xFF };
                                          }) };
  assertMasks(0, 0, masks);
}

void test_the_table_is_not_read_outside_off_peak_or_when_too_hot(void)
{
  const auto fail{ [](const uint8_t)
                   {
                     TEST_FAIL_MESSAGE("no boost to read");
                     return DualTariff::Boost{ 0, UINT32_MAX, 0xFFFF, 0xFF };
                   } };
  assertMasks(0, 0, Forcing::boostForcing(false, 0, 0, THRESHOLD_x100, 3, fail));
  assertMasks(0, 0, Forcing::boostForcing(true, 0, THRESHOLD_x100 + 1, THRESHOLD_x100, 3, fail));
}

// ============================================================================
// Pins and boosts together
// ============================================================================

void test_operator_or_joins_both_masks(void)
{
  assertMasks(bit(2) | bit(9), 0x81, OverrideMasks{ bit(2), 0x01 } | OverrideMasks{ bit(9), 0x80 });
  assertMasks(bit(4), 0x10, OverrideMasks{ bit(4), 0x10 } | OverrideMasks{ bit(4), 0x10 });
}

void test_pins_and_boosts_add_up(void)
{
  // D12 (remote load 0) and, at 1 h 30, boosts #0 and #1
  assertMasks(bit(5) | bit(6), 0x05, pinsOnly(bit(12)) | boostsAt(90 * MINUTE));
  // D4 (D5, D6) and the last boost (D7)
  assertMasks(bit(5) | bit(6) | bit(7), 0, pinsOnly(bit(4)) | boostsAt(6 * HOUR));
}

void test_pins_force_outside_off_peak_and_when_too_hot(void)
{
  assertMasks(bit(5), 0x02, pinsOnly(bit(3)) | boostsAt(90 * MINUTE, 2000, false));
  assertMasks(bit(5), 0x02, pinsOnly(bit(3)) | boostsAt(90 * MINUTE, THRESHOLD_x100 + 1));
}

int main(int /* argc */, char** /* argv */)
{
  UNITY_BEGIN();

  RUN_TEST(test_no_active_pin_forces_nothing);
  RUN_TEST(test_an_active_pin_forces_its_targets);
  RUN_TEST(test_a_pin_forces_its_remote_targets);
  RUN_TEST(test_only_the_override_pins_count);
  RUN_TEST(test_several_active_pins_add_up);
  RUN_TEST(test_each_pin_is_read_once);

  RUN_TEST(test_boost_window_opens_at_its_start);
  RUN_TEST(test_boost_window_closes_at_its_end);
  RUN_TEST(test_first_boost_is_open_from_the_start);
  RUN_TEST(test_no_boost_between_the_windows);
  RUN_TEST(test_last_boost_lasts_until_the_meter_ends_off_peak);
  RUN_TEST(test_boost_across_the_millis_wraparound);

  RUN_TEST(test_no_boost_outside_off_peak);
  RUN_TEST(test_boost_at_the_threshold);
  RUN_TEST(test_no_boost_above_the_threshold);
  RUN_TEST(test_boost_below_the_threshold);
  RUN_TEST(test_a_dead_probe_lets_the_boost_run);
  RUN_TEST(test_an_out_of_range_reading_stops_the_boost);

  RUN_TEST(test_overlapping_boosts_add_up);
  RUN_TEST(test_only_the_boosts_of_the_count_are_read);
  RUN_TEST(test_no_boost_at_all);
  RUN_TEST(test_the_table_is_not_read_outside_off_peak_or_when_too_hot);

  RUN_TEST(test_operator_or_joins_both_masks);
  RUN_TEST(test_pins_and_boosts_add_up);
  RUN_TEST(test_pins_force_outside_off_peak_and_when_too_hot);

  return UNITY_END();
}
